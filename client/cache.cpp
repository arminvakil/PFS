#include <cassert>
#include <cstring>
#include "cache.h"
#include "pfs_client.h"

Cache* Cache::instance_;

Cache::Cache() {
	blockCount = clientCacheSizeInBytes / pfsBlockSizeInBytes;
	data = new CacheBlock*[blockCount];

	pthread_mutex_init(&cacheLock, nullptr);

	LRUNode = MRUNode = dirtyListLRU = dirtyListMRU = nullptr;
	pthread_mutex_init(&recencyListLock, nullptr);
	pthread_mutex_init(&dirtyListLock, nullptr);

	freeListHead = nullptr;
	freeBlockCount = blockCount;
	pthread_mutex_init(&freeListLock, nullptr);
	for(int i = 0; i < blockCount; i++) {
		data[i] = new CacheBlock;
		CacheBlockMetadata* node = new CacheBlockMetadata();
		node->block = data[i];
		node->freeNode->next = freeListHead;
		if(freeListHead)
			freeListHead->freeNode->prev = node;
		freeListHead = node;
	}
	sem_init(&freeBlockSemaphore, 0, blockCount);
	sem_init(&harvesterSemaphore, 0, 0);

	hashTable = new CacheBlockMetadata*[HASH_BUCKETS];
	hashTableBucketLocks = new pthread_mutex_t[HASH_BUCKETS];
	for(int i = 0; i < HASH_BUCKETS; i++) {
		hashTable[i] = nullptr;
		pthread_mutex_init(hashTableBucketLocks + i, nullptr);
	}

	pthread_create(&harvesterThread, nullptr, &cacheHarvester, nullptr);
	pthread_create(&flusherThread, nullptr, &cacheFlusher, nullptr);
}

Cache::~Cache() {
	pthread_mutex_lock(&freeListLock);
	while(freeListHead != nullptr) {
		CacheBlockMetadata* node = (CacheBlockMetadata*) freeListHead->freeNode->next;
		delete freeListHead;
		freeListHead = node;
	}
	pthread_mutex_unlock(&freeListLock);
	for(int i = 0; i < HASH_BUCKETS; i++) {
		pthread_mutex_lock(hashTableBucketLocks + i);
		while(hashTable[i] != nullptr) {
			CacheBlockMetadata* node = (CacheBlockMetadata*)hashTable[i]->bucketNode->next;
			delete hashTable[i];
			hashTable[i] = node;
		}
		pthread_mutex_unlock(hashTableBucketLocks + i);
	}
}

bool Cache::readBlock(int fileDes, uint32_t blockAddr, char* data) {
	return false;
}

bool Cache::write(int fileDes, uint32_t offset, uint32_t size, const char* data) {
	return false;
}

void Cache::addBlock(int fileDes, uint32_t blockAddr, const char* data) {
	assert((blockAddr & (pfsBlockSizeInBytes - 1)) == 0);
	sem_wait(&freeBlockSemaphore);
	pthread_mutex_lock(&freeListLock);
	CacheBlockMetadata* node = freeListHead;
	pthread_mutex_lock(&(node->lock));
	assert(node != nullptr);
	CacheBlockMetadata* nx = (CacheBlockMetadata*)(node->freeNode->next);
	nx->freeNode->prev = nullptr;
	freeListHead = nx;

	if(freeBlockCount == blockCount) {
		pthread_mutex_lock(&recencyListLock);
		LRUNode = node;
		pthread_mutex_unlock(&recencyListLock);
	}
	freeBlockCount--;
	if(freeBlockCount <= 50)
		sem_post(&harvesterSemaphore);
	pthread_mutex_unlock(&freeListLock);

	node->block->reset();
	node->block->valid = true;
	node->block->addr = blockAddr;
	node->block->fdes = fileDes;
	memcpy(node->block->data, data, pfsBlockSizeInBytes);
	uint32_t bucket = hashPFS(fileDes, blockAddr);
	node->bucketId = bucket;
	pthread_mutex_lock(hashTableBucketLocks + bucket);
	if(hashTable[bucket])
		pthread_mutex_lock(&(hashTable[bucket]->lock));
	node->bucketNode->next = hashTable[bucket];
	if(hashTable[bucket]) {
		hashTable[bucket]->bucketNode->prev = node;
		pthread_mutex_unlock(&(hashTable[bucket]->lock));
	}
	hashTable[bucket] = node;
	pthread_mutex_unlock(hashTableBucketLocks + bucket);

	pthread_mutex_lock(&recencyListLock);
	if(MRUNode) {
		pthread_mutex_lock(&(MRUNode->lock));
	}
	node->recencyNode->next = MRUNode;
	if(MRUNode) {
		MRUNode->recencyNode->prev = node;
		pthread_mutex_unlock(&(MRUNode->lock));
	}
	MRUNode = node;
	pthread_mutex_unlock(&recencyListLock);
	pthread_mutex_unlock(&(node->lock));
}

