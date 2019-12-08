/*
 * client_service.cpp
 *
 *  Created on: Dec 4, 2019
 *      Author: armin
 */

#include "client_service.h"
#include "pfs_client.h"

ClientServiceImpl::ClientServiceImpl() {
	// TODO Auto-generated constructor stub

}

ClientServiceImpl::~ClientServiceImpl() {
	// TODO Auto-generated destructor stub
}

Status ClientServiceImpl::RevokePermission(ServerContext* context,
		const ParallelFileSystem::RevokePermissionRequest* request,
		ParallelFileSystem::StatusReply* reply) {
	std::cerr << "client received revoke request : " << request->name() << " "
			<< request->start() << " " << request->end() << " " << request->iswrite() << "\n";
	PFSClient::getInstance()->revokePermission(request->name(), request->start(),
			request->end(), request->iswrite());
	return Status::OK;
}
