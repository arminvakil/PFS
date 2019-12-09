/*
 * file.h
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#ifndef COMMON_FILE_H_
#define COMMON_FILE_H_

#include <iostream>
#include <cstring>
#include <stdint.h>
#include <vector>
#include "config.h"
#include "permission.h"

class File {
	std::string name;
	pfs_stat stat;
	int stripWidth;
public:
	std::vector<Permission*> permissions;
	pthread_mutex_t lock;

	File();
	virtual ~File();

	time_t getCreationTime() const {
		return stat.pst_ctime;
	}

	void setCreationTime(time_t creationTime) {
		this->stat.pst_ctime = creationTime;
	}

	time_t getLastModifiedTime() const {
		return stat.pst_mtime;
	}

	void setLastModifiedTime(time_t lastModifiedTime) {
		this->stat.pst_mtime = lastModifiedTime;
	}

	const std::string& getName() const {
		return name;
	}

	void setName(const std::string& name) {
		this->name = name;
	}

	uint32_t getSize() const {
		return stat.pst_size;
	}

	void setSize(uint32_t size) {
		this->stat.pst_size = size;
	}

	int getStripWidth() const {
		return stripWidth;
	}

	void setStripWidth(int stripWidth) {
		this->stripWidth = stripWidth;
	}

	bool hasPermission(uint32_t start, uint32_t end, bool write,
			pthread_mutex_t* lock, std::vector<Permission*> &permissions);

	void addPermission(uint32_t start, uint32_t end, bool write,
			pthread_mutex_t* lock, std::vector<Permission*> &permissions,
			bool lockNewPermissions);

	void revokePermission(uint32_t start, uint32_t end, bool write,
			pthread_mutex_t* lock, std::vector<Permission*> &permissions);

	void printPermissions(std::vector<Permission*> &permissions);

};

#endif /* COMMON_FILE_H_ */