void Cache::evictBlock(int fileDes, uint32_t blockAddr) {
	uint32_t bucket = hashPFS(fileDes, blockAddr);
	pthread_mutex_lock(hashTableBucketLocks + bucket);
	CacheBlockMetadata* bucketPrev = nullptr;
	CacheBlockMetadata* node = hashTable[bucket];
	pthread_mutex_lock(&(node->lock));
	CacheBlockMetadata* bucketNext = (CacheBlockMetadata*)hashTable[bucket]->bucketNode->next;
	if(bucketNext == nullptr) {
		if(!(node->block->fdes == fileDes && node->block->addr == blockAddr)) {
			pthread_mutex_unlock(&(node->lock));
			pthread_mutex_unlock(hashTableBucketLocks + bucket);
			return;
		}
	}
	else {
		pthread_mutex_lock(&(bucketNext->lock));
	}
	while(node != nullptr) {
		// if we are here, we have lock of all prev, node, and next, if exist
		if(node->block->fdes == fileDes && node->block->addr == blockAddr) {
			if(bucketPrev) {
				bucketPrev->bucketNode->next = bucketNext;
			}
			if(bucketNext) {
				bucketNext->bucketNode->prev = bucketPrev;
			}
			node->bucketNode->resetNode();
			if(bucketNext)
				pthread_mutex_unlock(&(bucketNext->lock));
			if(bucketPrev)
				pthread_mutex_unlock(&(bucketPrev->lock));
			break;
		}
		if(bucketPrev)
			pthread_mutex_unlock(&(bucketPrev->lock));
		bucketPrev = node;
		node = bucketNext;
		bucketNext = (CacheBlockMetadata*)bucketNext->bucketNode->next;
		if(bucketNext)
			pthread_mutex_lock(&(bucketNext->lock));
	}
	// Here, we only have lock for node and we have found the node
	if(node != nullptr) {
		// should evict the node
		if(node->block->clean) {
			pthread_mutex_lock(&recencyListLock);
			assert(MRUNode != nullptr);
			assert(LRUNode != nullptr);
			if(MRUNode == LRUNode) {
				assert(MRUNode == node);
				MRUNode = LRUNode = nullptr;
				node->reset();
			}
			else {
				CacheBlockMetadata* recencyPrev = (CacheBlockMetadata*)node->recencyNode->prev;
				CacheBlockMetadata* recencyNext = (CacheBlockMetadata*)node->recencyNode->next;
				if(recencyPrev != nullptr)
					pthread_mutex_lock(&(recencyPrev->lock));
				if(recencyNext != nullptr)
					pthread_mutex_lock(&(recencyNext->lock));
				if(recencyPrev)
					recencyPrev->recencyNode->next = recencyNext;
				if(recencyNext)
					recencyNext->recencyNode->prev = recencyPrev;
				if(MRUNode == node)
					MRUNode = recencyNext;
				else if(LRUNode == node)
					LRUNode = recencyPrev;
				if(recencyNext != nullptr)
					pthread_mutex_unlock(&(recencyNext->lock));
				if(recencyPrev != nullptr)
					pthread_mutex_unlock(&(recencyPrev->lock));
			}
			pthread_mutex_unlock(&recencyListLock);
		}
		else {
			// block is dirty
			pthread_mutex_lock(&dirtyListLock);
			assert(dirtyListMRU != nullptr);
			assert(dirtyListLRU != nullptr);
			if(dirtyListMRU == dirtyListLRU) {
				assert(dirtyListMRU == node);
				dirtyListMRU = dirtyListLRU = nullptr;
				node->reset();
			}
			else {
				CacheBlockMetadata* dirtyPrev = (CacheBlockMetadata*)node->dirtyNode->prev;
				CacheBlockMetadata* dirtyNext = (CacheBlockMetadata*)node->dirtyNode->next;
				if(dirtyPrev != nullptr)
					pthread_mutex_lock(&(dirtyPrev->lock));
				if(dirtyNext != nullptr)
					pthread_mutex_lock(&(dirtyNext->lock));
				if(dirtyPrev)
					dirtyPrev->dirtyNode->next = dirtyNext;
				if(dirtyNext)
					dirtyNext->dirtyNode->prev = dirtyPrev;
				if(dirtyListMRU == node)
					dirtyListMRU = dirtyNext;
				else if(dirtyListLRU == node)
					dirtyListLRU = dirtyPrev;
				if(dirtyNext != nullptr)
					pthread_mutex_unlock(&(dirtyNext->lock));
				if(dirtyPrev != nullptr)
					pthread_mutex_unlock(&(dirtyPrev->lock));
			}
			pthread_mutex_unlock(&dirtyListLock);
		}
	}
	pthread_mutex_unlock(hashTableBucketLocks + bucket);
	if(!(node->block->clean))
		PFSClient::getInstance()->flush(node->block);
	// should add the node to the free list
	pthread_mutex_lock(&freeListLock);
	if(freeListHead != nullptr) {
		pthread_mutex_lock(&(freeListHead->lock));
		freeListHead->freeNode->prev = node;
		node->freeNode->next = freeListHead;
		pthread_mutex_unlock(&(freeListHead->lock));
	}
	else {
		node->freeNode->resetNode();
	}
	freeListHead = node;
	freeBlockCount++;
	pthread_mutex_unlock(&freeListLock);
}

