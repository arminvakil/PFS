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
	while(true) {
		pthread_mutex_lock(&cacheLock);
		CacheBlockMetadata* node = removeDirtyListLRU(false);
		if(node == nullptr) {
			pthread_mutex_unlock(&cacheLock);
			break;
		}
		// do the real flush for node
		assert(!(node->block->clean));
		PFSClient::getInstance()->flush(node->block);
		pthread_mutex_unlock(&cacheLock);
		delete node;
	}

	while(true) {
		pthread_mutex_lock(&cacheLock);
		CacheBlockMetadata* node = removeRecencyListLRU(false);
		if(node == nullptr) {
			pthread_mutex_unlock(&cacheLock);
			break;
		}
		pthread_mutex_unlock(&cacheLock);
		delete node;
	}

	pthread_mutex_lock(&freeListLock);
	while(freeListHead != nullptr) {
		CacheBlockMetadata* node = (CacheBlockMetadata*) freeListHead->freeNode->next;
		delete freeListHead;
		freeListHead = node;
	}
	pthread_mutex_unlock(&freeListLock);

	delete hashTable;
	delete hashTableBucketLocks;
	for(int i = 0; i < blockCount; i++)
		delete data[i];
	delete data;
}

bool Cache::readBlock(int fileDes, uint32_t blockAddr, char* data) {
	pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* node = lookup(fileDes, blockAddr, false);
	if(node == nullptr) {
		pthread_mutex_unlock(&cacheLock);
		return false;
	}
	memcpy(data, node->block->data, pfsBlockSizeInBytes);
	removeFromRecencyList(node, false);
	addToRecencyList(node, false);
	pthread_mutex_unlock(&cacheLock);
	return true;
}

bool Cache::write(int fileDes, uint32_t offset, uint32_t size, const char* data) {
	pthread_mutex_lock(&cacheLock);
	uint32_t blockAddr = offset / pfsBlockSizeInBytes;
	blockAddr *= pfsBlockSizeInBytes;
	CacheBlockMetadata* node = lookup(fileDes, blockAddr, false);
	if(node == nullptr) {
		pthread_mutex_unlock(&cacheLock);
		return false;
	}
	uint32_t blockOffset = offset % pfsBlockSizeInBytes;
	memcpy(node->block->data + blockOffset, data, size);
	for(int i = 0; i < size; i++)
		node->block->dirty[i + offset] = true;
	if(node->block->clean)
		removeFromRecencyList(node, false);
	else
		removeFromDirtyList(node, false);
	node->block->clean = false;
	addToDirtyList(node, false);
	pthread_mutex_unlock(&cacheLock);
	return true;
}

void Cache::addBlock(int fileDes, uint32_t blockAddr, const char* data) {
	assert((blockAddr & (pfsBlockSizeInBytes - 1)) == 0);
	sem_wait(&freeBlockSemaphore);

	CacheBlockMetadata* node = getFreeListEntry();

	node->block->reset();
	node->block->valid = true;
	node->block->addr = blockAddr;
	node->block->fdes = fileDes;
	memcpy(node->block->data, data, pfsBlockSizeInBytes);
	uint32_t bucket = hashPFS(fileDes, blockAddr);
	node->bucketId = bucket;
	pthread_mutex_lock(&cacheLock);
	addToHashmap(node, false);
	addToRecencyList(node, false);
	pthread_mutex_unlock(&cacheLock);
}

void Cache::evictBlock(int fileDes, uint32_t blockAddr) {
	pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* node = lookup(fileDes, blockAddr, false);
	if(node == nullptr) {
		pthread_mutex_unlock(&cacheLock);
		return;
	}
	removeFromHashmap(node, false);
	if(node->block->clean)
		removeFromRecencyList(node, false);
	else
		removeFromDirtyList(node, false);
	node->reset();
	pthread_mutex_unlock(&cacheLock);
	addToFreeList(node);
	return;
}

CacheBlockMetadata* Cache::lookup(int fileDes, uint32_t blockAddr, bool shouldAcquireLock) {
	uint32_t bucket = hashPFS(fileDes, blockAddr);
	if(shouldAcquireLock)
		pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* bucketPrev = nullptr;
	CacheBlockMetadata* node = hashTable[bucket];
	while(node != nullptr) {
		if(node->block->fdes == fileDes && node->block->addr == blockAddr) {
			if(shouldAcquireLock)
				pthread_mutex_unlock(&cacheLock);
			return node;
		}
		node = (CacheBlockMetadata*)node->bucketNode->next;
	}
	if(shouldAcquireLock)
		pthread_mutex_unlock(&cacheLock);
	return nullptr;
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
			pthread_mutex_lock(&(cache->cacheLock));
			CacheBlockMetadata* node = cache->removeRecencyListLRU(false);
			if(node == nullptr) {
				node = cache->removeDirtyListLRU(false);
				assert(!(node->block->clean));
				assert(node->block->valid);
			}
			// Removing this node from hashmap table
			cache->removeFromHashmap(node, false);
			// Now, This node is not in any list.
			if(!(node->block->clean)) {
				PFSClient::getInstance()->flush(node->block);
			}
			pthread_mutex_unlock(&(cache->cacheLock));
			cont = cache->addToFreeList(node);
		}
	}
}

