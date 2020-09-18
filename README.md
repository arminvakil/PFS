This project is a simple Parallel File System (PFS) which provides access to file data for parallel applications which will be striped across multiple file servers and it uses Google Remote Procedure Calls(gRPC) for communication. The figure below gives an overview of PFS and its components.

![Overview of PFS Components][1]

Like most other files systems, the PFS is also designed as client-server architecture, although with multiple servers which typically run on separate nodes and have disks attached to them. Each file in the PFS system is striped across the disks of the file servers and the application processes interact with the file system using a client library. The PFS also has a Metadata Manager which handles all the metadata associated with files. The Metadata Manager does not take part in the actual read/write operations.

**Metadata Manager**

The metadata manager is responsible for the storage and maintaining of all the metadata associated with a file. In this project, we assumed a flat file system consisting of only one root directory. Each file in the directory records the following metadata:

Filename
File size
Time of creation
Time of last modification
File recipe
Note that a given file in the PFS is striped across various file servers to facilitate parallel access. The specifics of this file distribution are described by what is known as the file recipe. Note that the block size and the stripe size are a fixed value which will are defined as part of a configuration file. The stripe width defines how many nodes the file will be split across, will be specified by the user when the file is created. When a client opens a file, the metadata manager returns the file recipe to the client. Subsequently the client can directly communicate to the file servers to access file data.

**Token Management**

The metadata manager also performs the task of a token manager for ensuring consistency in the system. The goal here is to guarantee single-node equivalent POSIX/UNIX  (sequential) consistency for file system operations across the system. Every file system operation acquires an appropriate read or write lock to synchronize with conflicting operations on other nodes before reading or updating any file system data. In this project, we implemented byte-level locking in order to allow parallel applications to write concurrently to different bytes of the same file. The token manager coordinates locks by handing out lock tokens. Note that, lock tokens is/are associated with every byte to be accessed within a block and is usually handed out as ranges (starting byte to ending byte). There are two types of lock tokens that are issued by the token manager: read tokens and write tokens. Multiple nodes can simultaneously acquire read tokens for the same file-block. However, block-range tokens for parallel writes are ensured that ranges are exclusive. The first node to read/write a file acquires the block-range token for the whole file (zero to infinity). As long as no other nodes require access to the same file, all read and write operations can be processed locally if cached. When a second node begins writing to the same file it will need to revoke the relevant part of the block-range token held by the first node. When the first node receives a revoke request, it checks whether the file is still in use. If the file has since been closed, the first node will give up the whole (range) token, and the second node will then be able to acquire a token covering the whole file. On the other hand, if the second node starts writing to a file before the first node closes the file, the first node will relinquish only part of its block-range token. If the first node is writing sequentially at offset o1 and the second node at offset o2, the first node will relinquish its token from o2 to infinity (if o2 > o1) or from zero to o1 (if o2 < o1).  Consequently two clients can acquire  write tokens for the same block if they are writing different bytes within the block.  On the other hand, if they are writing to the same byte, they would still obtain a token/lock for that byte, in some serial order, before they each write into it. An example execution with 3 clients, and the tokens/locks that are acquired by each of these clients when accessing a 4KB file is shown below.

![Image of Example][2]

**File Servers**

There are a fixed number of file servers in the network which will be also be specified in the configuration file. Every file in the PFS is striped across a number of these file servers. You can simulate file striping by implementing each stripe using the native file system in the respective file servers.

 

**PFS Client**

The following file system interface is implemented for the PFS Client:

```cpp
int pfs_create(const char *filename, int stripe_width);

int pfs_open(const char *filename, const char mode);

ssize_t pfs_read(int filedes, void *buf, ssize_t nbyte, off_t offset, int *cache_hit);

ssize_t pfs_write(int filedes, const void *buf, size_t nbyte, off_t offset, int *cache_hit);

int pfs_close(int filedes);

int pfs_delete(const char *filename);

int pfs_fstat(int filedes, struct pfs_stat *buf); // Check the config file for the definition of pfs_stat structure
```
 

**Client Cache**

PFS uses local cache, at block granularity, on each client having a default size of 2 MB. The client cache consists of (1) free space and (2) used space. The used space in the client cache can further be divided into blocks that have been modified (i.e. dirty blocks which have not been flushed to the server yet) or clean blocks. As part of the cache management scheme, two threads are implemented:

1. Harvester Thread: The harvester thread maintains a sufficient amount of free space in the client cache. Whenever the free space falls below a specific lower threshold (50KB) , the harvester thread frees up memory from the used space based on an LRU policy till the amount of free space reaches an upper threshold (100KB).
2. Flusher Thread: The flusher thread periodically (every 30 seconds) flushes all the dirty blocks (modified blocks) back to the appropriate file servers.

If the client has the appropriate tokens, and a read/write call issued by the client results in a cache hit, the operations perform locally on the client cache. On the other hand, whenever there is a cache miss, the blocks will be fetched from the file server which will then be cached locally on the client using the available free space. The client returns information about a cache hit or miss for every read/write call using the Boolean argument cache_hit which is passed as a parameter to both the pfs_read() and pfs_write() function calls. If the client needs to acquire fresh tokens from the token manager for a read/write call, the file data need to be fetched from the file servers. Whenever a client is required to relinquish a part of its token, it flushes the dirty blocks to the file servers before relinquishing the tokens to the token manager.

[1]:
https://github.com/arminvakil/PFS/blob/master/pfsOverview.png
[2]:
https://github.com/arminvakil/PFS/blob/master/pfs.jpg
