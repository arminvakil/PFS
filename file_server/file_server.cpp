/*
 * file_server.cpp
 *
 *  Created on: Dec 8, 2019
 *      Author: armin
 */

#include "file_server.h"
#include <iostream>
#include <string>

FileServerServiceImpl::FileServerServiceImpl(std::string address) : address(address) {
	filesDirectory = "file_server_";
	filesDirectory.append(address);
	filesDirectory.append("_files");
	std::string removeCommand("rm -r ");
	std::string mkdirCommand("mkdir -p ");
	removeCommand.append(filesDirectory);
	mkdirCommand.append(filesDirectory);
	filesDirectory.append("/");
	system(removeCommand.c_str());
	system(mkdirCommand.c_str());
	pthread_mutex_init(&lock, nullptr);
}

FileServerServiceImpl::~FileServerServiceImpl() {
}

Status FileServerServiceImpl::CreateFile(ServerContext* context,
		const ParallelFileSystem::CreateStripeRequest* request,
		ParallelFileSystem::StatusReply* reply) {
	pthread_mutex_lock(&lock);
	auto it = files.find(request->name());
	FSFile* file = nullptr;
	if(it == files.end()) {
		file = new FSFile(request->name(), request->stripewidth());
		files[request->name()] = file;
	}
	else
		file = it->second;
	pthread_mutex_unlock(&lock);

	std::string stripeAddress = filesDirectory;
	stripeAddress.append(request->name());
	stripeAddress.push_back('_');
	stripeAddress.append(std::to_string(request->id()));
	file->addStrip(stripeAddress, request->id());
	std::cerr << "Create file request for " << request->name() <<
			" in " << stripeAddress << "\n";
	return Status::OK;
}

Status FileServerServiceImpl::ReadFile(ServerContext* context,
		const ParallelFileSystem::ReadFileRequest* request,
		ParallelFileSystem::DataReply* reply) {
	pthread_mutex_lock(&lock);
	auto it = files.find(request->name());
	if(it == files.end()) {
		printf("File %s does not exist\n", request->name().c_str());
		return Status::CANCELLED;
	}
	FSFile* file = it->second;
	pthread_mutex_unlock(&lock);
	int strip = (request->start() / (STRIP_SIZE * pfsBlockSizeInBytes))
			% file->getStripWidth();
	int maxSize = (request->start() + STRIP_SIZE * pfsBlockSizeInBytes) /
			(STRIP_SIZE * pfsBlockSizeInBytes);
	maxSize *= (STRIP_SIZE * pfsBlockSizeInBytes);
	std::string stripPath = file->getStripPath(strip);
	printf("Read file request for %s from %d to %d\n", stripPath.c_str(),
			request->start(), request->start() + request->size());
	if(stripPath.size() == 0) {
		printf("Incorrect server recieved the request for this strip\n");
		return Status::CANCELLED;
	}
	if(request->start() + request->size() > maxSize) {
		printf("Reading from two strips of %s at the same time!, from %d to %d\n",
				request->name().c_str(), request->start(), request->start() + request->size());
		return Status::CANCELLED;
	}
	FILE* fdes = fopen(stripPath.c_str(), "rb");
	if(fdes == 0) {
		printf("Reading from uncreated file %s\n", request->name().c_str());
		return Status::CANCELLED;
	}
	fseek(fdes, request->start(), SEEK_SET);
	char* data = new char[request->size()];
	fread(data, sizeof(char), request->size(), fdes);
	fclose(fdes);

	reply->set_data(data, request->size());
	delete data;

	return Status::OK;
}

Status FileServerServiceImpl::WriteFile(ServerContext* context,
		const ParallelFileSystem::WriteFileRequest* request,
		ParallelFileSystem::StatusReply* reply) {
	pthread_mutex_lock(&lock);
	auto it = files.find(request->name());
	if(it == files.end()) {
		printf("File %s does not exist\n", request->name().c_str());
		return Status::CANCELLED;
	}
	FSFile* file = it->second;
	pthread_mutex_unlock(&lock);
	int strip = (request->start() / (STRIP_SIZE * pfsBlockSizeInBytes))
			% file->getStripWidth();
	int maxSize = (request->start() + STRIP_SIZE * pfsBlockSizeInBytes) /
			(STRIP_SIZE * pfsBlockSizeInBytes);
	maxSize *= (STRIP_SIZE * pfsBlockSizeInBytes);
	std::string stripPath = file->getStripPath(strip);
	printf("Write file request for %s from %d to %d\n", stripPath.c_str(),
			request->start(), request->start() + int(request->data().size()));
	if(stripPath.size() == 0) {
		printf("Incorrect server recieved the request for this strip\n");
		return Status::CANCELLED;
	}
	if(request->start() + request->data().size() > maxSize) {
		printf("Writing to two strips at the same time!, from %d to %d, maxSize : %d\n",
				request->start(), request->start() + int(request->data().size()),
				maxSize);
		return Status::CANCELLED;
	}
	FILE* fdes = fopen(stripPath.c_str(), "rb+");
	fseek(fdes, request->start(), SEEK_SET);
	fwrite(request->data().c_str(), sizeof(char), request->data().size(), fdes);
	fclose(fdes);
	return Status::OK;
}

