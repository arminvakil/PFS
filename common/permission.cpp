/*
 * Permission.cpp
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#include "../include/permission.h"

Permission::~Permission() {
	// TODO Auto-generated destructor stub
}

bool Permission::isShared(uint32_t s, uint32_t e) {
	if(e <= this->start)
		return false;
	if(s >= this->end)
		return false;
	return true;
}

bool Permission::isInclusiveShared(uint32_t s, uint32_t e) {
	if(e < this->start)
		return false;
	if(s > this->end)
		return false;
	return true;
}

void Permission::lock() {
	pthread_mutex_lock(&permitLock);
}

void Permission::unlock() {
	pthread_mutex_unlock(&permitLock);
	for(int i = 0; i < waitingSemaphores.size(); i++)
		sem_post(waitingSemaphores[i]);
}
