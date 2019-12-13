#include "cache_block.h"

#include <cstring>

CacheBlock::CacheBlock() {
	reset();
}

CacheBlock::~CacheBlock() {
	// TODO Auto-generated destructor stub
}

void CacheBlock::reset() {
	valid = false;
	memset(dirty, 0, pfsBlockSizeInBytes);
	clean = true;
}

bool CacheBlock::isDirty() {
	return !clean;
}

void CacheBlock::resetDirty() {
	memset(dirty, 0, pfsBlockSizeInBytes);
	clean = true;
}
