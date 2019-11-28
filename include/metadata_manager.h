/*
 * metadata_manager.h
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#ifndef COMMON_METADATA_MANAGER_H_
#define COMMON_METADATA_MANAGER_H_

#include <iostream>
#include <unordered_map>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "message.grpc.pb.h"
#include "file.h"
#include "util.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using namespace ParallelFileSystem;
using ParallelFileSystem::MetadataManager;

class MetadataManagerServiceImpl final : public MetadataManager::Service {
	std::unordered_map<std::string, File*> files;
public:
	MetadataManagerServiceImpl();
	virtual ~MetadataManagerServiceImpl();

	Status CreateFile(ServerContext* context,
			const ParallelFileSystem::CreateFileRequest* request,
			ParallelFileSystem::StatusReply* reply) override {
		if(files.find(request->name()) != files.end()) {
			std::cout << "File " << request->name() << " already exists\n";
			reply->set_error(ERROR_ALREADY_EXISTS);
			return Status::CANCELLED;
		}
		File* file = new File();
		file->setName(request->name());
		file->setSize(0);
		file->setCreationTime(time(0));
		file->setLastModifiedTime(time(0));
		files[request->name()] = file;
		std::cout << "File " << request->name() << " is created\n";
		return Status::OK;
	}
//	Status SayHelloAgain(ServerContext* context, const HelloRequest* request,
//			HelloReply* reply) override {
//		std::string prefix("Hello again ");
//		reply->set_message(prefix + request->name());
//		std::cout << "Hello again request is serviced\n";
//		return Status::OK;
//	}
};

#endif /* COMMON_METADATA_MANAGER_H_ */
