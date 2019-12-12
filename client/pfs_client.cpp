/*
 * client.cpp
 *
 *  Created on: Nov 26, 2019
 *      Author: armin
 */

#include <iostream>
#include <pfs_client.h>
#include "client_file.h"
#include "util.h"

PFSClient* PFSClient::_instance = nullptr;
pthread_t PFSClient::clientServiceThread;
std::unique_ptr<Server> PFSClient::client = nullptr;

PFSClient::PFSClient() {
	stub_ = nullptr;
	cache = nullptr;
}

PFSClient::~PFSClient() {
}

void PFSClient::initialize(int argc, char** argv) {
	channel_ = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
	stub_ = MetadataManager::NewStub(channel_);

	for(int i = 0; i < NUM_FILE_SERVERS; i++) {
		std::string file_server_address("0.0.0.0:3000");
		file_server_address.push_back(char('0' + i));
		std::shared_ptr<FileServer::Stub> stub_ =
				FileServer::NewStub(grpc::CreateChannel(
						file_server_address, grpc::InsecureChannelCredentials()));
		fileServerStubs.push_back(stub_);
	}

	pthread_create(&clientServiceThread, nullptr, &(PFSClient::clientServiceFunc), (void*)argv[2]);

	ClientStartRequest request;
	std::string client_address("0.0.0.0:400");
	client_address.append(argv[2]);
	request.set_ip(client_address);

	// Container for the data we expect from the server.
	StatusReply reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	ClientContext context;

	// The actual RPC.
	Status status = stub_->ClientStart(&context, request, &reply);

	// Act upon its status.
	assert(status.ok());

	if(NO_CACHE != 1) {
		// by default with cache, if an argument is given, no cache
		cache = new Cache();
	}
}

void PFSClient::finalize() {
	client->Shutdown();
	pthread_join(clientServiceThread, nullptr);
	if(cache)
		delete cache;
}

void* PFSClient::clientServiceFunc(void* client_no) {
	std::string client_address("0.0.0.0:400");
	client_address.append((char*)client_no);
	ClientServiceImpl service;

	ServerBuilder builder;
	// Listen on the given address without any authentication mechanism.
	builder.AddListeningPort(client_address, grpc::InsecureServerCredentials());
	// Register "service" as the instance through which we'll communicate with
	// clients. In this case it corresponds to an *synchronous* service.
	builder.RegisterService(&service);
	// Finally assemble the client.
	client = builder.BuildAndStart();
	std::cout << "Client listening on " << client_address << std::endl;

	// Wait for the client to shutdown. Note that some other thread must be
	// responsible for shutting down the server for this call to ever return.
	client->Wait();
	return nullptr;
}

int PFSClient::createFile(const char *filename, int stripe_width) {
	if(stub_ == nullptr) {
		std::cerr << "stub_ is not created, please call initialize first\n";
		return -1;
	}
	CreateFileRequest request;
	request.set_name(filename);
	request.set_stripewidth(stripe_width);

	// Container for the data we expect from the server.
	StatusReply reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	ClientContext context;

	// The actual RPC.
	Status status = stub_->CreateFile(&context, request, &reply);

	// Act upon its status.
	if (status.ok()) {
		return 0;
	} else {
		std::cout << status.error_code() << ": " << status.error_message()
								<< std::endl;
		return -1 * abs(status.error_code());
	}
}

int PFSClient::openFile(const char *filename, const char mode) {
	if(stub_ == nullptr) {
		std::cerr << "stub_ is not created, please call initialize first\n";
		return -1;
	}
	OpenFileRequest request;
	request.set_name(filename);
	std::string m;
	m.push_back(mode);
	request.set_mode(m);

	// Container for the data we expect from the server.
	FileStatReply reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	ClientContext context;

	// The actual RPC.
	Status status = stub_->OpenFile(&context, request, &reply);

	// Act upon its status.
	if (status.ok()) {
		ClientFile* file = new ClientFile();
		file->setName(filename);
		file->setCreationTime(reply.creationtime());
		file->setLastModifiedTime(reply.lastmodified());
		file->setSize(reply.filesize());
		file->setStripWidth(reply.stripwidth());
		openedFiles.push_back(file);
		return file->getDescriptor();
	} else {
		std::cout << "Cannot open file " << filename << " : " <<
				status.error_code() << ": " <<
				getErrorMessage(status.error_code()) << std::endl;
		return status.error_code();
	}
}

