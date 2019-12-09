/*
 * pfs_file_server.cpp
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "message.grpc.pb.h"
#include "file_server.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

int main(int argc, char** argv) {
	if(argc < 2) {
		std::cerr << "Usage : " << argv[0] << " server_no\n";
		return -1;
	}

	std::string server_address("0.0.0.0:3000");
	server_address.append(argv[1]);
	FileServerServiceImpl service(server_address);

	ServerBuilder builder;
	// Listen on the given address without any authentication mechanism.
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	// Register "service" as the instance through which we'll communicate with
	// clients. In this case it corresponds to an *synchronous* service.
	builder.RegisterService(&service);
	// Finally assemble the server.
	std::unique_ptr<Server> server(builder.BuildAndStart());
	std::cout << "File Server listening on " << server_address << std::endl;

	// Wait for the server to shutdown. Note that some other thread must be
	// responsible for shutting down the server for this call to ever return.
	server->Wait();
}
