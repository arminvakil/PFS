#include "cache_block_metadata.h"

CacheBlockMetadata::CacheBlockMetadata() : bucketId(-1) {
	block = nullptr;
	pthread_mutex_init(&lock, nullptr);
	dirtyNode = new ListNode;
	recencyNode = new ListNode;
	freeNode = new ListNode;
	bucketNode = new ListNode;
}

CacheBlockMetadata::~CacheBlockMetadata() {
}

void CacheBlockMetadata::reset() {
	bucketId = -1;
	dirtyNode->resetNode();
	recencyNode->resetNode();
	freeNode->resetNode();
	bucketNode->resetNode();
}
