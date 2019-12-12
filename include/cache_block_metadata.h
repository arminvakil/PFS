#ifndef CLIENT_CACHE_BLOCK_METADATA_H_
#define CLIENT_CACHE_BLOCK_METADATA_H_

#include "cache_block.h"
#include <pthread.h>

class ListNode {
public:
	ListNode() { resetNode(); }
	ListNode* prev, *next;

	void resetNode() { prev = next = nullptr; }
};

class CacheBlockMetadata : public ListNode {
public:
	uint32_t bucketId;
	pthread_mutex_t lock;
	CacheBlock* block;
	ListNode* dirtyNode;
	ListNode* recencyNode;
	ListNode* freeNode;
	ListNode* bucketNode;
	CacheBlockMetadata();
	virtual ~CacheBlockMetadata();

	void reset();
};

#endif /* CLIENT_CACHE_BLOCK_METADATA_H_ */
