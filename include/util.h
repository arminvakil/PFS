/*
 * util.h
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#ifndef INCLUDE_UTIL_H_
#define INCLUDE_UTIL_H_

#include <iostream>
#include "cstdint"
#include "config.h"

extern const uint64_t pfsBlockSizeInBytes;

#define NO_ERROR 0
#define ERROR_ALREADY_EXISTS -1
#define ERROR_DOES_NOT_EXIST -2
#define ERROR_CLOSE_UNOPENED_FILE -3
#define ERROR_DELETE_OPEN_FILE -4
#define ERROR_REOPEN_FILE -5
#define ERROR_GET_PERMISSION_UNOPENED_FILE -6

std::string getErrorMessage(int error_code);

#endif /* INCLUDE_UTIL_H_ */