void* cacheHarvester(void* data) {
	Cache* cache = Cache::getInstance();
	while(true) {
		sem_wait(&(cache->harvesterSemaphore));
		bool cont;
		pthread_mutex_lock(&(cache->freeListLock));
		cont = (cache->freeBlockCount < 100);
		pthread_mutex_unlock(&(cache->freeListLock));
		while(cont) {
			pthread_mutex_lock(&(cache->recencyListLock));
			CacheBlockMetadata* node = cache->LRUNode;
			if(node == nullptr) {
				pthread_mutex_unlock(&(cache->recencyListLock));
				pthread_mutex_lock(&(cache->dirtyListLock));
				assert(cache->dirtyListLRU != nullptr);
				node = cache->dirtyListLRU;
				pthread_mutex_lock(&(node->lock));
				assert(!(node->block->clean));
				assert(node->block->valid);
				CacheBlockMetadata* dirtyPrev = (CacheBlockMetadata*)node->dirtyNode->prev;
				assert(dirtyPrev != nullptr);
				CacheBlockMetadata* dirtyNext = (CacheBlockMetadata*)node->dirtyNode->next;
				assert(dirtyNext == nullptr);
				pthread_mutex_lock(&(dirtyPrev->lock));
				cache->dirtyListLRU = dirtyPrev;
				pthread_mutex_unlock(&(cache->dirtyListLock));
				dirtyPrev->next = dirtyNext;
				pthread_mutex_unlock(&(dirtyPrev->lock));
				node->dirtyNode->resetNode();
				// Removing this node from hashmap table
				uint32_t bucket = node->bucketId;
				pthread_mutex_lock(cache->hashTableBucketLocks + bucket);
				CacheBlockMetadata* bucketPrev = (CacheBlockMetadata*)cache->hashTable[bucket]->bucketNode->prev;
				CacheBlockMetadata* bucketNext = (CacheBlockMetadata*)cache->hashTable[bucket]->bucketNode->next;
				if(bucketPrev == nullptr && bucketNext == nullptr) {
					assert(cache->hashTable[bucket] == node);
					cache->hashTable[bucket] = nullptr;
					pthread_mutex_unlock(cache->hashTableBucketLocks + bucket);
				}
				else {
					if(bucketPrev)
						pthread_mutex_lock(&(bucketPrev->lock));
					if(bucketNext)
						pthread_mutex_lock(&(bucketNext->lock));
					pthread_mutex_unlock(cache->hashTableBucketLocks + bucket);
					if(bucketPrev) {
						bucketPrev->next = bucketNext;
					}
					if(bucketNext) {
						bucketNext->prev = bucketPrev;
					}
					if(bucketNext)
						pthread_mutex_unlock(&(bucketNext->lock));
					if(bucketPrev)
						pthread_mutex_unlock(&(bucketPrev->lock));
					node->bucketNode->resetNode();
				}
				// Now, This node is not in any list.
				pthread_mutex_unlock(&(node->lock));
			}
			else {
				pthread_mutex_lock(&(node->lock));
				assert(node->block->clean);
				assert(node->block->valid);
				if(cache->LRUNode == cache->MRUNode) {
					node->recencyNode->resetNode();
					cache->LRUNode = cache->MRUNode = nullptr;
					pthread_mutex_unlock(&(cache->recencyListLock));
				}
				else {
					CacheBlockMetadata* recencyPrev = (CacheBlockMetadata*)node->recencyNode->prev;
					assert(recencyPrev != nullptr);
					CacheBlockMetadata* recencyNext = (CacheBlockMetadata*)node->recencyNode->next;
					assert(recencyNext == nullptr);
					pthread_mutex_lock(&(recencyPrev->lock));
					cache->LRUNode = recencyPrev;
					pthread_mutex_unlock(&(cache->recencyListLock));
					recencyPrev->recencyNode->next = recencyNext;
					pthread_mutex_unlock(&(recencyPrev->lock));
					node->recencyNode->resetNode();
				}
				// Removing this node from hashmap table
				uint32_t bucket = node->bucketId;
				pthread_mutex_lock(cache->hashTableBucketLocks + bucket);
				CacheBlockMetadata* bucketPrev = (CacheBlockMetadata*)cache->hashTable[bucket]->bucketNode->prev;
				CacheBlockMetadata* bucketNext = (CacheBlockMetadata*)cache->hashTable[bucket]->bucketNode->next;
				if(bucketPrev == nullptr && bucketNext == nullptr) {
					assert(cache->hashTable[bucket] == node);
					cache->hashTable[bucket] = nullptr;
					pthread_mutex_unlock(cache->hashTableBucketLocks + bucket);
				}
				else {
					if(bucketPrev)
						pthread_mutex_lock(&(bucketPrev->lock));
					if(bucketNext)
						pthread_mutex_lock(&(bucketNext->lock));
					pthread_mutex_unlock(cache->hashTableBucketLocks + bucket);
					if(bucketPrev) {
						bucketPrev->next = bucketNext;
					}
					if(bucketNext) {
						bucketNext->prev = bucketPrev;
					}
					if(bucketNext)
						pthread_mutex_unlock(&(bucketNext->lock));
					if(bucketPrev)
						pthread_mutex_unlock(&(bucketPrev->lock));
					node->bucketNode->resetNode();
				}
				// Now, This node is not in any list.
				pthread_mutex_unlock(&(node->lock));
			}
			if(!(node->block->clean)) {
				PFSClient::getInstance()->flush(node->block);
			}
			pthread_mutex_lock(&(cache->freeListLock));
			if(cache->freeListHead != nullptr) {
				pthread_mutex_lock(&(cache->freeListHead->lock));
				cache->freeListHead->freeNode->prev = node;
				node->freeNode->next = cache->freeListHead;
				pthread_mutex_unlock(&(cache->freeListHead->lock));
			}
			else {
				node->freeNode->resetNode();
			}
			cache->freeListHead = node;
			cache->freeBlockCount++;
			if(cache->freeBlockCount >= 100)
				cont = false;
			pthread_mutex_unlock(&(cache->freeListLock));
		}
	}
}

