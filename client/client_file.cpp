/*
 * client.cpp
 *
 *  Created on: Nov 26, 2019
 *      Author: armin
 */

#include <cassert>
#include <algorithm>
#include <iostream>
#include <client_file.h>

int ClientFile::descriptorCount = 0;

ClientFile::ClientFile() {
	pthread_mutex_init(&lock, nullptr);
	descriptor = ++descriptorCount;
	std::cerr << "opening file with descriptor : " << descriptor << "\n";
}

ClientFile::~ClientFile() {
	for(int i = 0; i < permissions.size(); i++)
		delete permissions[i];
	permissions.clear();
}

void ClientFile::unlockPermission(uint32_t start, uint32_t end, bool write) {
	std::cerr << "unlockPermission : " << start << " " << end << "\n";
	pthread_mutex_lock(&lock);
	for(int i = 0; i < permissions.size(); i++)
		if(permissions[i]->isShared(start, end)) {
			std::cerr << i << " " << permissions[i]->getStart() << " "
					<< permissions[i]->getEnd() << "\n";
			permissions[i]->unlock();
		}
	pthread_mutex_unlock(&lock);
	std::cerr << "unlockPermission done\n";
}