int PFSClient::acquirePermission(int filedes, off_t start, off_t end, bool write) {
	int foundAt = -1;
	PermissionRequest request;
	for(int i = 0; i < openedFiles.size(); i++) {
		std::cerr << __func__ << " " << i << " " << openedFiles[i]->getDescriptor() << "\n";
		if(openedFiles[i]->getDescriptor() == filedes) {
			request.set_name(openedFiles[i]->getName());
			foundAt = i;
			break;
		}
	}
	if(foundAt == -1) {
		std::cout << "Trying to get permission from an unopened file\n";
		return ERROR_CLOSE_UNOPENED_FILE;
	}

	if(openedFiles[foundAt]->hasPermission(start, end, write,
			&openedFiles[foundAt]->lock, openedFiles[foundAt]->permissions)) {
		return foundAt;
	}

	request.set_start(start);
	request.set_end(end);
	request.set_iswrite(write);

	// Container for the data we expect from the server.
	PermissionReply reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	ClientContext context;

	// The actual RPC.
	Status status = stub_->GetPermission(&context, request, &reply);

	// Act upon its status.
	if (status.ok()) {
		std::cerr << "have got permission for : " << reply.start() << " " <<
				reply.end() << " " << write << "\n";
		openedFiles[foundAt]->addPermission(reply.start(), reply.end(), write,
				&openedFiles[foundAt]->lock, openedFiles[foundAt]->permissions,
				true);
		return foundAt;
	} else {
		std::cout << status.error_code() << ": " << status.error_message()
											<< std::endl;
		return -1 * abs(status.error_code());
	}
}

void PFSClient::revokePermission(std::string filename, uint32_t start,
		uint32_t end, bool write) {
	int foundAt = -1;
	for(int i = 0; i < openedFiles.size(); i++) {
		if(openedFiles[i]->getName() == filename) {
			foundAt = i;
			break;
		}
	}
	if(foundAt == -1) {
		std::cout << "MetadataManager tries to get revoke permission of an unopened file\n";
		return;
	}
	ClientFile* file = openedFiles[foundAt];
	file->revokePermission(start, end, write, &(file->lock), file->permissions);
	file->printPermissions(file->permissions);
}

ssize_t PFSClient::readFile(int filedes, void *buf, ssize_t nbyte, off_t offset,
		int *cache_hit) {
	if(stub_ == nullptr) {
		std::cerr << "stub_ is not created, please call initialize first\n";
		return -1;
	}

	int foundAt = acquirePermission(filedes, offset, offset + nbyte, false);
	if(foundAt < 0) {
		// error
		return foundAt;
	}

	ssize_t nbyteBackup = nbyte;
	off_t offsetBackup = offset;

	char* data = (char*)buf;
	if(cache == nullptr) {

		while(nbyte > 0) {
			int fileServerIndex = ((offset / (STRIP_SIZE * pfsBlockSizeInBytes)) %
					openedFiles[foundAt]->getStripWidth()) % NUM_FILE_SERVERS;
			uint32_t lastByteInFileServer = offset + STRIP_SIZE * pfsBlockSizeInBytes;
			lastByteInFileServer /= (STRIP_SIZE * pfsBlockSizeInBytes);
			lastByteInFileServer *= (STRIP_SIZE * pfsBlockSizeInBytes);

			std::cerr << "sending request to : " << fileServerIndex << " " <<
					offset << " " << std::min(lastByteInFileServer - offset, nbyte) << "\n";

			ReadFileRequest request;
			request.set_name(openedFiles[foundAt]->getName());
			request.set_start(offset);
			request.set_size(std::min(lastByteInFileServer - offset, nbyte));

			// Container for the data we expect from the server.
			DataReply reply;

			// Context for the client. It could be used to convey extra information to
			// the server and/or tweak certain RPC behaviors.
			ClientContext context;

			// The actual RPC.
			Status status = fileServerStubs[fileServerIndex]->ReadFile(&context, request, &reply);

			// Act upon its status.
			assert(status.ok());
			assert(reply.data().size() == request.size());
			memcpy(data, reply.data().c_str(), request.size());

			data += std::min(lastByteInFileServer - offset, nbyte);
			nbyte -= std::min(lastByteInFileServer - offset, nbyte);
			offset = lastByteInFileServer;
		}
	}
	else {
		while(nbyte > 0) {
			uint32_t blockAddr = offset / pfsBlockSizeInBytes;
			blockAddr *= pfsBlockSizeInBytes;
			char cacheData[pfsBlockSizeInBytes];
			bool hit = cache->readBlock(filedes, blockAddr, cacheData);
			if(!hit) {
				getBlockFromFileServer(foundAt, blockAddr, cacheData);
				cache->addBlock(filedes, blockAddr, cacheData);
			}
			memcpy(data, cacheData + (offset % pfsBlockSizeInBytes),
					std::min(pfsBlockSizeInBytes - (offset % pfsBlockSizeInBytes), nbyte));
			data += std::min(pfsBlockSizeInBytes - (offset % pfsBlockSizeInBytes), nbyte);
			nbyte -= std::min(pfsBlockSizeInBytes - (offset % pfsBlockSizeInBytes), nbyte);
			offset = blockAddr + pfsBlockSizeInBytes;
		}
	}
	openedFiles[foundAt]->unlockPermission(offsetBackup, offsetBackup + nbyteBackup, false);
	return nbyte;
}

