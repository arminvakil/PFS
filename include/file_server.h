/*
 * file_server.h
 *
 *  Created on: Dec 8, 2019
 *      Author: armin
 */

#ifndef FILE_SERVER_FILE_SERVER_H_
#define FILE_SERVER_FILE_SERVER_H_

#include <unordered_map>
#include <grpcpp/grpcpp.h>

#include "message.grpc.pb.h"
#include "util.h"
#include "fs_file.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using namespace ParallelFileSystem;
using ParallelFileSystem::FileServer;

class FileServerServiceImpl : public FileServer::Service {
	std::string address;
	std::string filesDirectory;
	std::unordered_map<std::string, FSFile*> files;
public:
	FileServerServiceImpl(std::string address);
	virtual ~FileServerServiceImpl();

	Status CreateFile(ServerContext* context,
			const ParallelFileSystem::CreateStripeRequest* request,
			ParallelFileSystem::StatusReply* reply) override;

	Status ReadFile(ServerContext* context,
			const ParallelFileSystem::ReadFileRequest* request,
			ParallelFileSystem::DataReply* reply) override;

	Status WriteFile(ServerContext* context,
			const ParallelFileSystem::WriteFileRequest* request,
			ParallelFileSystem::StatusReply* reply) override;
};

#endif /* FILE_SERVER_FILE_SERVER_H_ */
