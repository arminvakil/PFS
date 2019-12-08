/*
 * mm_file.h
 *
 *  Created on: Nov 29, 2019
 *      Author: armin
 */

#ifndef METADATA_MANAGER_MM_FILE_H_
#define METADATA_MANAGER_MM_FILE_H_

#include "file.h"
#include "permission.h"
#include <unordered_map>
#include <pthread.h>
#include <semaphore.h>
#include <vector>

class MMFile : public File {
	typedef std::pair<std::string, std::vector<Permission*>*> PSVP;
	std::unordered_map<std::string, std::vector<Permission*>*> clientPermissions;
	pthread_mutex_t lock;
public:
	MMFile();
	virtual ~MMFile();

	bool open(std::string peer);
	bool close(std::string peer);

	bool isOpen();
	bool isOpenedBy(std::string peer);

	Permission* waitAndGetPermissionFor(std::string peer,
			uint32_t start, uint32_t end, bool iswrite);
};

#endif /* METADATA_MANAGER_MM_FILE_H_ */