void* cacheFlusher(void* data) {
	Cache* cache = Cache::getInstance();
	while(true) {
		sleep(30);
		while(true) {
			pthread_mutex_lock(&(cache->cacheLock));
			CacheBlockMetadata* node = cache->removeDirtyListLRU(false);
			if(node == nullptr) {
				pthread_mutex_unlock(&(cache->cacheLock));
				break;
			}
			// do the real flush for node
			assert(!(node->block->clean));
			PFSClient::getInstance()->flush(node->block);
			// adding to the clean list
			cache->addToRecencyList(node, false);
			pthread_mutex_unlock(&(cache->cacheLock));
		}
	}
	return nullptr;
}

void Cache::addToRecencyList(CacheBlockMetadata* node, bool shouldAcquireLock) {
	if(shouldAcquireLock)
		pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* nodeNext = MRUNode;
	MRUNode = node;
	if(LRUNode == nullptr)
		LRUNode = node;
	node->recencyNode->next = nodeNext;
	if(nodeNext) {
		nodeNext->recencyNode->prev = node;
	}
	if(shouldAcquireLock)
		pthread_mutex_unlock(&cacheLock);
}

void Cache::removeFromRecencyList(CacheBlockMetadata* node, bool shouldAcquireLock) {
	if(shouldAcquireLock)
		pthread_mutex_lock(&cacheLock);
	if(node == MRUNode && node == LRUNode) {
		MRUNode = LRUNode = nullptr;
		if(shouldAcquireLock)
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
	if(shouldAcquireLock)
		pthread_mutex_unlock(&cacheLock);
}

CacheBlockMetadata* Cache::removeRecencyListLRU(bool shouldAcquireLock) {
	if(shouldAcquireLock)
		pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* node = LRUNode;
	if(node == nullptr) {
		if(shouldAcquireLock)
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
		node->recencyNode->resetNode();
	}
	if(shouldAcquireLock)
		pthread_mutex_unlock(&cacheLock);
	return node;
}

void Cache::addToDirtyList(CacheBlockMetadata* node, bool shouldAcquireLock) {
	if(shouldAcquireLock)
		pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* nodeNext = dirtyListMRU;
	dirtyListMRU = node;
	if(dirtyListLRU == nullptr)
		dirtyListLRU = node;
	node->dirtyNode->next = nodeNext;
	if(nodeNext) {
		nodeNext->dirtyNode->prev = node;
	}
	if(shouldAcquireLock)
		pthread_mutex_unlock(&cacheLock);
}

void Cache::removeFromDirtyList(CacheBlockMetadata* node, bool shouldAcquireLock) {
	if(shouldAcquireLock)
		pthread_mutex_lock(&cacheLock);
	if(node == dirtyListMRU && node == dirtyListLRU) {
		dirtyListMRU = dirtyListLRU = nullptr;
		if(shouldAcquireLock)
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
	if(shouldAcquireLock)
		pthread_mutex_unlock(&cacheLock);
}

CacheBlockMetadata* Cache::removeDirtyListLRU(bool shouldAcquireLock) {
	if(shouldAcquireLock)
		pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* node = dirtyListLRU;
	if(node == nullptr) {
		if(shouldAcquireLock)
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
	if(shouldAcquireLock)
		pthread_mutex_unlock(&cacheLock);
	return node;
}

void Cache::addToHashmap(CacheBlockMetadata* node, bool shouldAcquireLock) {
	if(shouldAcquireLock)
		pthread_mutex_lock(&cacheLock);
	CacheBlockMetadata* nodeNext = hashTable[node->bucketId];
	hashTable[node->bucketId] = node;
	node->bucketNode->next = nodeNext;
	if(nodeNext) {
		nodeNext->bucketNode->prev = node;
	}
	if(shouldAcquireLock)
		pthread_mutex_unlock(&cacheLock);
}

void Cache::removeFromHashmap(CacheBlockMetadata* node, bool shouldAcquireLock) {
	if(shouldAcquireLock)
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
	if(shouldAcquireLock)
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
	freeBlockCount++;
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
