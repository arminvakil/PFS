/*
 * util.h
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#include <iostream>
#include "util.h"

std::string getErrorMessage(int error_code) {
	if(error_code >= 0)
		return "";
	switch(error_code) {
	case ERROR_ALREADY_EXISTS:
		return "Error : Already Exists";
	case ERROR_DOES_NOT_EXIST:
		return "Error : Does not exist";
	case ERROR_CLOSE_UNOPENED_FILE:
		return "Error : Closing an unopened file";
	case ERROR_DELETE_OPEN_FILE:
		return "Error : Deleting an opened file";
	case ERROR_REOPEN_FILE:
		return "Error : Reopening a file";
	case ERROR_GET_PERMISSION_UNOPENED_FILE:
		return "Error : Getting r/w permission for an unopened file";
	}
	return "Error : Unknown Error";
}