ssize_t PFSClient::writeFile(int filedes, const void *buf, size_t nbyte,
		off_t offset, int *cache_hit) {
	if(stub_ == nullptr) {
		std::cerr << "stub_ is not created, please call initialize first\n";
		return -1;
	}

	int foundAt = acquirePermission(filedes, offset, offset + nbyte, true);
	if(foundAt < 0) {
		// error
		return foundAt;
	}

	size_t nbyteBackup = nbyte;
	off_t offsetBackup = offset;
	char* data = (char*)(buf);
	if(cache == nullptr) {

		while(nbyte > 0) {
			int fileServerIndex = ((offset / (STRIP_SIZE * pfsBlockSizeInBytes)) %
					openedFiles[foundAt]->getStripWidth()) % NUM_FILE_SERVERS;
			uint32_t lastByteInFileServer = offset + STRIP_SIZE * pfsBlockSizeInBytes;
			lastByteInFileServer /= (STRIP_SIZE * pfsBlockSizeInBytes);
			lastByteInFileServer *= (STRIP_SIZE * pfsBlockSizeInBytes);

			std::cerr << "sending request to : " << fileServerIndex << " " <<
					offset << " " << std::min(lastByteInFileServer - offset, (ssize_t)nbyte) << "\n";

			WriteFileRequest request;
			request.set_name(openedFiles[foundAt]->getName());
			request.set_start(offset);
			request.set_data(data, std::min(lastByteInFileServer - offset, (ssize_t)nbyte));

			// Container for the data we expect from the server.
			StatusReply reply;

			// Context for the client. It could be used to convey extra information to
			// the server and/or tweak certain RPC behaviors.
			ClientContext context;

			// The actual RPC.
			Status status = fileServerStubs[fileServerIndex]->WriteFile(&context, request, &reply);

			// Act upon its status.
			assert(status.ok());

			data += std::min(lastByteInFileServer - offset, (ssize_t)nbyte);
			nbyte -= std::min(lastByteInFileServer - offset, (ssize_t)nbyte);
			offset = lastByteInFileServer;
		}
	}
	else {
		while(nbyte > 0) {
			uint32_t blockAddr = offset / pfsBlockSizeInBytes;
			blockAddr *= pfsBlockSizeInBytes;

			bool hit = cache->write(filedes, offset,
					std::min(pfsBlockSizeInBytes - (offset % pfsBlockSizeInBytes), (ssize_t)nbyte),
					data);
			if(!hit) {
				char cacheData[pfsBlockSizeInBytes];
				getBlockFromFileServer(foundAt, blockAddr, cacheData);
				cache->addBlock(filedes, blockAddr, cacheData);
				cache->write(filedes, offset,
						std::min(pfsBlockSizeInBytes - (offset % pfsBlockSizeInBytes), (ssize_t)nbyte),
						data);
			}

			data += std::min(pfsBlockSizeInBytes - (offset % pfsBlockSizeInBytes), (ssize_t)nbyte);
			nbyte -= std::min(pfsBlockSizeInBytes - (offset % pfsBlockSizeInBytes), (ssize_t)nbyte);
			offset = blockAddr + pfsBlockSizeInBytes;
		}
	}
	openedFiles[foundAt]->unlockPermission(offsetBackup, offsetBackup + nbyteBackup, true);
	return nbyteBackup;
}