void* cacheFlusher(void* data) {
	Cache* cache = Cache::getInstance();
	while(true) {
		sleep(30);
		while(true) {
			pthread_mutex_lock(&(cache->dirtyListLock));
			CacheBlockMetadata* node = cache->dirtyListLRU;
			if(node == nullptr) {
				pthread_mutex_unlock(&(cache->dirtyListLock));
				break;
			}
			pthread_mutex_lock(&(node->lock));
			if(node == cache->dirtyListMRU) {
				cache->dirtyListLRU = cache->dirtyListMRU = nullptr;
				pthread_mutex_unlock(&(cache->dirtyListLock));
			}
			else {
				CacheBlockMetadata* dirtyNext = (CacheBlockMetadata*)node->dirtyNode->next;
				CacheBlockMetadata* dirtyPrev = (CacheBlockMetadata*)node->dirtyNode->prev;
				assert(dirtyNext == nullptr);
				assert(dirtyPrev != nullptr);
				pthread_mutex_lock(&(dirtyPrev->lock));
				cache->dirtyListLRU = dirtyPrev;
				pthread_mutex_unlock(&(cache->dirtyListLock));
				dirtyPrev->next = dirtyNext;
				pthread_mutex_unlock(&(dirtyPrev->lock));
				node->dirtyNode->resetNode();
			}
			// do the real flush for node
			assert(!(node->block->clean));
			PFSClient::getInstance()->flush(node->block);
			// adding to the clean list
			pthread_mutex_lock(&(cache->recencyListLock));
			if(cache->MRUNode) {
				pthread_mutex_lock(&(cache->MRUNode->lock));
			}
			node->recencyNode->next = cache->MRUNode;
			if(cache->MRUNode) {
				cache->MRUNode->recencyNode->prev = node;
				pthread_mutex_unlock(&(cache->MRUNode->lock));
			}
			cache->MRUNode = node;
			pthread_mutex_unlock(&(cache->recencyListLock));
			pthread_mutex_unlock(&(node->lock));
		}
	}
	return nullptr;
}

