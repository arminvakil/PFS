/*
 * client.h
 *
 *  Created on: Nov 26, 2019
 *      Author: armin
 */

#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <sys/types.h>
#include <unistd.h>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include <message.grpc.pb.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using ParallelFileSystem::MetadataManager;

using namespace ParallelFileSystem;

class PFSClient {
	PFSClient();
	virtual ~PFSClient();

	static PFSClient* _instance;
public:

	static inline PFSClient* getInstance() {
		if (_instance != nullptr)
			return _instance;
		_instance = new PFSClient();
		return _instance;
	}

	void initialize(int argc, char** argv);

	int createFile(const char *filename, int stripe_width);

	int openFile(const char *filename, const char mode);

	ssize_t readFile(int filedes, void *buf, ssize_t nbyte, off_t offset, int *cache_hit);

	ssize_t writeFile(int filedes, const void *buf, size_t nbyte, off_t offset, int *cache_hit);

	int closeFile(int filedes);

	int deleteFile(const char *filename);

	int getFileStat(int filedes, struct pfs_stat *buf); // Check the config file for the definition of pfs_stat structure

private:
	std::shared_ptr<MetadataManager::Stub> stub_;
};

#endif /* _CLIENT_H_ */
