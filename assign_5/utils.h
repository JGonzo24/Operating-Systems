#ifndef UTILS_H 
#define UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>


#define DIRECT_ZONES 7
#define INODE_SIZE 64
#define DIR_ENTRY_SIZE 64
/*
 * @brief The structs to be used for getting args
 */
typedef struct {
  bool verbose;
  int part;
  int subpart;
  char *imgfile;
  char *path;
} minls_input_t;

typedef struct {
  bool verbose;
  int part;
  int subpart;
  char *imgfile;
  char *srcpath;
  char *dstpath;
} minget_input_t;

typedef enum {
  MINLS_TYPE,
  MINGET_TYPE
} struct_type;

typedef struct {
  struct_type type;
  union {
    minls_input_t* minls_struct;
    minget_input_t* minget_struct;
  } struct_var;
} args_struct_t;

typedef struct __attribute__((packed)) partition_table_entry_t {
  uint8_t  bootind;      // 0x80 if bootable
  uint8_t  start_head;
  uint8_t  start_sec;
  uint8_t  start_cyl;
  uint8_t  type;         // 0x81 for Minix
  uint8_t  end_head;
  uint8_t  end_sec;
  uint8_t  end_cyl;
  uint32_t lFirst;       // first sector (LBA)
  uint32_t size;         // size in sectors
} partition_table_entry_t; 

typedef struct suberblock {
  /* Minix Version 3 Superblock
  * this structure found in fs/super.h
  * in minix 3.1.1
  */
  /* on disk. These fields and orientation are non–negotiable */
  uint32_t ninodes; /* number of inodes in this filesystem */
  uint16_t pad1; /* make things line up properly */
  int16_t i_blocks; /* # of blocks used by inode bit map */
  int16_t z_blocks; /* # of blocks used by zone bit map */
  uint16_t firstdata; /* number of first data zone */
  int16_t log_zone_size; /* log2 of blocks per zone */
  int16_t pad2; /* make things line up again */
  uint32_t max_file; /* maximum file size */
  uint32_t zones; /* number of zones on disk */
  int16_t magic; /* magic number */
  int16_t pad3; /* make things line up again */
  uint16_t blocksize; /* block size in bytes */
  uint8_t subversion; /* filesystem sub–version */
} superblock_t;

typedef struct {
  FILE* img;
  off_t fs_start;
  superblock_t sb;
} fs_t;

typedef struct inode {
  uint16_t mode;
  uint16_t links;
  uint16_t uid;
  uint16_t gid;
  uint32_t size;
  uint32_t atime;
  uint32_t mtime;
  uint32_t ctime;
  uint32_t zone[DIRECT_ZONES];
  uint32_t indirect;
  uint32_t two_indirect;
  uint32_t unused;
} inode_t;

typedef struct minix_dir_entry {
  uint32_t inode;
  unsigned char name[60];
} minix_dir_entry;


void process_zone(fs_t *fs,
                         uint32_t zone,
                         unsigned char *buf,
                         size_t zone_bytes,
                         uint32_t *remaining,
                         FILE *out,
                         ssize_t *total_written,
                         bool *error, bool *seen_data);

void dir_process_zone(fs_t *fs,
                             uint32_t zone,
                             unsigned char *raw,
                             size_t zone_bytes,
                             uint32_t *remaining,
                             size_t *buf_pos,
                             bool *error);

ssize_t fs_read_file(fs_t *fs, inode_t *inode, FILE *out);
int fs_lookup_path(fs_t *fs, const char *path,
                   inode_t *out_inode, uint32_t *out_inum);
int inode_is_directory(inode_t* inode);
void mode_to_string(uint16_t mode, char out[11]);
int fs_read_directory(fs_t *fs, inode_t* dir_inode, 
				minix_dir_entry* entries);
off_t zone_to_offset(fs_t* fs, uint32_t zone);
int fs_read_inode(fs_t *fs, uint32_t inum, inode_t* out);
int read_superblock(fs_t* fs);
args_struct_t* Getopts(int argc, char *argv[]);
int allocate_struct(args_struct_t*, int argc, char *argv[]);
void free_args(args_struct_t* args);
FILE* open_img(const char *path);
fs_t fs_open(const char* path);
int read_partition_table(fs_t* fs, off_t offset, 
			partition_table_entry_t parts[4]);
int select_partition_table(int index, fs_t* fs,
			 partition_table_entry_t parts[4]);





#endif
