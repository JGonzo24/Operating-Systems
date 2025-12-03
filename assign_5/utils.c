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
 * We log to stderr.
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
 * @return 0 on success, EXIT_FAILURE on error
 */
int safe_fseeko(FILE *stream, off_t offset, int whence)
{
  if (fseeko(stream, offset, whence) != 0)
  {
    perror("fseeko");
    return EXIT_FAILURE;
  }
  return 0;
}

/**
 * @brief Safe wrapper for fread with error handling
 * @param ptr Pointer to buffer to read into
 * @param size Size of each element
 * @param nitems Number of elements to read
 * @param stream File stream to read from
 * @return 0 on success, EXIT_FAILURE on error
 */
int safe_fread(void *ptr, size_t size, size_t nitems, FILE *stream)
{
  if (fread(ptr, size, nitems, stream) != nitems)
  {
    perror("fread");
    return EXIT_FAILURE;
  }
  return 0;
}

/**
 * @brief Safe wrapper for malloc with error handling
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure (after printing error)
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
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory, or NULL on failure (after printing error)
 */
void *safe_calloc(size_t nmemb, size_t size)
{
  void *ptr = calloc(nmemb, size);
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
uint32_t *safe_read_zone_table(fs_t *fs, uint32_t zone_num, size_t table_bytes, const char *error_context)
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
 * @param zone_count Number of zones to process
 * @param out Output file
 * @param state File read state
 * @return 0 on success, -1 on error
 */
int process_zone_range(fs_t *fs, uint32_t *zones, size_t zone_count, FILE *out, file_read_state_t *state)
{
  size_t zone_bytes = fs_zone_bytes(fs);

  for (size_t i = 0; i < zone_count && state->remaining > 0; i++)
  {
    uint32_t zone = zones ? zones[i] : 0; // 0 for holes when zones is NULL
    size_t to_write = (state->remaining < zone_bytes) ? state->remaining : zone_bytes;

    if (process_data(fs, zone, to_write, out, state) < 0)
    {
      return -1;
    }
  }
  return 0;
}

/**
 * @brief Deallocates the argument struct
 *
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
 * @return 0 on success, EXIT_FAILURE on error
 */
int process_common_options(char opt, char *optarg, char *prog_name,
                           bool *verbose, int *part, int *subpart, struct_type type)
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
    return EXIT_FAILURE;
  default:
    print_usage(prog_name, type);
    return EXIT_FAILURE;
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
        return EXIT_FAILURE;
      }
    }
    /* Ensure after the optional flags you have the necessary arguments */
    if (argc - optind < 2)
    {
      print_usage(argv[0], MINGET_TYPE);
      return EXIT_FAILURE;
    }

    /* If you do, then save those into the struct */
    m->imgfile = argv[optind++];
    m->srcpath = argv[optind++];
    if (optind < argc)
    {
      m->dstpath = argv[optind];
    }
    break;
  }

  /* Repeat for MINLS */
  case (MINLS_TYPE):
  {
    minls_input_t *l = args->struct_var.minls_struct;

    while ((opt = getopt(argc, argv, "hvp:s:")) != -1)
    {
      if (process_common_options(opt, optarg, argv[0], &l->verbose, &l->part, &l->subpart, MINLS_TYPE) != 0)
      {
        return EXIT_FAILURE;
      }
    }

    /* Ensure you have necessary number of args after teh optional flags */
    if (argc - optind < 1)
    {
      print_usage(argv[0], MINLS_TYPE);
      return EXIT_FAILURE;
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
    return EXIT_FAILURE;
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
FILE *open_img(char *path)
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
 * the image as well as the offset for the struct
 * @param path The path to the image to be opened
 */
fs_t fs_open(char *path)
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
    return EXIT_FAILURE;
  }

  /* Read a sector from the image */
  if (safe_fread(sector, 1, sizeof(sector), fs->img) != 0)
  {
    return EXIT_FAILURE;
  }
  /* Ensure that the partition table contains the right signature */
  if (sector[510] != 0x55 || sector[511] != 0xAA)
  {
    fprintf(stderr, "Invalid partition table signature!");
    return EXIT_FAILURE;
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
    return EXIT_FAILURE;
  }

  partition_table_entry_t *entry = &parts[index];

  /* Checks the Parition type ensuring it is a MINIX partition */
  if (entry->type != 0x81)
  {
    fprintf(stderr, "Partition %d is not a Minix Partition!\n", index);
    return EXIT_FAILURE;
  }
  /* Multiply by 512 to get from sectors-> bytes and calculate byte offset */
  fs->fs_start = (off_t)entry->lFirst * 512;
  return 0;
}

/**
 * @brief Reads the super block to ensure correct File System
 * @param fs Filesystem struct pointer
 */
