/*
 * client.cpp
 *
 *  Created on: Nov 26, 2019
 *      Author: armin
 */

#include <iostream>
#include <pfs_client.h>

PFSClient* PFSClient::_instance = nullptr;

PFSClient::PFSClient() {
	stub_ = nullptr;
}

PFSClient::~PFSClient() {
}

void PFSClient::initialize(int argc, char** argv) {
	stub_ = MetadataManager::NewStub(grpc::CreateChannel(
			"localhost:50051", grpc::InsecureChannelCredentials()));
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

	return -1;
}

int PFSClient::openFile(const char *filename, const char mode) {
	return -1;
}

ssize_t PFSClient::readFile(int filedes, void *buf, ssize_t nbyte, off_t offset,
		int *cache_hit) {
	return -1;
}

ssize_t PFSClient::writeFile(int filedes, const void *buf, size_t nbyte,
		off_t offset, int *cache_hit) {
	return -1;
}

int PFSClient::closeFile(int filedes) {
	return -1;
}

int PFSClient::deleteFile(const char *filename) {
	return -1;
}

int PFSClient::getFileStat(int filedes, struct pfs_stat *buf) {
	return -1;
}
