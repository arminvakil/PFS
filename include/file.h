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

class File {
	std::string name;
	uint32_t size;
	time_t creationTime;
	time_t lastModifiedTime;
public:
	File();
	virtual ~File();

	time_t getCreationTime() const {
		return creationTime;
	}

	void setCreationTime(time_t creationTime) {
		this->creationTime = creationTime;
	}

	time_t getLastModifiedTime() const {
		return lastModifiedTime;
	}

	void setLastModifiedTime(time_t lastModifiedTime) {
		this->lastModifiedTime = lastModifiedTime;
	}

	const std::string& getName() const {
		return name;
	}

	void setName(const std::string& name) {
		this->name = name;
	}

	uint32_t getSize() const {
		return size;
	}

	void setSize(uint32_t size) {
		this->size = size;
	}
};

#endif /* COMMON_FILE_H_ */