int read_superblock(fs_t *fs)
{
  /* 1024 Byte offset from begninning of partition to get to the super block */
  off_t sb_offset = fs->fs_start + 1024;
  /* fseek to superblock */
  if (safe_fseeko(fs->img, sb_offset, SEEK_SET) != 0)
  {
    return EXIT_FAILURE;
  }

  /* Read from the image into the superblock */
  if (safe_fread(&fs->sb, sizeof(fs->sb), 1, fs->img) != 0)
  {
    return EXIT_FAILURE;
  }

  /* Ensure correct Magic number */
  if (fs->sb.magic != 0x4D5A)
  {
    fprintf(stderr,
            "Bad magic number.(0x%04x)\n This doesn't look like MINIX FS.\n",
            (unsigned)fs->sb.magic);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/**
 * @brief Calculates inode offset, populates inode struct
 *
 */
int fs_read_inode(fs_t *fs, uint32_t inum, inode_t *out)
{
  // Check Inode number
  if (inum < 1 || inum > fs->sb.ninodes)
  {
    fprintf(stderr, "Invalid inode number: %u, not in range (1..%u)\n", inum,
            fs->sb.ninodes);
    return -1;
  }
  // Get the starting block offset of the inode table
  off_t inode_table_block = 2 + fs->sb.i_blocks + fs->sb.z_blocks;

  // Compute byte offset of the requested inode
  off_t inode_offset = fs->fs_start +
                       inode_table_block * (off_t)fs->sb.blocksize +
                       (inum - 1) * (off_t)INODE_SIZE;

  if (safe_fseeko(fs->img, inode_offset, SEEK_SET) != 0)
  {
    return EXIT_FAILURE;
  }
  if (safe_fread(out, INODE_SIZE, 1, fs->img) != 0)
  {
    return EXIT_FAILURE;
  }
  return 0;
}

/**
 * @brief Calculates the offset based on the desired zone
 *
 * @param fs Pointer to fs to be used for the zone size
 * @param zone Used to calculate the block index
 */
off_t zone_to_offset(fs_t *fs, uint32_t zone)
{
  if (zone == 0)
  {
    return -1;
  }

  int blocks_per_zone = 1 << fs->sb.log_zone_size;
  off_t block_index = (off_t)zone * blocks_per_zone;
  return fs->fs_start + block_index * (off_t)fs->sb.blocksize;
}

/**
 * @brief
 *
 */
int fs_read_directory(fs_t *fs, inode_t *dir_inode, minix_dir_entry *entries)
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

  /* Allocate the an inode amount of bytes */
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
    size_t ptrs = fs_ptrs_per_block(fs);

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
        fprintf(stderr, "Invalid dir indirect zone %u\n", dir_inode->indirect);
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
        for (size_t i = 0; i < ptrs && remaining > 0 && !error; i++)
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

  /* ===== Parse raw bytes into directory entries ===== */
  int n_entries = dir_inode->size / DIR_ENTRY_SIZE;
  int out_count = 0;

  for (int i = 0; i < n_entries; i++)
  {
    minix_dir_entry *de = (minix_dir_entry *)(raw + i * DIR_ENTRY_SIZE);

    if (de->inode == 0)
      continue;

    entries[out_count++] = *de;
  }

  free(raw);
  return out_count;
}

/**
 * @brief Based on the mode, makes a string to be printed
 */
void mode_to_string(uint16_t mode, char out[11])
{
  uint16_t type = mode & 0170000;

  out[0] = (type == 0040000) ? 'd' : '-'; // dir or regular/other

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

int fs_lookup_path(fs_t *fs, const char *path, inode_t *out_inode,
                   uint32_t *out_inum)
{
  // Special case: root or empty path -> inode 1
  if (!path || path[0] == '\0' || (strcmp(path, "/") == 0))
  {
    if (fs_read_inode(fs, 1, out_inode) != 0)
    {
      return -1;
    }
    // INUMs start at 1
    if (out_inum)
    {
      *out_inum = 1;
    }
    return 0;
  }

  // Duplicate the path as we are going to tokenize
  char *dup = strdup(path);
  if (!dup)
  {
    perror("strdup");
    return -1;
  }

  // Gets the current inode
  inode_t curr;
  if (fs_read_inode(fs, 1, &curr) != 0)
  {
    free(dup);
    return -1;
  }
  uint32_t current_inum = 1;

  char *p = dup;

  // Skip leading slashes
  while (*p == '/')
  {
    p++;
  }
  // Tokenize the path, obtaining characters between '/'
  char *token = strtok(p, "/");

  while (token != NULL)
  {
    // Current must be a directory to descend further
    if (!inode_is_directory(&curr))
    {
      free(dup);
      return -1;
    }

    int max = curr.size / DIR_ENTRY_SIZE;
    minix_dir_entry *entries = safe_malloc(max * sizeof(*entries));
    if (!entries)
    {
      free(dup);
      return -1;
    }

    int count = fs_read_directory(fs, &curr, entries);
    if (count < 0)
    {
      free(entries);
      free(dup);
      return -1;
    }

    uint32_t next_inum = 0;
    for (int i = 0; i < count; i++)
    {
      char name_buf[61];
      memcpy(name_buf, entries[i].name, 60);
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
      // component not found in this directory
      free(dup);
      return -1;
    }

    if (fs_read_inode(fs, next_inum, &curr) != 0)
    {
      free(dup);
      return -1;
    }

    current_inum = next_inum;
    token = strtok(NULL, "/");
  }

  // Done walking the path
  free(dup);

  if (out_inode)
  {
    *out_inode = curr; // copy final inode struct out
  }
  if (out_inum)
  {
    *out_inum = current_inum; // and its inode number
  }

  return 0;
}

/**
 * @brief Checks Mode type and compares to see if the inode contains a directory
 */
int inode_is_directory(inode_t *inode)
{
  uint16_t type = inode->mode & 0170000;
  return (type == 0040000);
}

void dir_process_zone(fs_t *fs, uint32_t zone, unsigned char *raw,
                      size_t zone_bytes, uint32_t *remaining, size_t *buf_pos,
                      bool *error)
{
  if (*error || *remaining == 0)
    return;

  size_t to_copy = (*remaining < zone_bytes) ? *remaining : zone_bytes;

  if (zone == 0)
  {
    // Hole: fill this part of the directory with zeros
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

  /* Single loop: handle hole vs real zone *inside* */
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
 * @brief Gets the zone from the inode, processes its data
 *
 *
 *
 */
int read_direct_zones(fs_t *fs, inode_t *inode, FILE *out,
                      file_read_state_t *state)
{
  // Process all direct zones using the helper function
  return process_zone_range(fs, inode->zone, DIRECT_ZONES, out, state);
}

int read_single_indirect(fs_t *fs, inode_t *inode, FILE *out,
                         file_read_state_t *state)
{
  if (state->remaining == 0)
  {
    return 0; // Nothing to read!
  }

  size_t ptrs = fs_ptrs_per_block(fs);

  // ------------------ Holes -----------------//
  if (inode->indirect == 0)
  {
    return process_zone_range(fs, NULL, ptrs, out, state);
  }

  // ---------- REAL single-indirect zone ------- //
  size_t table_bytes = fs->sb.blocksize;
  uint32_t *table = safe_read_zone_table(fs, inode->indirect, table_bytes, "single-indirect");
  if (!table)
  {
    return -1;
  }

  // Now iterate through the pointers in the table and write to the outfile
  int result = process_zone_range(fs, table, ptrs, out, state);
  free(table);
  return result;
}

int read_double_indirect(fs_t *fs, const inode_t *inode, FILE *out,
                         file_read_state_t *state)
{
  if (state->remaining == 0)
  {
    return 0;
  }

  size_t ptrs = fs_ptrs_per_block(fs);

  /* --------- No double-indirect zone: entire region is holes --------- */
  if (inode->two_indirect == 0)
  {
    for (size_t i = 0; i < ptrs && state->remaining > 0; i++)
    {
      if (process_zone_range(fs, NULL, ptrs, out, state) < 0)
      {
        return -1;
      }
    }
    return 0;
  }

  /* --------- Real double-indirect --------- */
  size_t table_bytes = fs->sb.blocksize;
  uint32_t *outer = safe_read_zone_table(fs, inode->two_indirect, table_bytes, "double-indirect outer");
  if (!outer)
  {
    return -1;
  }

  for (size_t i = 0; i < ptrs && state->remaining > 0; i++)
  {
    uint32_t first_level_zone = outer[i];

    if (first_level_zone == 0)
    {
      /* This whole chunk (ptrs data zones) is holes. */
      if (process_zone_range(fs, NULL, ptrs, out, state) < 0)
      {
        free(outer);
        return -1;
      }
      continue;
    }

    uint32_t *inner = safe_read_zone_table(fs, first_level_zone, table_bytes, "double-indirect inner");
    if (!inner)
    {
      free(outer);
      return -1;
    }

    if (process_zone_range(fs, inner, ptrs, out, state) < 0)
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

int inode_is_regular(inode_t *inode)
{
  uint16_t type = inode->mode & 0170000;
  return (type == 0100000); // regular file
}

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

void print_part(partition_table_entry_t *p, int idx,
                char *label, bool verbose)
{
  LOG(label, verbose, "Partition %d:", idx);
  LOG(label, verbose, "  bootind = 0x%02x", p->bootind);
  LOG(label, verbose, "  type    = 0x%02x", p->type);
  LOG(label, verbose, "  lFirst  = %u", p->lFirst);
  LOG(label, verbose, "  size    = %u", p->size);
}
