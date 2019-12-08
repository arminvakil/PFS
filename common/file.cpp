/*
 * file.cpp
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#include <cstdio>
#include "file.h"
#include <sstream>

File::File() {
	// TODO Auto-generated constructor stub
}

File::~File() {
	// TODO Auto-generated destructor stub
}

bool File::hasPermission(uint32_t start, uint32_t end, bool write,
		pthread_mutex_t* lock, std::vector<Permission*> &permissions) {
	if(lock)
		pthread_mutex_lock(lock);
	if(permissions.empty()) {
		if(lock)
			pthread_mutex_unlock(lock);
		return false;
	}
	int s = permissions.size() - 1, e = permissions.size() - 1;
	for(int i = 0; i < permissions.size(); i++)
		if(permissions[i]->getStart() > start) {
			if(i == 0) {
				if(lock)
					pthread_mutex_unlock(lock);
				return false;
			}
			s = i - 1;
			break;
		}
	for(int i = s; i < permissions.size(); i++) {
		if(permissions[i]->getEnd() >= end) {
			e = i;
			break;
		}
	}
	if(permissions[s]->getStart() > start) {
		if(lock)
			pthread_mutex_unlock(lock);
		return false;
	}
	if(permissions[e]->getEnd() < end) {
		if(lock)
			pthread_mutex_unlock(lock);
		return false;
	}
	if(write) {
		for(int i = s; i <= e; i++)
			if(!permissions[i]->isWrite()) {
				if(lock)
					pthread_mutex_unlock(lock);
				return false;
			}
	}
	for(int i = s; i < e; i++)
		if(permissions[i]->getEnd() != permissions[i+1]->getStart()) {
			if(lock)
				pthread_mutex_unlock(lock);
			return false;
		}
	if(lock)
		pthread_mutex_unlock(lock);
	return true;
}

void File::addPermission(uint32_t start, uint32_t end, bool write,
		pthread_mutex_t* lock, std::vector<Permission*> &permissions,
		bool lockNewPermissions) {
	std::cerr << "adding Permission : " << start << " " << end << "\n";
	if(lock)
		pthread_mutex_lock(lock);
	int s = -1, e = -1;
	for(int i = 0; i < permissions.size(); i++) {
		if(permissions[i]->isInclusiveShared(start, end)) {
			s = i;
			break;
		}
	}
	if(s != -1) {
		e = s;
		for(int i = s + 1; i < permissions.size(); i++) {
			if(!permissions[i]->isInclusiveShared(start, end)) {
				e = i - 1;
				break;
			}
		}
		if(write) {
			Permission* sPermit = nullptr;
			Permission* ePermit = nullptr;
			int newStart = start;
			int newEnd = end;
			if(!permissions[s]->isWrite() && permissions[s]->getStart() < start)
				sPermit = new Permission(permissions[s]->getStart(), start, false);
			else if(permissions[s]->isWrite())
				newStart = std::min(permissions[s]->getStart(), start);

			if(!permissions[e]->isWrite() && permissions[e]->getEnd() > end)
				ePermit = new Permission(end, permissions[s]->getEnd(), false);
			else if(permissions[e]->isWrite())
				newEnd = std::max(permissions[e]->getEnd(), end);

			for(int i = s; i <= e; i++) {
				delete permissions[s];
				permissions.erase(permissions.begin() + s);
			}

			if(ePermit)
				permissions.insert(permissions.begin() + s, ePermit);
			Permission* permit = new Permission(newStart, newEnd, write);
			if(lockNewPermissions)
				permit->lock();
			permissions.insert(permissions.begin() + s, permit);
			if(sPermit)
				permissions.insert(permissions.begin() + s, sPermit);
			if(lock)
				pthread_mutex_unlock(lock);
			return;
		}
		else {
			int newStart = start;
			int newEnd = end;
			if(!permissions[s]->isWrite() && permissions[s]->getStart() < start)
				newStart = permissions[s]->getStart();
			if(!permissions[e]->isWrite() && permissions[e]->getEnd() > end)
				newEnd = permissions[e]->getEnd();
			std::vector<Permission*> writePermits;
			for(int i = s; i <= e; i++) {
				if(permissions[i]->isWrite())
					writePermits.push_back(permissions[i]);
				else {
					delete permissions[i];
				}
			}
			permissions.erase(permissions.begin() + s, permissions.begin() + e + 1);
			if(writePermits.empty()) {
				Permission* permit = new Permission(newStart, newEnd, false);
				if(lockNewPermissions)
					permit->lock();
				permissions.insert(permissions.begin() + s, permit);
				if(lock)
					pthread_mutex_unlock(lock);
				return;
			}
			else {
				if(writePermits[writePermits.size() - 1]->getStart() <= newEnd
						&& newEnd <= writePermits[writePermits.size() - 1]->getEnd()) {
					newEnd = writePermits[writePermits.size() - 1]->getStart();
					if(lockNewPermissions)
						writePermits[writePermits.size() - 1]->lock();
					permissions.insert(permissions.begin() + s,
							writePermits[writePermits.size() - 1]);
					writePermits.pop_back();
				}
				while(!writePermits.empty()) {
					Permission* permit = new Permission(
							writePermits[writePermits.size() - 1]->getEnd(),
							newEnd, false);
					if(lockNewPermissions)
						permit->lock();
					permissions.insert(permissions.begin() + s, permit);
					newEnd = writePermits[writePermits.size() - 1]->getStart();
					if(lockNewPermissions)
						writePermits[writePermits.size() - 1]->lock();
					permissions.insert(permissions.begin() + s,
							writePermits[writePermits.size() - 1]);
					writePermits.pop_back();
				}
				if(newStart < newEnd) {
					Permission* permit = new Permission(newStart, newEnd, false);
					if(lockNewPermissions)
						permit->lock();
					permissions.insert(permissions.begin() + s, permit);
				}
				if(lock)
					pthread_mutex_unlock(lock);
				return;
			}
		}
	}
	else {
		// No sharing
		Permission* permit = new Permission(start, end, write);
		if(lockNewPermissions)
			permit->lock();
		if(permissions.empty()) {
			permissions.push_back(permit);
			if(lock)
				pthread_mutex_unlock(lock);
			return;
		}
		for(int i = permissions.size() - 1; i >= 0; i--) {
			if(permissions[i]->getEnd() < start) {
				permissions.insert(permissions.begin() + i + 1, permit);
				if(lock)
					pthread_mutex_unlock(lock);
				return;
			}
		}
		permissions.insert(permissions.begin(), permit);
		if(lock)
			pthread_mutex_unlock(lock);
		return;
	}
}

void File::revokePermission(uint32_t start, uint32_t end, bool write,
		pthread_mutex_t* lock, std::vector<Permission*> &permissions) {
	if(lock) {
		pthread_mutex_lock(lock);
		sem_t sem;
		sem_init(&sem, 0, 0);
		int count = 0;
		for(int i = 0; i < permissions.size(); i++)
			if(permissions[i]->isShared(start, end)) {
				permissions[i]->addWaitingSemaphore(&sem);
				count++;
			}
		pthread_mutex_unlock(lock);
		for(int i = 0; i < count; i++)
			sem_wait(&sem);
		pthread_mutex_lock(lock);
	}
	int s = -1, e = -1;
	for(int i = 0; i < permissions.size(); i++) {
		if(permissions[i]->isShared(start, end)) {
			s = i;
			break;
		}
	}
	for(int i = permissions.size() - 1; i >= 0; i--) {
		if(permissions[i]->isShared(start, end)) {
			e = i;
			break;
		}
	}

	if(s != -1 && s == e) {
		Permission* sPermit = nullptr;
		Permission* ePermit = nullptr;
		if(permissions[s]->getStart() < start) {
			sPermit = new Permission(permissions[s]->getStart(),
					start, permissions[s]->isWrite());
		}
		if(end < permissions[e]->getEnd()) {
			ePermit = new Permission(end, permissions[e]->getEnd(),
					permissions[e]->isWrite());
		}
		permissions.erase(permissions.begin() + s);
		if(ePermit)
			permissions.insert(permissions.begin() + s, ePermit);
		if(sPermit)
			permissions.insert(permissions.begin() + s, sPermit);
		if(lock)
			pthread_mutex_unlock(lock);
		return;
	}
	if(s != -1 && permissions[s]->getStart() < start) {
		permissions[s]->setEnd(start);
		s++;
	}
	if(e != -1 && permissions[e]->getEnd() > end) {
		permissions[e]->setStart(end);
		e--;
	}
	if(s != -1 && s <= e)
		permissions.erase(permissions.begin() + s, permissions.begin() + e + 1);
	if(lock)
		pthread_mutex_unlock(lock);
}

void File::printPermissions(std::vector<Permission*> &permissions) {
	std::stringstream ss;
	ss << getName() << "\t";
	for(auto permit : permissions)
		ss << "(" << permit->getStart() << ", " << permit->getEnd() << ", " <<
			(permit->isWrite() ? 'w' : 'r') << ")\t";
	ss << "\n";
	printf("%s", ss.str().c_str());
}
