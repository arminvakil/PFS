#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pfs.h"

/* This test set consists of three clients */
/* Client #1 - Creates a new file and writes 4KB of data on to it */
/* It takes a source file as a command line argument. Data from source
 file is written on to the new pfs_file */
/* test1-c1.c, test1-c2.c test1-c3.c must be executed one after the other*/

#define ONEKB 1024

int main(int argc, char *argv[]) {
	int ifdes, fdes;
	int err_value;
	char input_fname[20];
	char *buf;
	int cache_hit;
	ssize_t nread;

	// the command line arguments include an input filename
	if (argc < 6) {
		printf("usage: a.out <input filename> client_no start end X\n");
		exit(0);
	}

	int start = atoi(argv[3]);
	int end = atoi(argv[4]);

	// Initialize the client
	initialize(argc, argv);

	strcpy(input_fname, argv[1]);
	ifdes = open(input_fname, O_RDONLY);
	buf = (char *) malloc(end);
	for(int i = 0; i < end; i++)
		buf[i] = atoi(argv[5]);
//	nread = pread(ifdes, (void *) buf, 4 * ONEKB, 0);

	char pfs_fname[20];
	strcpy(pfs_fname, argv[1]);
	// create a file only once, say at client 1
	err_value = pfs_create(pfs_fname, 7);
//	err_value = pfs_create("pfs_file1", 3);
	if (err_value < 0) {
		printf("Unable to create a file\n");
//		exit(0);
	}

	// All the clients open the file
	fdes = pfs_open(pfs_fname, 'w');
	if (fdes < 0) {
		printf("Error opening file\n");
		exit(0);
	}
	std::cerr << "File descriptor : " << fdes << "\n";

	//At Client 1
	//Write the first 200 bytes of data from the input file onto pfs_file
	err_value = pfs_write(fdes, (void *) buf, end - start, start, &cache_hit);
	printf("Wrote %d bytes to the file, cache_hit: %d\n", err_value, cache_hit);

	char* read_buf = new char[end - start];
	err_value = pfs_read(fdes, (void *) read_buf, end - start, start, &cache_hit);

	printf("Read %d bytes from the file, cache_hit: %d\n", err_value, cache_hit);
	for(int i = 0; i < end - start; i++)
		std::cerr << int(read_buf[i]) << " ";
	std::cerr << "\n";

	pfs_close(fdes);
	free(buf);
	close(ifdes);
	finalize();
	return 0;
}