Status FileServerServiceImpl::ReadBlock(ServerContext* context,
		const ParallelFileSystem::ReadBlockRequest* request,
		ParallelFileSystem::DataReply* reply) {
	pthread_mutex_lock(&lock);
	auto it = files.find(request->name());
	if(it == files.end()) {
		printf("File %s does not exist\n", request->name().c_str());
		return Status::CANCELLED;
	}
	FSFile* file = it->second;
	pthread_mutex_unlock(&lock);
	int strip = (request->addr() / (STRIP_SIZE * pfsBlockSizeInBytes))
			% file->getStripWidth();
	int maxSize = (request->addr() + STRIP_SIZE * pfsBlockSizeInBytes) /
			(STRIP_SIZE * pfsBlockSizeInBytes);
	maxSize *= (STRIP_SIZE * pfsBlockSizeInBytes);
	std::string stripPath = file->getStripPath(strip);
	printf("Read block request for %s from %d to %d\n", stripPath.c_str(),
			request->addr(), request->addr() + pfsBlockSizeInBytes);
	if(stripPath.size() == 0) {
		printf("Incorrect server recieved the request for this strip\n");
		return Status::CANCELLED;
	}
	if(request->addr() + pfsBlockSizeInBytes > maxSize) {
		printf("Reading from two strips of %s at the same time!, from %d to %d\n",
				request->name().c_str(), request->addr(), request->addr() + pfsBlockSizeInBytes);
		return Status::CANCELLED;
	}
	FILE* fdes = fopen(stripPath.c_str(), "rb");
	if(fdes == 0) {
		printf("Reading from uncreated file %s\n", request->name().c_str());
		return Status::CANCELLED;
	}
	fseek(fdes, request->addr(), SEEK_SET);
	char* data = new char[pfsBlockSizeInBytes];
	fread(data, sizeof(char), pfsBlockSizeInBytes, fdes);
	fclose(fdes);

	reply->set_data(data, pfsBlockSizeInBytes);
	delete data;

	return Status::OK;
}

Status FileServerServiceImpl::WriteBlock(ServerContext* context,
		const ParallelFileSystem::WriteBlockRequest* request,
		ParallelFileSystem::StatusReply* reply) {
	pthread_mutex_lock(&lock);
	auto it = files.find(request->name());
	if(it == files.end()) {
		printf("File %s does not exist\n", request->name().c_str());
		return Status::CANCELLED;
	}
	FSFile* file = it->second;
	pthread_mutex_unlock(&lock);
	int strip = (request->addr() / (STRIP_SIZE * pfsBlockSizeInBytes))
						% file->getStripWidth();
	int maxSize = (request->addr() + STRIP_SIZE * pfsBlockSizeInBytes) /
			(STRIP_SIZE * pfsBlockSizeInBytes);
	maxSize *= (STRIP_SIZE * pfsBlockSizeInBytes);
	std::string stripPath = file->getStripPath(strip);
	printf("Write block request for %s from %d to %d\n", stripPath.c_str(),
			request->addr(), request->addr() + pfsBlockSizeInBytes);
	if(stripPath.size() == 0) {
		printf("Incorrect server recieved the request for this strip\n");
		return Status::CANCELLED;
	}
	if(request->addr() + pfsBlockSizeInBytes > maxSize) {
		printf("Writing to two strips at the same time!, from %d to %d, maxSize : %d\n",
				request->addr(), request->addr() + pfsBlockSizeInBytes,
				maxSize);
		return Status::CANCELLED;
	}
	FILE* fdes = fopen(stripPath.c_str(), "rb+");
	fseek(fdes, request->addr(), SEEK_SET);
	for(int i = 0; i < pfsBlockSizeInBytes; i++)
		if(request->dirty()[i]) {
			fwrite(request->data().c_str() + i, sizeof(char), 1, fdes);
		}
		else {
			fseek(fdes, 1, SEEK_SET);
		}
	fclose(fdes);
	return Status::OK;
}
