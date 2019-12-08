/*
 * metadata_manager.cpp
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#include "metadata_manager.h"

std::unordered_map<std::string, std::shared_ptr<Client::Stub>> MetadataManagerServiceImpl::clientStubs;

MetadataManagerServiceImpl::MetadataManagerServiceImpl() {
	// TODO Auto-generated constructor stub
	pthread_mutex_init(&filesLock, nullptr);
}

MetadataManagerServiceImpl::~MetadataManagerServiceImpl() {
	// TODO Auto-generated destructor stub
}

Status MetadataManagerServiceImpl::ClientStart(ServerContext* context,
		const ParallelFileSystem::ClientStartRequest* request,
		ParallelFileSystem::StatusReply* reply) {
	std::shared_ptr<Client::Stub> stub_ =
			Client::NewStub(grpc::CreateChannel(
					request->ip(), grpc::InsecureChannelCredentials()));
	auto it = clientStubs.find(context->peer());
	if(it != clientStubs.end()) {
		it->second.reset();
	}
	clientStubs[context->peer()] = stub_;
	return Status::OK;
}

Status MetadataManagerServiceImpl::CreateFile(ServerContext* context,
		const ParallelFileSystem::CreateFileRequest* request,
		ParallelFileSystem::StatusReply* reply) {
	pthread_mutex_lock(&filesLock);
	if(files.find(request->name()) != files.end()) {
		pthread_mutex_unlock(&filesLock);
		printf("File %s already exists\n", request->name().c_str());
		reply->set_error(ERROR_ALREADY_EXISTS);
		return Status::CANCELLED;
	}
	MMFile* file = new MMFile();
	file->setName(request->name());
	file->setSize(0);
	file->setCreationTime(time(0));
	file->setLastModifiedTime(time(0));
	files[request->name()] = file;
	pthread_mutex_unlock(&filesLock);
	printf("File %s is created by %s\n", request->name().c_str(),
			context->peer().c_str());
	return Status::OK;
}

Status MetadataManagerServiceImpl::OpenFile(ServerContext* context,
		const ParallelFileSystem::OpenFileRequest* request,
		ParallelFileSystem::FileStatReply* reply) {
	StatusReply* status = new StatusReply;
	pthread_mutex_lock(&filesLock);
	auto it = files.find(request->name());
	if(it == files.end()) {
		pthread_mutex_unlock(&filesLock);
		status->set_error(ERROR_DOES_NOT_EXIST);
		printf("File %s doesn't exist to open\n", request->name().c_str());
		return Status::CANCELLED;
	}
	MMFile* file = it->second;
	if(!file->open(context->peer())) {
		pthread_mutex_unlock(&filesLock);
		status->set_error(ERROR_REOPEN_FILE);
		printf("File %s is reopened by %s\n", request->name().c_str(),
				context->peer().c_str());
		return Status::CANCELLED;
	}
	else {
		pthread_mutex_unlock(&filesLock);
		reply->set_error(0);
		reply->set_creationtime(file->getCreationTime());
		reply->set_lastmodified(file->getLastModifiedTime());
		reply->set_filesize(file->getSize());
		printf("File %s is opened by %s\n", request->name().c_str(),
				context->peer().c_str());
		return Status::OK;
	}
}

Status MetadataManagerServiceImpl::CloseFile(ServerContext* context,
		const ParallelFileSystem::CloseFileRequest* request,
		ParallelFileSystem::StatusReply* reply) {
	pthread_mutex_lock(&filesLock);
	auto it = files.find(request->name());
	if(it == files.end()) {
		pthread_mutex_unlock(&filesLock);
		printf("File %s doesn't exist to close\n", request->name().c_str());
		return Status::CANCELLED;
	}
	MMFile* file = it->second;
	pthread_mutex_unlock(&filesLock);
	if(!file->close(context->peer())) {
		printf("File %s is closed by %s while it was not opened before\n",
				request->name().c_str(), context->peer().c_str());
		return Status::CANCELLED;
	}
	else {
		printf("File %s is closed by %s\n", request->name().c_str(),
				context->peer().c_str());
		return Status::OK;
	}
}

Status MetadataManagerServiceImpl::DeleteFile(ServerContext* context,
		const ParallelFileSystem::DeleteFileRequest* request,
		ParallelFileSystem::StatusReply* reply) {
	pthread_mutex_lock(&filesLock);
	auto it = files.find(request->name());
	if(it == files.end()) {
		pthread_mutex_unlock(&filesLock);
		printf("File %s doesn't exist to delete\n", request->name().c_str());
		reply->set_error(ERROR_DOES_NOT_EXIST);
		return Status::CANCELLED;
	}
	MMFile* file = it->second;
	if(file->isOpen()) {
		pthread_mutex_unlock(&filesLock);
		printf("Trying to delete and open file : %s\n", request->name().c_str());
		reply->set_error(ERROR_DELETE_OPEN_FILE);
		return Status::CANCELLED;
	}
	// TODO delete the file
	delete it->second;
	files.erase(it);
	printf("File %s is deleted by %s\n", request->name().c_str(),
			context->peer().c_str());
	pthread_mutex_unlock(&filesLock);
	return Status::OK;
}

Status MetadataManagerServiceImpl::GetPermission(ServerContext* context,
		const ParallelFileSystem::PermissionRequest* request,
		ParallelFileSystem::PermissionReply* reply) {
	pthread_mutex_lock(&filesLock);
	auto it = files.find(request->name());
	if(it == files.end()) {
		pthread_mutex_unlock(&filesLock);
		printf("File %s doesn't exist to get permission\n", request->name().c_str());
		reply->set_error(ERROR_DOES_NOT_EXIST);
		return Status::CANCELLED;
	}
	MMFile* file = it->second;
	if(!file->isOpenedBy(context->peer())) {
		pthread_mutex_unlock(&filesLock);
		printf("Trying to get permission for an unopened file %s by : %s\n",
				request->name().c_str(), context->peer().c_str());
		reply->set_error(ERROR_GET_PERMISSION_UNOPENED_FILE);
		return Status::CANCELLED;
	}
	pthread_mutex_unlock(&filesLock);
	Permission* permit = file->waitAndGetPermissionFor(context->peer(),
			request->start(), request->end(), request->iswrite());
	reply->set_start(permit->getStart());
	reply->set_end(permit->getEnd());
	reply->set_error(0);
	printf("%s have got permission for %s %u %u %d\n",
			context->peer().c_str(), request->name().c_str(), permit->getStart(),
			permit->getEnd(), permit->isWrite());
	return Status::OK;
}

struct RevokePermitFuncData {
	RevokePermissionRequest request;
	std::shared_ptr<Client::Stub> stub;
};

void* revokePermit(void* data) {
	RevokePermitFuncData* func_data = reinterpret_cast<RevokePermitFuncData*>(data);
	// Container for the data we expect from the server.
	StatusReply reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	ClientContext context;

	// The actual RPC.
	Status status = func_data->stub->RevokePermission(&context, func_data->request, &reply);
	delete func_data;
	return nullptr;
}

pthread_t* MetadataManagerServiceImpl::sendRevokePermissionRequest(
		std::string peer, std::string filename,
		uint32_t start, uint32_t end, bool write) {
	printf("send revoke for %s to %s: %u %u\n", filename.c_str(), peer.c_str(),
			start, end);

	RevokePermitFuncData* data = new RevokePermitFuncData;
	data->request.set_name(filename);
	data->request.set_start(start);
	data->request.set_end(end);
	data->request.set_iswrite(write);
	data->stub = clientStubs[peer];

	pthread_t* th = new pthread_t;
	pthread_create(th, nullptr, &revokePermit, (void*)data);

	return th;
}
