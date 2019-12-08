/*
 * mm_fle.cpp
 *
 *  Created on: Nov 29, 2019
 *      Author: armin
 */

#include "mm_file.h"
#include "metadata_manager.h"

#include <climits>
#include <algorithm>

MMFile::MMFile() {
	// TODO Auto-generated constructor stub
	pthread_mutex_init(&lock, nullptr);
}

MMFile::~MMFile() {
	// TODO Auto-generated destructor stub
}

bool MMFile::open(std::string peer) {
	pthread_mutex_lock(&lock);
	auto it = clientPermissions.find(peer);
	if(it != clientPermissions.end()) {
		pthread_mutex_unlock(&lock);
		return false;
	}
	std::vector<Permission*>* permits = new std::vector<Permission*>;
	clientPermissions.insert(PSVP(peer, permits));
	pthread_mutex_unlock(&lock);
	return true;
}

bool MMFile::close(std::string peer) {
	pthread_mutex_lock(&lock);
	auto it = clientPermissions.find(peer);
	if(it == clientPermissions.end()) {
		pthread_mutex_unlock(&lock);
		return false;
	}
	std::vector<Permission*>* permits = it->second;
	for(auto p : *permits)
		delete p;
	delete permits;
	clientPermissions.erase(it);
	pthread_mutex_unlock(&lock);
	return true;
}

bool MMFile::isOpen() {
	bool result;
	pthread_mutex_lock(&lock);
	if(clientPermissions.empty()) {
		result = false;
	}
	else {
		result = true;
	}
	pthread_mutex_unlock(&lock);
	return result;
}

bool MMFile::isOpenedBy(std::string peer) {
	bool result;
	pthread_mutex_lock(&lock);
	if(clientPermissions.find(peer) == clientPermissions.end()) {
		result = false;
	}
	else {
		result = true;
	}
	pthread_mutex_unlock(&lock);
	return result;
}

Permission* MMFile::waitAndGetPermissionFor(std::string peer,
		uint32_t start, uint32_t end, bool iswrite) {
	pthread_mutex_lock(&lock);
	std::cerr << __func__ << " " << peer << " " << start << " " << end
			<< " " << iswrite << "\n";
	if(clientPermissions.empty()) {
		Permission* permit = new Permission(0, INT_MAX, iswrite);
		clientPermissions[peer]->push_back(permit);
		pthread_mutex_unlock(&lock);
		return permit;
	}
	else {
		std::vector<pthread_t*> threads;
		bool firstPermit = true;
		for(auto strToPermits : clientPermissions) {
			for(auto permit : *(strToPermits.second)) {
				firstPermit = false;
				if((permit->isWrite() || iswrite) && permit->isShared(start, end)) {
					this->revokePermission(start, end, iswrite,
							nullptr, *(strToPermits.second));
					this->printPermissions(*(strToPermits.second));
					pthread_t* th = MetadataManagerServiceImpl::sendRevokePermissionRequest(
							strToPermits.first, this->getName(), start, end, iswrite);
					threads.push_back(th);
					break;
				}
			}
		}
		Permission* permit;
		if(!threads.empty() || !firstPermit)
			permit = new Permission(start, end, iswrite);
		else
			permit = new Permission(0, INT_MAX, iswrite);
		this->addPermission(permit->getStart(), permit->getEnd(), iswrite,
				nullptr, *(clientPermissions[peer]), false);
		for(int i = 0; i < threads.size(); i++) {
			pthread_join(*threads[i], nullptr);
			delete threads[i];
		}
		pthread_mutex_unlock(&lock);
		return permit;
	}
	return nullptr;
}
