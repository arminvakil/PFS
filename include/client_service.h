/*
 * client_service.h
 *
 *  Created on: Dec 4, 2019
 *      Author: armin
 */

#ifndef CLIENT_CLIENT_SERVICE_H_
#define CLIENT_CLIENT_SERVICE_H_

#include <grpcpp/grpcpp.h>

#include "message.grpc.pb.h"
#include "mm_file.h"
#include "util.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using namespace ParallelFileSystem;
using ParallelFileSystem::Client;

class ClientServiceImpl final : public Client::Service {
public:
	ClientServiceImpl();
	virtual ~ClientServiceImpl();
	Status RevokePermission(ServerContext* context,
			const ParallelFileSystem::RevokePermissionRequest* request,
			ParallelFileSystem::StatusReply* reply) override;
};

#endif /* CLIENT_CLIENT_SERVICE_H_ */