void Cache::addToRecencyList(CacheBlockMetadata* node) {
	pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* nodeNext = MRUNode;
	MRUNode = node;
	if(LRUNode == nullptr)
		LRUNode = node;
	node->recencyNode->next = nodeNext;
	if(nodeNext) {
		nodeNext->recencyNode->prev = node;
	}
	pthread_mutex_unlock(&cacheLock);
}

void Cache::removeFromRecencyList(CacheBlockMetadata* node) {
	pthread_mutex_lock(&cacheLock);
	if(node == MRUNode && node == LRUNode) {
		MRUNode = LRUNode = nullptr;
		pthread_mutex_unlock(&cacheLock);
		return;
	}
	CacheBlockMetadata* recencyPrev = (CacheBlockMetadata*)node->recencyNode->prev;
	CacheBlockMetadata* recencyNext = (CacheBlockMetadata*)node->recencyNode->next;
	if(node == LRUNode)
		LRUNode = recencyPrev;
	if(recencyPrev)
		recencyPrev->recencyNode->next = recencyNext;
	if(recencyNext)
		recencyNext->recencyNode->prev = recencyPrev;
	node->recencyNode->resetNode();
	pthread_mutex_unlock(&cacheLock);
}

CacheBlockMetadata* Cache::removeRecencyListLRU() {
	pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* node = LRUNode;
	if(node == nullptr) {
		pthread_mutex_unlock(&cacheLock);
		return node;
	}
	if(LRUNode == MRUNode) {
		node->recencyNode->resetNode();
		LRUNode = MRUNode = nullptr;
	}
	else {
		CacheBlockMetadata* recencyPrev = (CacheBlockMetadata*)node->recencyNode->prev;
		assert(recencyPrev != nullptr);
		CacheBlockMetadata* recencyNext = (CacheBlockMetadata*)node->recencyNode->next;
		assert(recencyNext == nullptr);
		LRUNode = recencyPrev;
		recencyPrev->recencyNode->next = recencyNext;
		pthread_mutex_unlock(&(recencyPrev->lock));
		node->recencyNode->resetNode();
	}
	pthread_mutex_unlock(&cacheLock);
	return node;
}

