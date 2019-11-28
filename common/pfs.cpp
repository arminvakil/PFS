/*
 * pfs.cpp
 *
 *  Created on: Nov 26, 2019
 *      Author: armin
 */

#include <pfs_client.h>
#include "pfs.h"

void initialize(int argc, char** argv) {
	PFSClient::getInstance()->initialize(argc, argv);
	return;
}

int pfs_create(const char *filename, int stripe_width) {
	return PFSClient::getInstance()->createFile(filename, stripe_width);
}

int pfs_open(const char *filename, const char mode) {
	return PFSClient::getInstance()->openFile(filename, mode);
}

ssize_t pfs_read(int filedes, void *buf, ssize_t nbyte, off_t offset, int *cache_hit) {
	return PFSClient::getInstance()->readFile(filedes, buf, nbyte, offset, cache_hit);
}

ssize_t pfs_write(int filedes, const void *buf, size_t nbyte, off_t offset, int *cache_hit) {
	return PFSClient::getInstance()->writeFile(filedes, buf, nbyte, offset, cache_hit);
}

int pfs_close(int filedes) {
	return PFSClient::getInstance()->closeFile(filedes);
}

int pfs_delete(const char *filename) {
	return PFSClient::getInstance()->deleteFile(filename);
}

int pfs_fstat(int filedes, struct pfs_stat *buf) { // Check the config file for the definition of pfs_stat structure
	return PFSClient::getInstance()->getFileStat(filedes, buf);
}
