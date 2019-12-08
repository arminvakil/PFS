/*
 * ClientFile.h
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#ifndef COMMON_CLIENTFILE_H_
#define COMMON_CLIENTFILE_H_

#include <iostream>
#include <vector>

#include "config.h"
#include "file.h"
#include "permission.h"
#include "pfs.h"

class ClientFile : public File {
	int descriptor;
	static int descriptorCount;
public:
	ClientFile();
	virtual ~ClientFile();

	int getDescriptor() {
		return this->descriptor;
	}

	// give the permission to client and guarantee it will not revoke until it unlocks it
	void unlockPermission(uint32_t start, uint32_t end, bool write);
};

#endif /* COMMON_CLIENTFILE_H_ */
