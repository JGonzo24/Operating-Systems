#include "utils.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

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
 * @brief allocate the desired struct
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

FILE *open_img(const char *path) {
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    perror(path);
    exit(EXIT_FAILURE);
  }
  return f;
}

fs_t fs_open(const char *path) {
  fs_t fs;
  fs.img = open_img(path);
  fs.fs_start = 0;
  return fs;
}

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

off_t zone_to_offset(fs_t *fs, uint32_t zone) {
  if (zone == 0) {
    return -1;
  }

  int blocks_per_zone = 1 << fs->sb.log_zone_size;
  off_t block_index = (off_t)zone * blocks_per_zone;
  return fs->fs_start + block_index * (off_t)fs->sb.blocksize;
}

int fs_read_directory(fs_t *fs, inode_t *dir_inode, minix_dir_entry *entries) {
  off_t off = zone_to_offset(fs, dir_inode->zone[0]);
  if (off < 0) {
    fprintf(stderr, "Zone is 0!\n");
    return -1;
  }

  if (fseeko(fs->img, off, SEEK_SET) != 0) {
    perror("fseeko");
    return -1;
  }
  size_t dir_size = dir_inode->size;
  unsigned char *raw = malloc(dir_size);
  if (!raw) {
    perror("malloc");
    return -1;
  }

  if (fread(raw, 1, dir_size, fs->img) != dir_size) {
    perror("fread");
    free(raw);
    return -1;
  }

  int n_entries = dir_size / DIR_ENTRY_SIZE;
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

int inode_is_directory(inode_t *inode) {
  uint16_t type = inode->mode & 0170000;
  return (type == 0040000);
}

ssize_t fs_read_file_direct(fs_t *fs, inode_t *inode, FILE *out) {
  uint32_t remaining = inode->size;
  int blocks_per_zone = 1 << fs->sb.log_zone_size;
  size_t zone_bytes = fs->sb.blocksize * blocks_per_zone;

  unsigned char *buf = malloc(zone_bytes);
  if (!buf) {
    perror("malloc");
    return -1;
  }

  ssize_t total_written = 0;

  for (int i = 0; i < 7 && remaining > 0; i++) {
    uint32_t z = inode->zone[i];
    if (z == 0) {
      break;
    }
    off_t offset = zone_to_offset(fs, z);
    if (offset < 0) {
      break;
    }

    if (fseeko(fs->img, offset, SEEK_SET) != 0) {
      perror("fseeko");
      break;
    }

    size_t to_read = remaining < zone_bytes ? remaining : zone_bytes;

    if (fread(buf, 1, to_read, fs->img) != to_read) {
      perror("fread");
      break;
    }

    if (fwrite(buf, 1, to_read, out) != to_read) {
      perror("fwrite");
      break;
    }

    remaining -= to_read;
    total_written += to_read;
  }
  free(buf);
  return total_written;
}

ssize_t fs_read_file(fs_t *fs, inode_t *inode, FILE *out) {
  uint32_t remaining = inode->size;
  bool error = false;
  if (remaining == 0) {
    return 0;
  }

  int blocks_per_zone = 1 << fs->sb.log_zone_size;
  size_t zone_bytes = fs->sb.blocksize * blocks_per_zone;

  unsigned char *buffer = malloc(zone_bytes);
  if (!buffer) {
    perror("malloc");
    error = true;
    return -1;
  }

  ssize_t total_written = 0;

  // 1. Direct zones
  for (int i = 0; i < 7 && remaining > 0 && !error; i++) {
    uint32_t zone = inode->zone[i];
    process_zone(fs, zone, buffer, zone_bytes, &remaining, out, &total_written,
                 &error);
  }

  // 2. Indirect zones
  if (!error && remaining > 0 && inode->indirect != 0) {
    uint32_t *entries = malloc(zone_bytes);
    if (!entries) {
      perror("malloc (indirect entries)");
      error = true;
    } else {
      off_t offset = zone_to_offset(fs, inode->indirect);
      if (offset < 0) {
        fprintf(stderr, "Invalid indeirect zone %u\n", inode->indirect);
        error = true;
      } else if (fseeko(fs->img, offset, SEEK_SET) != 0) {
        perror("fseeko (indirect)");
        error = true;
      } else if (fread(entries, 1, zone_bytes, fs->img) != zone_bytes) {
        perror("fread (indirect)");
        error = true;
      } else {
        int n_entries = zone_bytes / sizeof(uint32_t);
        for (int i = 0; i < n_entries && remaining > 0 && !error; i++) {
          process_zone(fs, entries[i], buffer, zone_bytes, &remaining, out,
                       &total_written, &error);
        }
      }
      free(entries);
    }
  }

  // 3. Double indirect zones
  if (!error && remaining > 0 && inode->two_indirect != 0) {
    // L1 table: array of zone numbers (each points to an L2 table)
    uint32_t *l1 = malloc(zone_bytes);
    if (!l1) {
      perror("malloc (two_indirect L1)");
      error = true;
    } else {
      off_t l1_off = zone_to_offset(fs, inode->two_indirect);
      if (l1_off < 0) {
        fprintf(stderr, "Invalid two_indirect zone %u\n", inode->two_indirect);
        error = true;
      } else if (fseeko(fs->img, l1_off, SEEK_SET) != 0) {
        perror("fseeko (two_indirect L1)");
        error = true;
      } else if (fread(l1, 1, zone_bytes, fs->img) != zone_bytes) {
        perror("fread (two_indirect L1)");
        error = true;
      } else {
        int entries_per_zone = zone_bytes / sizeof(uint32_t);

        for (int i = 0; i < entries_per_zone && remaining > 0 && !error; i++) {
          uint32_t l1_zone = l1[i];

          if (l1_zone == 0) {
            // This entire L2 chunk is holes: simulate entries_per_zone
            for (int j = 0; j < entries_per_zone && remaining > 0 && !error;
                 j++) {
              process_zone(fs, 0, buffer, zone_bytes, &remaining, out,
                           &total_written, &error);
            }
            continue;
          }

          // Allocate buffer for L2 table
          uint32_t *l2 = malloc(zone_bytes);
          if (!l2) {
            perror("malloc (two_indirect L2)");
            error = true;
            break;
          }

          off_t l2_off = zone_to_offset(fs, l1_zone);
          if (l2_off < 0) {
            fprintf(stderr, "Invalid L2 zone %u in two_indirect\n", l1_zone);
            error = true;
            free(l2);
            break;
          } else if (fseeko(fs->img, l2_off, SEEK_SET) != 0) {
            perror("fseeko (two_indirect L2)");
            error = true;
            free(l2);
            break;
          } else if (fread(l2, 1, zone_bytes, fs->img) != zone_bytes) {
            perror("fread (two_indirect L2)");
            error = true;
            free(l2);
            break;
          } else {
            // Now l2[j] are data zone numbers
            for (int j = 0; j < entries_per_zone && remaining > 0 && !error;
                 j++) {
              process_zone(fs, l2[j], buffer, zone_bytes, &remaining, out,
                           &total_written, &error);
            }
          }
          free(l2);
        } // end L1 loop
      } // end L1 read ok
      free(l1);
    } // end l1 malloc ok
  }

  free(buffer);
  return error ? -1 : total_written;
}

void process_zone(fs_t *fs, uint32_t zone, unsigned char *buf,
                  size_t zone_bytes, uint32_t *remaining, FILE *out,
                  ssize_t *total_written, bool *error) {
  if (*error || *remaining == 0)
    return;

  size_t to_write = (*remaining < zone_bytes) ? *remaining : zone_bytes;

  if (zone == 0) {
    // Hole: write zeros
    memset(buf, 0, to_write);
  } else {
    off_t offset = zone_to_offset(fs, zone);
    if (offset < 0) {
      fprintf(stderr, "Invalid zone %u\n", zone);
      *error = true;
      return;
    }

    if (fseeko(fs->img, offset, SEEK_SET) != 0) {
      perror("fseeko");
      *error = true;
      return;
    }

    if (fread(buf, 1, to_write, fs->img) != to_write) {
      perror("fread");
      *error = true;
      return;
    }
  }

  if (fwrite(buf, 1, to_write, out) != to_write) {
    perror("fwrite");
    *error = true;
    return;
  }

  *remaining -= to_write;
  *total_written += to_write;
}
