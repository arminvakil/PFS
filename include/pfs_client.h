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
#include <vector>
#include <pthread.h>

#include "cache.h"
#include "cache_block.h"
#include "client_file.h"
#include "client_service.h"

#include <grpcpp/grpcpp.h>
#include <message.grpc.pb.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using ParallelFileSystem::MetadataManager;

using namespace ParallelFileSystem;

class PFSClient {
	std::vector<ClientFile*> openedFiles;
	PFSClient();
	virtual ~PFSClient();

	static PFSClient* _instance;

	static pthread_t clientServiceThread;

	static std::unique_ptr<Server> client;

	std::vector<std::shared_ptr<FileServer::Stub>> fileServerStubs;
public:

	static inline PFSClient* getInstance() {
		if (_instance != nullptr)
			return _instance;
		_instance = new PFSClient();
		return _instance;
	}

	void initialize(int argc, char** argv);

	void finalize();

	static void* clientServiceFunc(void* client_no);

	int createFile(const char *filename, int stripe_width);

	int openFile(const char *filename, const char mode);

	int acquirePermission(int filedes, off_t start, off_t end, bool write);

	void revokePermission(std::string filename, uint32_t start, uint32_t end, bool write);

	ssize_t readFile(int filedes, void *buf, ssize_t nbyte, off_t offset, int *cache_hit);

	ssize_t writeFile(int filedes, const void *buf, size_t nbyte, off_t offset, int *cache_hit);

	int closeFile(int filedes);

	int deleteFile(const char *filename);

	int getFileStat(int filedes, struct pfs_stat *buf); // Check the config file for the definition of pfs_stat structure

	void flush(CacheBlock* block);

	void getBlockFromFileServer(int fileIndex,
			uint32_t blockAddr, const char* data);

private:
	std::shared_ptr<MetadataManager::Stub> stub_;
	std::shared_ptr<grpc::Channel> channel_;

	Cache* cache;
};

#endif /* _CLIENT_H_ */