void Cache::addToDirtyList(CacheBlockMetadata* node) {
	pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* nodeNext = dirtyListMRU;
	dirtyListMRU = node;
	if(dirtyListLRU == nullptr)
		dirtyListLRU = node;
	node->dirtyNode->next = nodeNext;
	if(nodeNext) {
		nodeNext->dirtyNode->prev = node;
	}
	pthread_mutex_unlock(&cacheLock);
}

void Cache::removeFromDirtyList(CacheBlockMetadata* node) {
	pthread_mutex_lock(&cacheLock);
	if(node == dirtyListMRU && node == dirtyListLRU) {
		dirtyListMRU = dirtyListLRU = nullptr;
		pthread_mutex_unlock(&cacheLock);
		return;
	}
	CacheBlockMetadata* dirtyPrev = (CacheBlockMetadata*)node->dirtyNode->prev;
	CacheBlockMetadata* dirtyNext = (CacheBlockMetadata*)node->dirtyNode->next;
	if(node == dirtyListLRU)
		dirtyListLRU = dirtyPrev;
	if(dirtyPrev)
		dirtyPrev->dirtyNode->next = dirtyNext;
	if(dirtyNext)
		dirtyNext->dirtyNode->prev = dirtyPrev;
	node->dirtyNode->resetNode();
	pthread_mutex_unlock(&cacheLock);
}

CacheBlockMetadata* Cache::removeDirtyListLRU() {
	pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* node = dirtyListLRU;
	if(node == nullptr) {
		pthread_mutex_unlock(&cacheLock);
		return node;
	}
	if(dirtyListLRU == dirtyListMRU) {
		node->dirtyNode->resetNode();
		dirtyListLRU = dirtyListMRU = nullptr;
	}
	else {
		CacheBlockMetadata* dirtyPrev = (CacheBlockMetadata*)node->dirtyNode->prev;
		assert(dirtyPrev != nullptr);
		CacheBlockMetadata* dirtyNext = (CacheBlockMetadata*)node->dirtyNode->next;
		assert(dirtyNext == nullptr);
		dirtyListLRU = dirtyPrev;
		dirtyPrev->dirtyNode->next = dirtyNext;
		node->dirtyNode->resetNode();
	}
	pthread_mutex_unlock(&cacheLock);
	return node;
}

void Cache::addToHashmap(CacheBlockMetadata* node) {
	pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* nodeNext = hashTable[node->bucketId];
	hashTable[node->bucketId] = node;
	node->bucketNode->next = nodeNext;
	if(nodeNext) {
		nodeNext->bucketNode->prev = node;
	}
	pthread_mutex_unlock(&cacheLock);
}

void Cache::removeFromHashmap(CacheBlockMetadata* node) {
	pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* bucketPrev = (CacheBlockMetadata*)node->bucketNode->prev;
	CacheBlockMetadata* bucketNext = (CacheBlockMetadata*)node->bucketNode->next;
	if(node == hashTable[node->bucketId])
		hashTable[node->bucketId] = bucketNext;
	if(bucketPrev)
		bucketPrev->bucketNode->next = bucketNext;
	if(bucketNext)
		bucketNext->bucketNode->prev = bucketPrev;
	node->bucketNode->resetNode();
	pthread_mutex_unlock(&cacheLock);
}

bool Cache::addToFreeList(CacheBlockMetadata* node) {
	pthread_mutex_lock(&freeListLock);
	CacheBlockMetadata* nodeNext = freeListHead;
	freeListHead = node;
	node->freeNode->next = nodeNext;
	if(nodeNext) {
		nodeNext->freeNode->prev = node;
	}
	bool res = freeBlockCount < 100;
	pthread_mutex_unlock(&freeListLock);
	return res;
}

CacheBlockMetadata* Cache::getFreeListEntry() {
	sem_wait(&freeBlockSemaphore);
	pthread_mutex_lock(&freeListLock);
	CacheBlockMetadata* node = freeListHead;
	assert(node != nullptr);
	CacheBlockMetadata* nx = (CacheBlockMetadata*)(node->freeNode->next);
	nx->freeNode->prev = nullptr;
	freeListHead = nx;

	freeBlockCount--;
	if(freeBlockCount <= 50)
		sem_post(&harvesterSemaphore);
	pthread_mutex_unlock(&freeListLock);
	return node;
}
