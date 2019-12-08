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
}

PFSClient::~PFSClient() {
}

void PFSClient::initialize(int argc, char** argv) {
	channel_ = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
	stub_ = MetadataManager::NewStub(channel_);

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
}

void PFSClient::finalize() {
	client->Shutdown();
	pthread_join(clientServiceThread, nullptr);
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
	// TODO do actual read to the server
	openedFiles[foundAt]->unlockPermission(offset, offset + nbyte, false);
	return 0;
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
	sleep(5);
	// TODO do actual write to the server
	openedFiles[foundAt]->unlockPermission(offset, offset + nbyte, true);
	return 0;
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
