#ifndef CLIENT_CACHE_BLOCK_H_
#define CLIENT_CACHE_BLOCK_H_

#include "util.h"

class CacheBlock {
public:
	uint32_t fdes;
	uint32_t addr;
	bool dirty[pfsBlockSizeInBytes];
	bool clean;
	bool valid;
	uint8_t data[pfsBlockSizeInBytes];
	CacheBlock();
	virtual ~CacheBlock();

	void reset();

	bool isDirty();
};

#endif /* CLIENT_CACHE_BLOCK_H_ */
