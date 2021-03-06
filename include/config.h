#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <sys/types.h>
#include <unistd.h>

#define PFS_BLOCK_SIZE 1 // 1 Kilobyte
#define STRIP_SIZE 1     // 4 blocks
#define NUM_FILE_SERVERS 5
#define CLIENT_CACHE_SIZE 2 // 2 Megabytes

#define NO_CACHE 0

struct pfs_stat {
  time_t pst_mtime; /* time of last data modification */
  time_t pst_ctime; /* time of creation */
  off_t pst_size;    /* File size in bytes */
};

#endif
