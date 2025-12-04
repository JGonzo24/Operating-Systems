#define _POSIX_C_SOURCE 200809L /* must be before any #include */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

/**
 *
 * Log to stderr.
 *
 * @param label: the name of the program being ran
 * @param verbose: bool stating whether to print or not
 *
 */
#define LOG(label, verbose, fmt, ...)                          \
  do                                                           \
  {                                                            \
    if (verbose)                                               \
    {                                                          \
      fprintf(stderr, "[%s] " fmt "\n", label, ##__VA_ARGS__); \
    }                                                          \
  } while (0)

/**
 * @brief Safe wrapper for fseeko with error handling
 * @param stream File stream to seek on
 * @param offset Offset to seek to
 * @param whence Position reference (SEEK_SET, SEEK_CUR, SEEK_END)
 * @return 0 on success, -1 on error
 */
int safe_fseeko(FILE *stream, off_t offset, int whence)
{
  if (fseeko(stream, offset, whence) != 0)
  {
    perror("fseeko");
    return -1;
  }
  return 0;
}

/**
 * @brief Safe wrapper for fread with error handling
 * @param ptr Pointer to buffer to read into
 * @param size Size of each element
 * @param bytes_to_read Number of bytes to read
 * @param stream File stream to read from
 * @return 0 on success, -1 on error
 */
int safe_fread(void *ptr, size_t size, size_t bytes_to_read, FILE *stream)
{
  if (fread(ptr, size, bytes_to_read, stream) != bytes_to_read)
  {
    perror("fread");
    return -1;
  }
  return 0;
}

/**
 * @brief Safe wrapper for malloc with error handling
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *safe_malloc(size_t size)
{
  void *ptr = malloc(size);
  if (!ptr)
  {
    perror("malloc");
  }
  return ptr;
}

/**
 * @brief Safe wrapper for calloc with error handling
 * @param num_blocks Number of blocks to allocate
 * @param size Size of each block
 * @return Pointer to allocated memory, or NULL on failure
 */
void *safe_calloc(size_t num_blocks, size_t size)
{
  void *ptr = calloc(num_blocks, size);
  if (!ptr)
  {
    perror("calloc");
  }
  return ptr;
}

/**
 * @brief Safely reads a zone table into allocated memory
 * @param fs File system pointer
 * @param zone_num Zone number to read
 * @param table_bytes Size of table to allocate and read
 * @param error_context Context string for error messages
 * @return Pointer to allocated table, or NULL on failure
 */
uint32_t *safe_read_zone_table(fs_t *fs, uint32_t zone_num,
                               size_t table_bytes,
                               const char *error_context)
{
  uint32_t *table = safe_malloc(table_bytes);
  if (!table)
  {
    return NULL;
  }

  off_t offset = zone_to_offset(fs, zone_num);
  if (offset < 0)
  {
    fprintf(stderr, "Invalid %s zone %u\n", error_context, zone_num);
    free(table);
    return NULL;
  }

  if (safe_fseeko(fs->img, offset, SEEK_SET) != 0)
  {
    fprintf(stderr, "fseeko (%s)\n", error_context);
    free(table);
    return NULL;
  }

  if (safe_fread(table, 1, table_bytes, fs->img) != 0)
  {
    fprintf(stderr, "fread (%s)\n", error_context);
    free(table);
    return NULL;
  }
  return table;
}

/**
 * @brief Process a range of zones, handling holes appropriately
 * @param fs File system pointer
 * @param zones Array of zone numbers (NULL for hole processing)
 * @param num_zones Number of zones to process
 * @param out Output file
 * @param state File read state
 * @return 0 on success, -1 on error
 */
int process_zone_range(fs_t *fs, uint32_t *zones, size_t num_zones,
                       FILE *out, file_read_state_t *state)
{
  size_t zone_bytes = fs_zone_bytes(fs);

  for (size_t i = 0; i < num_zones && state->remaining > 0; i++)
  {
    /* 0 for holes when zones is NULL */
    uint32_t zone = zones ? zones[i] : 0;
    size_t to_write = (state->remaining < zone_bytes) ? 
                                    state->remaining : zone_bytes;

    if (process_data(fs, zone, to_write, out, state) < 0)
    {
      return -1;
    }
  }
  return 0;
}

/**
 * @brief Deallocates the argument struct
 * @param args Argument struct pointer to be freed
 */
void free_args(args_struct_t *args)
{
  if (!args)
    return;
  switch (args->type)
  {
  case MINGET_TYPE:
    free(args->struct_var.minget_struct);
    break;
  case MINLS_TYPE:
    free(args->struct_var.minls_struct);
    break;
  default:
    break;
  }
  free(args);
}

/**
 * @brief Allocates an argument struct_var
 *
 * Depending on the program being ran, it allocates a new
 * argument struct and then calls allocate_struct() which
 * fills the struct with the paramters given to the function.
 *
 * @param argc Number of arguments in the argument array
 * @param argv pointer to the argument arrays from the command line
 * @return
 *
 */
args_struct_t *Getopts(int argc, char *argv[])
{
  /* Check what program is calling Getopts() */
  char *prog = argv[0];
  args_struct_t *args = safe_calloc(1, sizeof(*args));
  if (!args)
    return NULL;

  /* Allocate the correct struct depending on program being ran */
  if (strstr(prog, "minls"))
  {
    minls_input_t *minls_struct = safe_calloc(1, sizeof(*minls_struct));
    if (!minls_struct)
    {
      free(args);
      return NULL;
    }
    args->type = MINLS_TYPE;
    args->struct_var.minls_struct = minls_struct;
    minls_struct->part = -1;
    minls_struct->subpart = -1;
  }
  else if (strstr(prog, "minget"))
  {
    minget_input_t *minget_struct = safe_calloc(1, sizeof(*minget_struct));
    if (!minget_struct)
    {
      free(args);
      return NULL;
    }
    args->type = MINGET_TYPE;
    args->struct_var.minget_struct = minget_struct;
    minget_struct->part = -1;
    minget_struct->subpart = -1;
  }
  else
  {
    fprintf(stderr, "Unknown program name: %s\n", prog);
    free(args);
    return NULL;
  }
  /* Populate the args struct */
  if (allocate_struct(args, argc, argv) != 0)
  {
    free_args(args);
    return NULL;
  }
  return args;
}

/**
 * @brief Prints usage message for the given program type
 *
 * @param prog_name The program name from argv[0]
 * @param type The type of program (MINGET_TYPE or MINLS_TYPE)
 */
void print_usage(char *prog_name, struct_type type)
{
  if (type == MINGET_TYPE)
  {
    fprintf(stderr,
            "Usage: %s [ -v ] [ -p part [ -s subpart ] ] imagefile"
            " srcpath [ dstpath ]\n",
            prog_name);
  }
  else if (type == MINLS_TYPE)
  {
    fprintf(stderr,
            "Usage: %s [ -v ] [ -p part [ -s subpart ] ] imagefile"
            " [ path ]\n",
            prog_name);
  }
}

/**
 * @brief Processes common command line options
 *
 * @param opt The option character from getopt
 * @param optarg The option argument from getopt
 * @param prog_name The program name for usage messages
 * @param verbose Pointer to verbose flag
 * @param part Pointer to partition number
 * @param subpart Pointer to subpartition number
 * @param type Program type for usage messages
 * @return 0 on success, -1 on error
 */
int process_common_options(char opt, char *optarg, char *prog_name,
                           bool *verbose, int *part, int *subpart,
                           struct_type type)
{
  switch (opt)
  {
  case 'v':
    *verbose = true;
    break;
  case 'p':
    *part = atoi(optarg);
    break;
  case 's':
    *subpart = atoi(optarg);
    break;
  case 'h':
    print_usage(prog_name, type);
    return -1;
  default:
    print_usage(prog_name, type);
    return -1;
  }
  return 0;
}

/**
 * @brief Populates the argument struct
 *
 * This function will insert the arguments from argv[]
 * into the structs depending on what program that is being ran
 *
 * @param args Pointer to the argument struct
 * @param argc Number of arguments in the argument array
 * @param argv pointer to the argument arrays from the command line
 */
int allocate_struct(args_struct_t *args, int argc, char *argv[])
{
  struct_type type = args->type;
  int opt;

  switch (type)
  {
  case (MINGET_TYPE):
  {
    minget_input_t *m = args->struct_var.minget_struct;

    while ((opt = getopt(argc, argv, "hvp:s:")) != -1)
    {
      if (process_common_options(opt, optarg, argv[0], &m->verbose,
                                 &m->part, &m->subpart, MINGET_TYPE) != 0)
      {
        return -1;
      }
    }
    /* Ensure after the optional flags you have the necessary arguments */
    if (argc - optind < 2)
    {
      print_usage(argv[0], MINGET_TYPE);
      return -1;
    }

    /* If given the correct arguments, then save those into the struct */
    m->imgfile = argv[optind++];
    m->srcpath = argv[optind++];
    if (optind < argc)
    {
      m->dstpath = argv[optind];
    }
    break;
  }

  /* Similar for MINLS */
  case (MINLS_TYPE):
  {
    minls_input_t *l = args->struct_var.minls_struct;

    while ((opt = getopt(argc, argv, "hvp:s:")) != -1)
    {
      if (process_common_options(opt, optarg, argv[0], &l->verbose,
                                 &l->part, &l->subpart, MINLS_TYPE) != 0)
      {
        return -1;
      }
    }

    /* Ensure you have necessary number of args after the optional flags */
    if (argc - optind < 1)
    {
      print_usage(argv[0], MINLS_TYPE);
      return -1;
    }

    /* Get the final argument and save into the struct */
    l->imgfile = argv[optind++];

    /* If given a path, put into struct */
    if (optind < argc)
    {
      l->path = argv[optind];
    }
    else
    {
      l->path = "/";
    }
    break;
  }

  default:
    fprintf(stderr, "Unknown struct type in allocate_struct\n");
    return -1;
  }
  return 0;
}

/**
 * @brief Opens the image file
 *
 * Error checks the opening of a path using fopen()
 *
 * @param path The path for the file system image
 */
FILE *open_img(const char *path)
{
  FILE *f = fopen(path, "r");
  if (f == NULL)
  {
    perror(path);
    exit(EXIT_FAILURE);
  }
  return f;
}

/**
 * @brief Opens the file system and initializes its members
 *
 * Instantiates a filesystem struct and populates it with
 * the image as well as the offset for the struct.
 * @param path The path to the image to be opened
 */
fs_t fs_open(const char *path)
{
  fs_t fs;
  fs.img = open_img(path);
  fs.fs_start = 0;
  return fs;
}

/**
 * @brief Seeks, reads, then saves data to the partitions
 *
 * This function seeks to the correct location of the image based
 * on the offset, and then reads a sector of the image and saves it
 * into the partition table.
 *
 * After this, it ensures that the partition table contains the
 * correct signature. Finally, it writes to the partitions starting
 * at sector + 0x1BE as that is the offset to the first partition block.
 *
 * @param fs Pointer to the file system struct
 * @param offset Calculated based on the subpartitions (If any)
 * @param parts Partition table struct
 *
 */
int read_partition_table(fs_t *fs, off_t offset,
                         partition_table_entry_t parts[4])
{
  uint8_t sector[512];
  /* Seek to the correct offset based on the partition */
  if (safe_fseeko(fs->img, offset, SEEK_SET) != 0)
  {
    return -1;
  }

  /* Read a sector from the image */
  if (safe_fread(sector, 1, sizeof(sector), fs->img) != 0)
  {
    return -1;
  }
  /* Check MBR signature (bytes 510-511 = 0x55AA) */
  if (sector[510] != 0x55 || sector[511] != 0xAA)
  {
    fprintf(stderr, "Invalid partition table signature!");
    return -1;
  }
  /* Copy the partition entries into the partition structs */
  memcpy(parts, sector + 0x1BE, 4 * sizeof(partition_table_entry_t));
  return 0;
}

/**
 * @brief Reads the partition type and ensures it is a Minix parition
 *
 * Ensures the correct parition type and calculates the index into the
 * file system to get to the first parition block.
 *
 * @param index Used to get the correct partition entry
 * @param fs Pointer to file system struct
 * @param parts Partition structs
 */
int select_partition_table(int index, fs_t *fs,
                           partition_table_entry_t parts[4])
{
  if (index < 0)
  {
    fs->fs_start = 0;
    return 0;
  }
  if (index > 3)
  {
    fprintf(stderr, "Partition index (%d) not in range [0-3]!\n", index);
    return -1;
  }

  partition_table_entry_t *entry = &parts[index];

  /* Checks the parition type ensuring it is a MINIX partition */
  if (entry->type != 0x81)
  {
    fprintf(stderr, "Partition %d is not a Minix Partition!\n", index);
    return -1;
  }
  /* Multiply by 512 to get from sectors to bytes and calculate offset */
  fs->fs_start = (off_t)entry->lFirst * 512;
  return 0;
}

/**
 * @brief Reads the super block to ensure correct File System
 * @param fs Filesystem struct pointer
 * @return -1 on error, 0 on success
 */
int read_superblock(fs_t *fs)
{
  /* 1024 Byte offset from beginning of partition to get to superblock */
  off_t sb_offset = fs->fs_start + 1024;
  /* fseek to superblock */
  if (safe_fseeko(fs->img, sb_offset, SEEK_SET) != 0)
  {
    return -1;
  }

  /* Read from the image into the superblock */
  if (safe_fread(&fs->sb, sizeof(fs->sb), 1, fs->img) != 0)
  {
    return -1;
  }

  /* Ensure correct Magic number */
  if (fs->sb.magic != 0x4D5A)
  {
    fprintf(stderr,
            "Bad magic number.(0x%04x)\n"
            " This doesn't look like MINIX FS.\n",
            (unsigned)fs->sb.magic);
    return -1;
  }
  return 0;
}

/**
 * @brief Calculates inode offset, populates inode struct
 *
 * @param fs File system struct pointer
 * @param inum Used to calculate the inode offset
 * @param out The ouput inode struct pointer to read into
 *
 * @return 0 on success, -1 on error
 */
int fs_read_inode(fs_t *fs, uint32_t inum, inode_t *out)
{
  /* Check Inode number */
  if (inum < 1 || inum > fs->sb.ninodes)
  {
    fprintf(stderr,
            "Invalid inode number: %u, not in range (1..%u)\n",
            inum, fs->sb.ninodes);
    return -1;
  }
  /* Get the starting block offset of the inode table */
  off_t inode_table_block = 2 + fs->sb.i_blocks + fs->sb.z_blocks;

  /* Compute byte offset of the requested inode */
  off_t inode_offset = fs->fs_start +
                       inode_table_block * (off_t)fs->sb.blocksize +
                       (inum - 1) * (off_t)INODE_SIZE;

  if (safe_fseeko(fs->img, inode_offset, SEEK_SET) != 0)
  {
    return -1;
  }
  if (safe_fread(out, INODE_SIZE, 1, fs->img) != 0)
  {
    return -1;
  }
  return 0;
}

/**
 * @brief Calculates the offset based on the desired zone
 *
 * Takes a zone number and figures out where that zone starts in the
 * filesystem image. If Zone == 0, then it is a hole in the filesystem,
 * do nothing. Otherwise convert zone number to byte offset.
 *
 * @param fs Pointer to filesystem struct (for zone size info)
 * @param zone Zone number to calculate offset for
 * @return Byte offset of the zone, or -1 for holes/invalid zones
 */
off_t zone_to_offset(fs_t *fs, uint32_t zone)
{
  if (zone == 0)
  {
    return -1;
  }

  /* Zone size = blocksize * 2^log_zone_size */
  int blocks_per_zone = 1 << fs->sb.log_zone_size;
  off_t block_index = (off_t)zone * blocks_per_zone;
  return fs->fs_start + block_index * (off_t)fs->sb.blocksize;
}

/**
 * @brief Reads directory entries from a directory inode
 *
 * Takes a directory inode and reads all its entries into an array.
 * Has to handle direct zones and indirect zones, plus deal with holes.
 * The directory data gets parsed into proper directory entry structs.
 *
 * @param fs Pointer to the filesystem struct
 * @param dir_inode Pointer to the directory's inode
 * @param entries Array to store the directory entries in
 * @return Number of entries found, or -1 on error
 */
int fs_read_directory(fs_t *fs, inode_t *dir_inode,
                      minix_dir_entry *entries)
{
  if (!inode_is_directory(dir_inode))
  {
    fprintf(stderr, "fs_read_directory: inode is not a directory\n");
    return -1;
  }

  /* Calculate the remaining size to read */
  uint32_t remaining = dir_inode->size;
  if (remaining == 0)
  {
    return 0;
  }

  /* Get the blocks per zone, then calculate how many
     blocks total in that zone */
  int blocks_per_zone = 1 << fs->sb.log_zone_size;
  size_t zone_bytes = fs->sb.blocksize * blocks_per_zone;

  /* Allocate an inode amount of bytes */
  unsigned char *raw = safe_malloc(remaining);
  if (!raw)
  {
    return -1;
  }

  size_t buf_pos = 0;
  bool error = false;

  /* -------- Direct Zones --------- */
  for (int i = 0; i < 7 && remaining > 0 && !error; i++)
  {
    uint32_t z = dir_inode->zone[i];
    dir_process_zone(fs, z, raw, zone_bytes, &remaining, &buf_pos, &error);
  }

  /* --------- Indirect Zones ------ */
  if (!error && remaining > 0 && dir_inode->indirect != 0)
  {
    size_t zone_bytes = fs_zone_bytes(fs);
    size_t num_ptrs = fs_ptrs_per_block(fs);

    uint32_t *table = safe_malloc(zone_bytes);
    if (!table)
    {
      error = true;
    }
    else
    {
      off_t off = zone_to_offset(fs, dir_inode->indirect);
      if (off < 0)
      {
        fprintf(stderr, "Invalid dir indirect zone %u\n",
                dir_inode->indirect);
        error = true;
      }
      else if (fseeko(fs->img, off, SEEK_SET) != 0)
      {
        perror("fseeko (dir indirect)");
        error = true;
      }
      else if (fread(table, 1, zone_bytes, fs->img) != zone_bytes)
      {
        perror("fread (dir indirect)");
        error = true;
      }
      else
      {
        for (size_t i = 0; i < num_ptrs && remaining > 0 && !error; i++)
        {
          uint32_t z = table[i];
          dir_process_zone(fs, z, raw, zone_bytes, &remaining, &buf_pos,
                           &error);
        }
      }
      free(table);
    }
  }

  if (error)
  {
    free(raw);
    return -1;
  }

  /* ----- Parse raw bytes into directory entries ----- */
  int n_entries = dir_inode->size / DIR_ENTRY_SIZE; /* Max possible entries */
  int out_count = 0;

  for (int i = 0; i < n_entries; i++)
  {
    minix_dir_entry *de = (minix_dir_entry *)(raw + i * DIR_ENTRY_SIZE);

    if (de->inode == 0) /* Skip deleted entries */
      continue;

    entries[out_count++] = *de; /* Copy valid entry */
  }

  free(raw);
  return out_count;
}

/**
 * @brief Based on the mode, makes a string to be printed
 *
 * Converts the unix file mode bits into a human readable string.
 * First character shows file type (d for directory,
 * - for regular file), then 9 characters for read/write/execute permissions.
 *
 * @param mode The file mode bits from the inode
 * @param out Array to store the 11-character permissions array
 */
void mode_to_string(uint16_t mode, char out[11])
{
  uint16_t type = mode & 0170000;

  out[0] = (type == 0040000) ? 'd' : '-'; /* dir or regular/other */

  out[1] = (mode & 0400) ? 'r' : '-';
  out[2] = (mode & 0200) ? 'w' : '-';
  out[3] = (mode & 0100) ? 'x' : '-';

  out[4] = (mode & 0040) ? 'r' : '-';
  out[5] = (mode & 0020) ? 'w' : '-';
  out[6] = (mode & 0010) ? 'x' : '-';

  out[7] = (mode & 0004) ? 'r' : '-';
  out[8] = (mode & 0002) ? 'w' : '-';
  out[9] = (mode & 0001) ? 'x' : '-';

  out[10] = '\0';
}

/**
 * @brief Looks up a file or directory by path in the filesystem
 *
 * Takes a path and walks through the directory structure to find the
 * corresponding inode. Handles the root directory, then tokenizes the
 * path and looks up each component.
 *
 * @param fs Pointer to the filesystem struct
 * @param path Path to look up (can be relative or absolute)
 * @param out_inode Pointer to store the resulting inode
 * @param out_inum Pointer to store the resulting inode number
 * @return 0 on success, -1 if path not found
 */
int fs_lookup_path(fs_t *fs, const char *path, inode_t *out_inode,
                   uint32_t *out_inum)
{
  /* Root or empty path -> inum == 1 */
  if (!path || path[0] == '\0' || (strcmp(path, "/") == 0))
  {
    if (fs_read_inode(fs, 1, out_inode) != 0)
    {
      return -1;
    }
    /* INUMs start at 1 */
    if (out_inum)
    {
      *out_inum = 1;
    }
    return 0;
  }

  /* Duplicate the path as we are going to tokenize */
  char *dup = strdup(path);
  if (!dup)
  {
    perror("strdup");
    return -1;
  }

  /* Gets the current inode starting at the root directory */
  inode_t curr;
  if (fs_read_inode(fs, 1, &curr) != 0)
  {
    free(dup);
    return -1;
  }
  uint32_t current_inum = 1;

  char *p = dup;

  /* Skip leading slashes */
  while (*p == '/')
  {
    p++;
  }
  /* Tokenize the path, obtaining characters between '/' */
  char *token = strtok(p, "/");

  while (token != NULL)
  {
    /* Current must be a directory to descend further */
    if (!inode_is_directory(&curr))
    {
      free(dup);
      return -1;
    }

    /* Calculate the total amount of possible entires */
    int max_entries = curr.size / DIR_ENTRY_SIZE;
    minix_dir_entry *entries = safe_malloc(max_entries * sizeof(*entries));
    if (!entries)
    {
      free(dup);
      return -1;
    }

    /* Get the total amount of actual entries */
    int num_entries = fs_read_directory(fs, &curr, entries);
    if (num_entries < 0)
    {
      free(entries);
      free(dup);
      return -1;
    }

    uint32_t next_inum = 0;
    /* Search for matching filename in directory */
    for (int i = 0; i < num_entries; i++)
    {
      char name_buf[61];
      memcpy(name_buf, entries[i].name, 60); /* Ensure null termination */
      name_buf[60] = '\0';

      if (strcmp(name_buf, token) == 0)
      {
        next_inum = entries[i].inode;
        break;
      }
    }

    free(entries);

    if (next_inum == 0)
    {
      /* Component not found in this directory */
      free(dup);
      return -1;
    }

    /* Read the inode into curr from the file system and inode number */
    if (fs_read_inode(fs, next_inum, &curr) != 0)
    {
      free(dup);
      return -1;
    }

    current_inum = next_inum;
    token = strtok(NULL, "/");
  }

  /* Done walking the path */
  free(dup);

  if (out_inode)
  {
    /* Copy final inode struct out */
    *out_inode = curr;
  }
  if (out_inum)
  {
    /* Along with its inode number */
    *out_inum = current_inum;
  }
  return 0;
}

/**
 * @brief Checks Mode type and compares to see if the inode is a directory
 */
int inode_is_directory(inode_t *inode)
{
  uint16_t type = inode->mode & 0170000;
  return (type == 0040000);
}

/**
 * @brief Processes a single zone when reading directory contents
 * @param fs Filesystem structure pointer
 * @param zone Zone number to process
 * @param raw Buffer to write directory data
 * @param zone_bytes Size of each zone in bytes
 * @param remaining Pointer to remaining bytes to read
 * @param buf_pos Pointer to current buffer position
 * @param error Pointer to error flag
 */
void dir_process_zone(fs_t *fs, uint32_t zone, unsigned char *raw,
                      size_t zone_bytes, uint32_t *remaining,
                      size_t *buf_pos, bool *error)
{
  if (*error || *remaining == 0)
    return;

  size_t to_copy = (*remaining < zone_bytes) ? *remaining : zone_bytes;

  if (zone == 0)
  {
    /* Hole: fill this part of the directory with zeros */
    memset(raw + *buf_pos, 0, to_copy);
  }
  else
  {
    off_t off = zone_to_offset(fs, zone);
    if (off < 0)
    {
      fprintf(stderr, "Invalid directory zone %u\n", zone);
      *error = true;
      return;
    }

    if (fseeko(fs->img, off, SEEK_SET) != 0)
    {
      perror("fseeko (dir zone)");
      *error = true;
      return;
    }

    if (fread(raw + *buf_pos, 1, to_copy, fs->img) != to_copy)
    {
      perror("fread (dir zone)");
      *error = true;
      return;
    }
  }
  *buf_pos += to_copy;
  *remaining -= to_copy;
}

/**
 * @brief returns the amount of bytes in a zone
 */
size_t fs_zone_bytes(fs_t *fs)
{
  return (size_t)fs->sb.blocksize << fs->sb.log_zone_size;
}

/**
 * @brief Number of 32-bit zone pointers in one zone
 */
size_t fs_ptrs_per_block(fs_t *fs)
{
  return fs->sb.blocksize / sizeof(uint32_t);
}

/**
 * @brief Processes data from a zone and writes it to output file
 * @param fs Filesystem structure pointer
 * @param zone Zone number to process
 * @param to_write Number of bytes to write
 * @param out Output file pointer
 * @param state File reading state structure
 * @return 0 on success, -1 on error
 */
int process_data(fs_t *fs, uint32_t zone, size_t to_write, FILE *out,
                 file_read_state_t *state)
{
  /* Nothing to do */
  if (state->remaining == 0 || to_write == 0)
  {
    return 0;
  }

  /* Don’t write past end-of-file */
  if (to_write > state->remaining)
  {
    to_write = state->remaining;
  }

  size_t buf_size = fs->sb.blocksize;
  unsigned char buf[buf_size];

  /* If this is a real zone, seek to its start once before the loop */
  off_t off = -1;
  if (zone != 0)
  {
    off = zone_to_offset(fs, zone);
    if (off < 0)
    {
      fprintf(stderr, "Invalid data zone %u\n", zone);
      return -1;
    }
    if (fseeko(fs->img, off, SEEK_SET) != 0)
    {
      perror("fseeko (data zone)");
      return -1;
    }
  }

  /* Process data in chunks up to blocksize */
  while (to_write > 0)
  {
    size_t chunk = (to_write < buf_size) ? to_write : buf_size;

    if (zone == 0)
    {
      /* HOLE: fill buffer with zeros */
      memset(buf, 0, chunk);
    }
    else
    {
      /* REAL DATA: read from image into buf */
      if (fread(buf, 1, chunk, fs->img) != chunk)
      {
        perror("fread (data zone)");
        return -1;
      }
    }

    /* Write whatever is in buf (zeros or real data) to output */
    if (fwrite(buf, 1, chunk, out) != chunk)
    {
      perror("fwrite");
      return -1;
    }

    to_write -= chunk;
    state->remaining -= chunk;
    state->total_written += chunk;
  }
  return 0;
}

/**
 * @brief Gets the zones from the inode and processes their data
 * @param fs Filesystem structure pointer
 * @param inode Inode containing direct zones
 * @param out Output file pointer
 * @param state File reading state structure
 * @return 0 on success, -1 on error
 */
int read_direct_zones(fs_t *fs, inode_t *inode, FILE *out,
                      file_read_state_t *state)
{
  /* Process all direct zones using the helper function */
  return process_zone_range(fs, inode->zone, DIRECT_ZONES, out, state);
}

/**
 * @brief Reads file data through single indirect zone pointer
 * @param fs Filesystem structure pointer
 * @param inode Inode containing single indirect zone
 * @param out Output file pointer
 * @param state File reading state structure
 * @return 0 on success, -1 on error
 */
int read_single_indirect(fs_t *fs, inode_t *inode, FILE *out,
                         file_read_state_t *state)
{
  if (state->remaining == 0)
  {
    /* Nothing to read! */
    return 0;
  }

  size_t num_ptrs = fs_ptrs_per_block(fs);

  /* ------------------ Holes ----------------- */
  if (inode->indirect == 0)
  {
    return process_zone_range(fs, NULL, num_ptrs, out, state);
  }

  /* ---------- REAL single-indirect zone ------- */
  size_t table_bytes = fs->sb.blocksize;
  uint32_t *table = safe_read_zone_table(fs, inode->indirect,
                                         table_bytes, "single-indirect");
  if (!table)
  {
    return -1;
  }

  /* Now iterate through the pointers in the table and write to the outfile */
  int result = process_zone_range(fs, table, num_ptrs, out, state);
  free(table);
  return result;
}

/**
 * @brief Reads file data through double indirect zone pointer
 * @param fs Filesystem structure pointer
 * @param inode Inode containing double indirect zone
 * @param out Output file pointer
 * @param state File reading state structure
 * @return 0 on success, -1 on error
 */
int read_double_indirect(fs_t *fs, const inode_t *inode, FILE *out,
                         file_read_state_t *state)
{
  if (state->remaining == 0)
  {
    return 0;
  }

  size_t num_ptrs = fs_ptrs_per_block(fs);

  /* --------- No double-indirect zone: entire region is holes --------- */
  if (inode->two_indirect == 0)
  {
    for (size_t i = 0; i < num_ptrs && state->remaining > 0; i++)
    {
      if (process_zone_range(fs, NULL, num_ptrs, out, state) < 0)
      {
        return -1;
      }
    }
    return 0;
  }

  /* --------- Real double-indirect --------- */
  size_t table_bytes = fs->sb.blocksize;
  uint32_t *outer = safe_read_zone_table(fs, inode->two_indirect,
                                         table_bytes,
                                         "double-indirect outer");
  if (!outer)
  {
    return -1;
  }

  /* Process each pointer in outer table */
  for (size_t i = 0; i < num_ptrs && state->remaining > 0; i++)
  {
    uint32_t first_level_zone = outer[i];

    if (first_level_zone == 0)
    {
      /* This whole chunk (num_ptrs data zones) is holes */
      if (process_zone_range(fs, NULL, num_ptrs, out, state) < 0)
      {
        free(outer);
        return -1;
      }
      continue;
    }

    uint32_t *inner = safe_read_zone_table(fs, first_level_zone,
                                           table_bytes,
                                           "double-indirect inner");
    if (!inner)
    {
      free(outer);
      return -1;
    }

    if (process_zone_range(fs, inner, num_ptrs, out, state) < 0)
    {
      free(inner);
      free(outer);
      return -1;
    }
    free(inner);
  }
  free(outer);
  return 0;
}

/**
 * @brief Reads entire file content from inode to output stream
 * @param fs Filesystem structure pointer
 * @param inode Inode of the file to read
 * @param out Output file stream
 * @return Number of bytes written on success, -1 on error
 */
ssize_t fs_read_file(fs_t *fs, inode_t *inode, FILE *out)
{
  file_read_state_t st = {
      .remaining = inode->size,
      .total_written = 0,
  };

  if (st.remaining == 0)
  {
    return 0;
  }

  if (read_direct_zones(fs, inode, out, &st) < 0)
  {
    return -1;
  }
  if (st.remaining == 0)
  {
    return (ssize_t)st.total_written;
  }

  if (read_single_indirect(fs, inode, out, &st) < 0)
  {
    return -1;
  }
  if (st.remaining == 0)
  {
    return (ssize_t)st.total_written;
  }

  if (read_double_indirect(fs, inode, out, &st) < 0)
  {
    return -1;
  }

  return (ssize_t)st.total_written;
}

/**
 * @brief Checks if an inode represents a regular file
 * @param inode Inode structure to check
 * @return 1 if regular file, 0 otherwise
 */
int inode_is_regular(inode_t *inode)
{
  uint16_t type = inode->mode & 0170000;
  return (type == 0100000); /* regular file */
}

/**
 * @brief Prints superblock information for debugging
 * @param fs Filesystem structure pointer
 * @param label Label for debug output
 * @param verbose Whether to print verbose output
 */
void print_superblock(fs_t *fs, char *label, bool verbose)
{
  LOG(label, verbose, "Superblock:");
  LOG(label, verbose, "  ninodes       = %u", fs->sb.ninodes);
  LOG(label, verbose, "  i_blocks      = %d", fs->sb.i_blocks);
  LOG(label, verbose, "  z_blocks      = %d", fs->sb.z_blocks);
  LOG(label, verbose, "  firstdata     = %u", fs->sb.firstdata);
  LOG(label, verbose, "  log_zone_size = %d", fs->sb.log_zone_size);
  LOG(label, verbose, "  max_file      = %u", fs->sb.max_file);
  LOG(label, verbose, "  zones         = %u", fs->sb.zones);
  LOG(label, verbose, "  magic         = 0x%04x", fs->sb.magic);
  LOG(label, verbose, "  blocksize     = %u", fs->sb.blocksize);
  LOG(label, verbose, "  subversion    = %u", fs->sb.subversion);
}

/**
 * @brief Prints inode information for debugging
 * @param inode Inode structure to print
 * @param inum Inode number
 * @param label Label for debug output
 * @param verbose Whether to print verbose output
 */
void print_inode(inode_t *inode, uint32_t inum,
                 char *label, bool verbose)
{
  LOG(label, verbose, "Inode %u:", inum);
  LOG(label, verbose, "  mode      = 0%o", inode->mode);
  LOG(label, verbose, "  links     = %u", inode->links);
  LOG(label, verbose, "  uid       = %u", inode->uid);
  LOG(label, verbose, "  gid       = %u", inode->gid);
  LOG(label, verbose, "  size      = %u", inode->size);
  LOG(label, verbose, "  atime     = %u", inode->atime);
  LOG(label, verbose, "  mtime     = %u", inode->mtime);
  LOG(label, verbose, "  ctime     = %u", inode->ctime);

  for (int i = 0; i < DIRECT_ZONES; i++)
  {
    LOG(label, verbose, "  zone[%d]   = %u", i, inode->zone[i]);
  }

  LOG(label, verbose, "  indirect      = %u", inode->indirect);
  LOG(label, verbose, "  two_indirect  = %u", inode->two_indirect);
}

/**
 * @brief Prints partition table entry information for debugging
 * @param p Partition table entry to print
 * @param idx Partition index number
 * @param label Label for debug output
 * @param verbose Whether to print verbose output
 */
void print_part(partition_table_entry_t *p, int idx,
                char *label, bool verbose)
{
  LOG(label, verbose, "Partition %d:", idx);
  LOG(label, verbose, "  bootind = 0x%02x", p->bootind);
  LOG(label, verbose, "  type    = 0x%02x", p->type);
  LOG(label, verbose, "  lFirst  = %u", p->lFirst);
  LOG(label, verbose, "  size    = %u", p->size);
}