int PFSClient::closeFile(int filedes) {
	if(stub_ == nullptr) {
		std::cerr << "stub_ is not created, please call initialize first\n";
		return -1;
	}
	CloseFileRequest request;
	bool foundAt = -1;
	for(int i = 0; i < openedFiles.size(); i++) {
		std::cerr << __func__ << " " << i << " " << openedFiles[i]->getDescriptor() << "\n";
		if(openedFiles[i]->getDescriptor() == filedes) {
			request.set_name(openedFiles[i]->getName());
			foundAt = i;
			break;
		}
	}
	if(foundAt == -1) {
		std::cout << "Trying to close an unopened file\n";
		return ERROR_CLOSE_UNOPENED_FILE;
	}

	// Container for the data we expect from the server.
	StatusReply reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	ClientContext context;

	// The actual RPC.
	Status status = stub_->CloseFile(&context, request, &reply);

	// Act upon its status.
	if (status.ok()) {
		delete(openedFiles[foundAt]);
		openedFiles.erase(openedFiles.begin() + foundAt);
		return 0;
	} else {
		std::cout << status.error_code() << ": " << status.error_message()
								<< std::endl;
		return -1 * abs(status.error_code());
	}
}

int PFSClient::deleteFile(const char *filename) {
	if(stub_ == nullptr) {
		std::cerr << "stub_ is not created, please call initialize first\n";
		return -1;
	}
	DeleteFileRequest request;
	request.set_name(filename);

	// Container for the data we expect from the server.
	StatusReply reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	ClientContext context;

	// The actual RPC.
	Status status = stub_->DeleteFile(&context, request, &reply);

	// Act upon its status.
	if (status.ok()) {
		return 0;
	} else {
		std::cout << status.error_code() << ": " << status.error_message()
								<< std::endl;
		return -1 * abs(status.error_code());
	}
}

int PFSClient::getFileStat(int filedes, struct pfs_stat *buf) {
	return -1;
}

void PFSClient::flush(CacheBlock* block) {
	assert(!(block->clean));
	int foundAt = -1;
	WriteBlockRequest request;
	for(int i = 0; i < openedFiles.size(); i++) {
		std::cerr << __func__ << " " << i << " " << openedFiles[i]->getDescriptor() << "\n";
		if(openedFiles[i]->getDescriptor() == block->fdes) {
			request.set_name(openedFiles[i]->getName());
			foundAt = i;
			break;
		}
	}
	assert(foundAt != -1);
	int fileServerIndex = ((block->addr / (STRIP_SIZE * pfsBlockSizeInBytes)) %
			openedFiles[foundAt]->getStripWidth()) % NUM_FILE_SERVERS;
	std::cerr << "sending block to : " << fileServerIndex << " " <<
			block->addr << "\n";

	request.set_addr(block->addr);
	request.set_data(block->data, PFS_BLOCK_SIZE);
	request.set_dirty(block->dirty, PFS_BLOCK_SIZE);

	// Container for the data we expect from the server.
	StatusReply reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	ClientContext context;

	// The actual RPC.
	Status status = fileServerStubs[fileServerIndex]->WriteBlock(&context, request, &reply);

	// Act upon its status.
	assert(status.ok());
}

void PFSClient::getBlockFromFileServer(int fileIndex,
		uint32_t blockAddr, const char* data) {

	int fileServerIndex = ((blockAddr / (STRIP_SIZE * pfsBlockSizeInBytes)) %
			openedFiles[fileIndex]->getStripWidth()) % NUM_FILE_SERVERS;

	ReadBlockRequest request;
	request.set_name(openedFiles[fileIndex]->getName());
	request.set_addr(blockAddr);

	// Container for the data we expect from the server.
	DataReply reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	ClientContext context;

	// The actual RPC.
	Status status = fileServerStubs[fileServerIndex]->ReadBlock(&context, request, &reply);

	// Act upon its status.
	assert(status.ok());
}
