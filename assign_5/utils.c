#define _POSIX_C_SOURCE 200809L /* must be before any #include */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

/**
 * @brief Deallocates the argument struct
 */
void free_args(args_struct_t *args) {
  if (!args)
    return;
  switch (args->type) {
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
 */
args_struct_t *Getopts(int argc, char *argv[]) {
  // Check what program is calling Getopts()
  char *prog = argv[0];
  args_struct_t *args = calloc(1, sizeof(*args));
  if (!args)
    return NULL;

  if (strstr(prog, "minls")) {
    minls_input_t *minls_struct = calloc(1, sizeof(*minls_struct));
    args->type = MINLS_TYPE;
    args->struct_var.minls_struct = minls_struct;
    minls_struct->part = -1;
    minls_struct->subpart = -1;
  } else if (strstr(prog, "minget")) {
    minget_input_t *minget_struct =
        (minget_input_t *)calloc(1, sizeof(*minget_struct));
    args->type = MINGET_TYPE;
    args->struct_var.minget_struct = minget_struct;
    minget_struct->part = -1;
    minget_struct->subpart = -1;
  } else {
    fprintf(stderr, "Unknown program name: %s\n", prog);
    free(args);
    return NULL;
  }
  if (allocate_struct(args, argc, argv) != 0) {
    free_args(args);
    return NULL;
  }
  return args;
}

/**
 * @brief Populates the argument struct
 *
 * This function will insert the arguments from argv[]
 * into the structs depending on what program that is being ran
 *
 */
int allocate_struct(args_struct_t *args, int argc, char *argv[]) {
  struct_type type = args->type;
  int opt;

  switch (type) {
  case (MINGET_TYPE): {
    minget_input_t *m = args->struct_var.minget_struct;

    while ((opt = getopt(argc, argv, "hvp:s:")) != -1) {
      switch (opt) {
      case 'v':
        m->verbose = true;
        break;
      case 'p':
        m->part = atoi(optarg);
        break;
      case 's':
        m->subpart = atoi(optarg);
        break;
      case 'h':
        fprintf(stderr,
                "Usage: %s [ -v ] [ -p part [ -s subpart ] ] imagefile"
                " srcpath [ dstpath ]\n",
                argv[0]);
        return EXIT_FAILURE;
        break;
      default:
        fprintf(stderr,
                "Usage: %s [ -v ] [ -p part [ -s subpart ] ] imagefile"
                " srcpath [ dstpath ]\n",
                argv[0]);
        return EXIT_FAILURE;
      }
    }

    if (argc - optind < 2) {
      fprintf(stderr,
              "Usage: %s [ -v ] [ -p part [ -s subpart ] ] imagefile"
              " srcpath [ dstpath ]\n",
              argv[0]);
      return EXIT_FAILURE;
    }

    m->imgfile = argv[optind++];
    m->srcpath = argv[optind++];
    if (optind < argc) {
      m->dstpath = argv[optind];
    }
    break;
  }

  case (MINLS_TYPE): {
    minls_input_t *l = args->struct_var.minls_struct;

    while ((opt = getopt(argc, argv, "hvp:s:")) != -1) {
      switch (opt) {
      case 'h':
        fprintf(stderr,
                "Usage: %s [ -v ] [ -p part [ -s subpart ] ] imagefile"
                " srcpath [ dstpath ]\n",
                argv[0]);
        return EXIT_FAILURE;
        break;
      case 'v':
        l->verbose = true;
        break;
      case 'p':
        l->part = atoi(optarg);
        break;
      case 's':
        l->subpart = atoi(optarg);
        break;
      default:
        fprintf(stderr,
                "Usage: %s [ -v ] [ -p part [ -s subpart ] ] imagefile"
                " [ path ]\n",
                argv[0]);
        return EXIT_FAILURE;
      }
    }

    if (argc - optind < 1) {
      fprintf(stderr,
              "Usage: %s [ -v ] [ -p part [ -s subpart ] ] imagefile"
              " [ path ]\n",
              argv[0]);
      return EXIT_FAILURE;
    }

    // Get the last args
    l->imgfile = argv[optind++];
    if (optind < argc) {
      l->path = argv[optind];
    } else {
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
 */
FILE *open_img(const char *path) {
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    perror(path);
    exit(EXIT_FAILURE);
  }
  return f;
}

/**
 * @brief Opens the file system and initializes its members
 */
fs_t fs_open(const char *path) {
  fs_t fs;
  fs.img = open_img(path);
  fs.fs_start = 0;
  return fs;
}

/*
 * @brief Uses fseek() and fread() to ensure a valid partition table
 *
 */
int read_partition_table(fs_t *fs, off_t offset,
                         partition_table_entry_t parts[4]) {
  uint8_t sector[512];
  if (fseek(fs->img, offset, SEEK_SET) != 0) {
    perror("fseek");
    return EXIT_FAILURE;
  }

  if (fread(sector, 1, sizeof(sector), fs->img) != sizeof(sector)) {
    perror("fread");
    return EXIT_FAILURE;
  }
  if (sector[510] != 0x55 || sector[511] != 0xAA) {
    fprintf(stderr, "Invalid partition table signature!");
    return EXIT_FAILURE;
  }

  // Copy the entries starting at the offset at 0x1BE
  memcpy(parts, sector + 0x1BE, 4 * sizeof(partition_table_entry_t));
  return 0;
}

/*
 * @brief Reads the partition type and ensures it is a Minix parition
 *
 * Ensures the correct parition type and calculates the index into the
 * file system to get to the first parition block.
 */

int select_partition_table(int index, fs_t *fs,
                           partition_table_entry_t parts[4]) {
  if (index < 0) {
    fs->fs_start = 0;
    return 0;
  }
  if (index > 3) {
    fprintf(stderr, "Partition index (%d) not in range [0-3]!\n", index);
    return EXIT_FAILURE;
  }

  partition_table_entry_t *entry = &parts[index];

  if (entry->type != 0x81) {
    fprintf(stderr, "Partition %d is not a Minix Partition!\n", index);
    return EXIT_FAILURE;
  }

  fs->fs_start = (off_t)entry->lFirst * 512;
  return 0;
}

/*
 * @brief Reads the super block to ensure correct File System
 */
int read_superblock(fs_t *fs) {
  off_t sb_offset = fs->fs_start + 1024;
  if (fseeko(fs->img, sb_offset, SEEK_SET) != 0) {
    perror("fseeko");
    return EXIT_FAILURE;
  }

  if (fread(&fs->sb, sizeof(fs->sb), 1, fs->img) != 1) {
    perror("fread");
    return EXIT_FAILURE;
  }

  if (fs->sb.magic != 0x4D5A) {
    fprintf(stderr,
            "Bad magic number. (0x%04x)\n This doesn't look like MINIX FS.\n",
            (unsigned)fs->sb.magic);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/*
 * @brief Reads the inode,
 */
int fs_read_inode(fs_t *fs, uint32_t inum, inode_t *out) {
  // Check Inode number
  if (inum < 1 || inum > fs->sb.ninodes) {
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

  if (fseeko(fs->img, inode_offset, SEEK_SET) < 0) {
    perror("fseeko");
    return EXIT_FAILURE;
  }
  if (fread(out, INODE_SIZE, 1, fs->img) != 1) {
    perror("fread inode");
    return EXIT_FAILURE;
  }
  return 0;
}

/*
 * @brief Calculates the offset based on the desired zone
 */
off_t zone_to_offset(fs_t *fs, uint32_t zone) {
  if (zone == 0) {
    return -1;
  }

  int blocks_per_zone = 1 << fs->sb.log_zone_size;
  off_t block_index = (off_t)zone * blocks_per_zone;
  return fs->fs_start + block_index * (off_t)fs->sb.blocksize;
}

/*
 * @brief
 *
 */
int fs_read_directory(fs_t *fs, inode_t *dir_inode, minix_dir_entry *entries) {
  if (!inode_is_directory(dir_inode)) {
    fprintf(stderr, "fs_read_directory: inode is not a directory\n");
    return -1;
  }

  size_t block_bytes = fs->sb.blocksize;

  uint32_t remaining = dir_inode->size;
  if (remaining == 0) {
    return 0;
  }

  int blocks_per_zone = 1 << fs->sb.log_zone_size;
  size_t zone_bytes = fs->sb.blocksize * blocks_per_zone;

  unsigned char *raw = malloc(remaining);
  if (!raw) {
    perror("malloc (directory raw)");
    return -1;
  }

  size_t buf_pos = 0;
  bool error = false;

  /* =========================
   * 1. DIRECT ZONES (zone[0..6])
   * ========================= */
  for (int i = 0; i < 7 && remaining > 0 && !error; i++) {
    uint32_t z = dir_inode->zone[i];
    dir_process_zone(fs, z, raw, zone_bytes, &remaining, &buf_pos, &error);
  }

  /* =========================
   * 2. INDIRECT ZONE (dir_inode->indirect)
   * ========================= */
  if (!error && remaining > 0 && dir_inode->indirect != 0) {
    uint32_t *table = malloc(block_bytes);
    if (!table) {
      perror("malloc (dir indirect table)");
      error = true;
    } else {
      off_t off = zone_to_offset(fs, dir_inode->indirect);
      if (off < 0) {
        fprintf(stderr, "Invalid dir indirect zone %u\n", dir_inode->indirect);
        error = true;
      } else if (fseeko(fs->img, off, SEEK_SET) != 0) {
        perror("fseeko (dir indirect)");
        error = true;
      } else if (fread(table, 1, block_bytes, fs->img) != block_bytes) {
        perror("fread (dir indirect)");
        error = true;
      } else {
        int n_entries = block_bytes / sizeof(uint32_t);
        for (int i = 0; i < n_entries && remaining > 0 && !error; i++) {
          uint32_t z = table[i];
          dir_process_zone(fs, z, raw, zone_bytes, &remaining, &buf_pos,
                           &error);
        }
      }
      free(table);
    }
  }

  if (error) {
    free(raw);
    return -1;
  }

  /* ===== Parse raw bytes into directory entries ===== */
  int n_entries = dir_inode->size / DIR_ENTRY_SIZE;
  int out_count = 0;

  for (int i = 0; i < n_entries; i++) {
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
void mode_to_string(uint16_t mode, char out[11]) {
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
                   uint32_t *out_inum) {
  // Special case: root or empty path -> inode 1
  if (!path || path[0] == '\0' || (strcmp(path, "/") == 0)) {
    if (fs_read_inode(fs, 1, out_inode) != 0) {
      return -1;
    }
    if (out_inum) {
      *out_inum = 1;
    }
    return 0;
  }

  char *dup = strdup(path);
  if (!dup) {
    perror("strdup");
    return -1;
  }

  inode_t curr;
  if (fs_read_inode(fs, 1, &curr) != 0) {
    free(dup);
    return -1;
  }
  uint32_t current_inum = 1;

  char *p = dup;

  // Skip leading slashes
  while (*p == '/') {
    p++;
  }

  char *token = strtok(p, "/");

  while (token != NULL) {
    // Current must be a directory to descend further
    if (!inode_is_directory(&curr)) {
      free(dup);
      return -1;
    }

    int max = curr.size / DIR_ENTRY_SIZE;
    minix_dir_entry *entries = malloc(max * sizeof(*entries));
    if (!entries) {
      perror("malloc");
      free(dup);
      return -1;
    }

    int count = fs_read_directory(fs, &curr, entries);
    if (count < 0) {
      free(entries);
      free(dup);
      return -1;
    }

    uint32_t next_inum = 0;
    for (int i = 0; i < count; i++) {
      char name_buf[61];
      memcpy(name_buf, entries[i].name, 60);
      name_buf[60] = '\0';

      if (strcmp(name_buf, token) == 0) {
        next_inum = entries[i].inode;
        break;
      }
    }

    free(entries);

    if (next_inum == 0) {
      // component not found in this directory
      free(dup);
      return -1;
    }

    if (fs_read_inode(fs, next_inum, &curr) != 0) {
      free(dup);
      return -1;
    }

    current_inum = next_inum;
    token = strtok(NULL, "/");
  }

  // Done walking the path
  free(dup);

  if (out_inode) {
    *out_inode = curr; // copy final inode struct out
  }
  if (out_inum) {
    *out_inum = current_inum; // and its inode number
  }

  return 0;
}

/**
 * @brief Checks Mode type and compares to see if the inode contains a directory
 */
int inode_is_directory(inode_t *inode) {
  uint16_t type = inode->mode & 0170000;
  return (type == 0040000);
}

void dir_process_zone(fs_t *fs, uint32_t zone, unsigned char *raw,
                      size_t zone_bytes, uint32_t *remaining, size_t *buf_pos,
                      bool *error) {
  if (*error || *remaining == 0)
    return;

  size_t to_copy = (*remaining < zone_bytes) ? *remaining : zone_bytes;

  if (zone == 0) {
    // Hole: fill this part of the directory with zeros
    memset(raw + *buf_pos, 0, to_copy);
  } else {
    off_t off = zone_to_offset(fs, zone);
    if (off < 0) {
      fprintf(stderr, "Invalid directory zone %u\n", zone);
      *error = true;
      return;
    }

    if (fseeko(fs->img, off, SEEK_SET) != 0) {
      perror("fseeko (dir zone)");
      *error = true;
      return;
    }

    if (fread(raw + *buf_pos, 1, to_copy, fs->img) != to_copy) {
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
size_t fs_zone_bytes(fs_t *fs) {
  return (size_t)fs->sb.blocksize << fs->sb.log_zone_size;
}

/**
 * @brief The table of zone pointers is blocksize bytes long
 */
size_t fs_ptrs_per_zone(fs_t *fs) {
  return (size_t)fs->sb.blocksize / sizeof(uint32_t);
}

int process_data(fs_t *fs, uint32_t zone, size_t to_write, FILE *out,
                 file_read_state_t *state) {
  if (state->remaining == 0 || to_write == 0) {
    fprintf(stderr, "No more remaining or no to_write!\n");
    return 0;
  }

  if (to_write > state->remaining) {
    to_write = state->remaining;
  }

  size_t buf_size = fs->sb.blocksize;
  unsigned char *buf[buf_size];

  // ------------------ I/O Portion -------------------//
  while (to_write > 0) {
    size_t chunk = (to_write < buf_size) ? to_write : buf_size;
    // Write zeros if holes
    if (zone == 0) {
      memset(buf, 0, chunk);
    }
    // Real zone
    else {
      if (fread(buf, 1, chunk, fs->img) != chunk) {
        perror("fread");
        return -1;
      }
    }

    // Write either zeros or actual data to out
    if (fwrite(buf, 1, chunk, out) != chunk) {
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
                      file_read_state_t *state) {
  size_t zone_bytes = fs_zone_bytes(fs);

  // for every zone, process the zone's data from the inode
  for (int i = 0; i < DIRECT_ZONES && state->remaining > 0; i++) {
    uint32_t z = inode->zone[i];
    size_t to_write =
        (state->remaining < zone_bytes) ? state->remaining : zone_bytes;

    if (process_data(fs, z, to_write, out, state) < 0) {
      fprintf(stderr, "Error in process_data (read_direct_zones)\n");
      return -1;
    }
  }
  return 0;
}

int read_single_indirect(fs_t *fs, inode_t *inode, FILE *out,
                         file_read_state_t *state) {
  if (state->remaining == 0) {
    return 0; // Nothing to read!
  }

  size_t zone_bytes = fs_zone_bytes(fs);
  size_t ptrs = fs_ptrs_per_zone(fs);

  // ------------------ Holes -----------------//
  if (inode->indirect == 0) {
    for (size_t i = 0; i < ptrs && state->remaining > 0; i++) {
      size_t to_write =
          (state->remaining < zone_bytes) ? state->remaining : zone_bytes;

      // Process this data
      if (process_data(fs, 0, to_write, out, state) < 0) {
        fprintf(stderr, "Error: Process_data (read_single_indirect)\n");
        return -1;
      }
    }
    return 0;
  }

  // ---------- REAL single-indirect zone ------- //
  size_t table_bytes = ptrs * sizeof(uint32_t);
  uint32_t *table = malloc(table_bytes);
  if (!table) {
    perror("malloc");
    return -1;
  }

  off_t offset = zone_to_offset(fs, inode->indirect);

  if (offset < 0) {
    fprintf(stderr, "Invalid indirect zone %u\n", inode->indirect);
    free(table);
    return -1;
  }

  // Now read from the image to save it into the table
  if (fseeko(fs->img, offset, SEEK_SET) != 0) {
    perror("fseeko (single-indirect)");
    free(table);
    return -1;
  }

  if (fread(table, 1, table_bytes, fs->img) != table_bytes) {
    perror("fread (single-indirect)");
    free(table);
    return -1;
  }
  
  // Now iterate through the pointers in the table and write to the outfile
  for (size_t i = 0; i < ptrs && state->remaining > 0; i++)
  {
    uint32_t zone = table[i];
    size_t to_write =
       (state->remaining < zone_bytes) ? state->remaining : zone_bytes; 
    if (process_data(fs, zone, to_write, out, state) < 0)
    {
      free(table);
      return -1;
    }
  }
  free(table);
  return 0;
}


int read_double_indirect(fs_t *fs,
                                const inode_t *inode,
                                FILE *out,
                                file_read_state_t *state)
{
  if (state->remaining == 0) {
    return 0;
  }

  size_t zone_bytes = fs_zone_bytes(fs);
  size_t ptrs       = fs_ptrs_per_zone(fs);

  /* --------- No double-indirect zone: entire region is holes --------- */
  if (inode->two_indirect == 0) {
    for (size_t i = 0; i < ptrs && state->remaining > 0; i++) {
      for (size_t j = 0; j < ptrs && state->remaining > 0; j++) {
        size_t to_write =
            (state->remaining < zone_bytes) ? state->remaining : zone_bytes;
        if (process_data(fs, 0, to_write, out, state) < 0) {
          return -1;
        }
      }
    }
    return 0;
  }

  /* --------- Real double-indirect --------- */

  size_t table_bytes = ptrs * sizeof(uint32_t);

  uint32_t *outer = malloc(table_bytes);
  if (!outer) {
    perror("malloc (double-indirect outer)");
    return -1;
  }

  off_t off = zone_to_offset(fs, inode->two_indirect);
  if (off < 0) {
    fprintf(stderr, "Invalid double-indirect zone %u\n", inode->two_indirect);
    free(outer);
    return -1;
  }

  if (fseeko(fs->img, off, SEEK_SET) != 0) {
    perror("fseeko (double-indirect outer)");
    free(outer);
    return -1;
  }

  if (fread(outer, 1, table_bytes, fs->img) != table_bytes) {
    perror("fread (double-indirect outer)");
    free(outer);
    return -1;
  }

  uint32_t *inner = malloc(table_bytes);
  if (!inner) {
    perror("malloc (double-indirect inner)");
    free(outer);
    return -1;
  }

  for (size_t i = 0; i < ptrs && state->remaining > 0; i++) {
    uint32_t first_level_zone = outer[i];

    if (first_level_zone == 0) {
      /* This whole chunk (ptrs data zones) is holes. */
      for (size_t j = 0; j < ptrs && state->remaining > 0; j++) {
        size_t to_write =
            (state->remaining < zone_bytes) ? state->remaining : zone_bytes;
        if (process_data(fs, 0, to_write, out, state) < 0) {
          free(inner);
          free(outer);
          return -1;
        }
      }
      continue;
    }

    off_t off2 = zone_to_offset(fs, first_level_zone);
    if (off2 < 0) {
      fprintf(stderr, "Invalid 2nd-level indirect zone %u\n",
              first_level_zone);
      free(inner);
      free(outer);
      return -1;
    }

    if (fseeko(fs->img, off2, SEEK_SET) != 0) {
      perror("fseeko (double-indirect inner)");
      free(inner);
      free(outer);
      return -1;
    }

    if (fread(inner, 1, table_bytes, fs->img) != table_bytes) {
      perror("fread (double-indirect inner)");
      free(inner);
      free(outer);
      return -1;
    }

    for (size_t j = 0; j < ptrs && state->remaining > 0; j++) {
      uint32_t data_zone = inner[j];
      size_t to_write =
          (state->remaining < zone_bytes) ? state->remaining : zone_bytes;
      if (process_data(fs, data_zone, to_write, out, state) < 0) {
        free(inner);
        free(outer);
        return -1;
      }
    }
  }

  free(inner);
  free(outer);
  return 0;
}

ssize_t fs_read_file(fs_t *fs, inode_t *inode, FILE *out)
{
  file_read_state_t st = {
    .remaining     = inode->size,
    .total_written = 0,
  };

  if (st.remaining == 0) {
    return 0;
  }

  if (read_direct_zones(fs, inode, out, &st) < 0) {
    return -1;
  }
  if (st.remaining == 0) {
    return (ssize_t)st.total_written;
  }

  if (read_single_indirect(fs, inode, out, &st) < 0) {
    return -1;
  }
  if (st.remaining == 0) {
    return (ssize_t)st.total_written;
  }

  if (read_double_indirect(fs, inode, out, &st) < 0) {
    return -1;
  }

  return (ssize_t)st.total_written;
}
