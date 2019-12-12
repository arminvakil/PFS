/*
 * util.h
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#ifndef INCLUDE_UTIL_H_
#define INCLUDE_UTIL_H_

#include <iostream>
#include <functional>
#include "cstdint"
#include "config.h"

#define pfsBlockSizeInBytes (PFS_BLOCK_SIZE * 1024)
#define clientCacheSizeInBytes (CLIENT_CACHE_SIZE * 1024 * 1024)

#define HASH_BUCKETS (clientCacheSizeInBytes / pfsBlockSizeInBytes) / 2

#define NO_ERROR 0
#define ERROR_ALREADY_EXISTS -1
#define ERROR_DOES_NOT_EXIST -2
#define ERROR_CLOSE_UNOPENED_FILE -3
#define ERROR_DELETE_OPEN_FILE -4
#define ERROR_REOPEN_FILE -5
#define ERROR_GET_PERMISSION_UNOPENED_FILE -6

std::string getErrorMessage(int error_code);

std::size_t hashPFS(uint32_t fdes, uint32_t blockAddr);

#endif /* INCLUDE_UTIL_H_ */
