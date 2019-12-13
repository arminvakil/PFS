/*
 * metadata_manager.h
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#ifndef COMMON_METADATA_MANAGER_H_
#define COMMON_METADATA_MANAGER_H_

#include <iostream>
#include <memory>
#include <semaphore.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "message.grpc.pb.h"
#include "mm_file.h"
#include "util.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using namespace ParallelFileSystem;
using ParallelFileSystem::MetadataManager;
using grpc::Channel;
using grpc::ClientContext;

class MetadataManagerServiceImpl final : public MetadataManager::Service {
	std::unordered_map<std::string, MMFile*> files;
	static std::unordered_map<std::string, std::shared_ptr<Client::Stub>> clientStubs;
	std::vector<std::shared_ptr<FileServer::Stub>> fileServerStubs;
	pthread_mutex_t filesLock;
public:
	MetadataManagerServiceImpl();
	virtual ~MetadataManagerServiceImpl();

	Status ClientStart(ServerContext* context,
			const ParallelFileSystem::ClientStartRequest* request,
			ParallelFileSystem::StatusReply* reply) override;

	Status CreateFile(ServerContext* context,
			const ParallelFileSystem::CreateFileRequest* request,
			ParallelFileSystem::StatusReply* reply) override;

	Status OpenFile(ServerContext* context,
			const ParallelFileSystem::OpenFileRequest* request,
			ParallelFileSystem::FileStatReply* reply) override;

	Status CloseFile(ServerContext* context,
			const ParallelFileSystem::CloseFileRequest* request,
			ParallelFileSystem::StatusReply* reply) override;

	Status DeleteFile(ServerContext* context,
			const ParallelFileSystem::DeleteFileRequest* request,
			ParallelFileSystem::StatusReply* reply) override;

	Status GetPermission(ServerContext* context,
			const ParallelFileSystem::PermissionRequest* request,
			ParallelFileSystem::PermissionReply* reply) override;

	Status GetFileDesc(ServerContext* context,
			const ParallelFileSystem::GetFileDescRequest* request,
			ParallelFileSystem::FileStatReply* reply) override;

	static pthread_t* sendRevokePermissionRequest(std::string peer, std::string filename,
			uint32_t start, uint32_t end, bool write);
};

#endif /* COMMON_METADATA_MANAGER_H_ */
