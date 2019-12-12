#ifndef CLIENT_CACHE_H_
#define CLIENT_CACHE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>
#include <semaphore.h>
#include <string>
#include <vector>

#include "cache_block.h"
#include "cache_block_metadata.h"
#include "config.h"
#include "util.h"

class Cache {
public:
	CacheBlock** data;
	int blockCount;

	CacheBlockMetadata* freeListHead;
	sem_t freeBlockSemaphore, harvesterSemaphore;
	uint32_t freeBlockCount;

	pthread_mutex_t recencyListLock;
	CacheBlockMetadata* LRUNode;
	CacheBlockMetadata* MRUNode;

	pthread_mutex_t dirtyListLock;
	CacheBlockMetadata* dirtyListLRU;
	CacheBlockMetadata* dirtyListMRU;

	CacheBlockMetadata** hashTable;
	pthread_mutex_t* hashTableBucketLocks;

	pthread_t harvesterThread;
	pthread_t flusherThread;

	static Cache* instance_;
	Cache();
	virtual ~Cache();

	static Cache* getInstance() {
		if(instance_ == nullptr)
			return (instance_ = new Cache());
		return instance_;
	}

	bool readBlock(int fileDes, uint32_t blockAddr, char* data);
	bool write(int fileDes, uint32_t offset, uint32_t size, const char* data);

	void addBlock(int fileDes, uint32_t blockAddr, const char* data);
	void evictBlock(int fileDes, uint32_t blockAddr);

	void addToRecencyList(CacheBlockMetadata* node);
	void removeFromRecencyList(CacheBlockMetadata* node);
	CacheBlockMetadata* removeRecencyListLRU();
	void addToDirtyList(CacheBlockMetadata* node);
	void removeFromDirtyList(CacheBlockMetadata* node);
	CacheBlockMetadata* removeDirtyListLRU();
	void addToHashmap(CacheBlockMetadata* node);
	void removeFromHashmap(CacheBlockMetadata* node);
	bool addToFreeList(CacheBlockMetadata* node);
	CacheBlockMetadata* getFreeListEntry();

	pthread_mutex_t cacheLock;
	pthread_mutex_t freeListLock;
};

void* cacheHarvester(void* data);
void* cacheFlusher(void* data);

#endif /* CLIENT_CACHE_H_ */
