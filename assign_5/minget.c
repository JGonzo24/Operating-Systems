#define _POSIX_C_SOURCE 200809L /* must be before any #include */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

/*
 * minget:
 *   minget [ -v ] [ -p part [ -s subpart ] ] imagefile srcpath [ dstpath ]
 *
 * - imagefile: path to the MINIX filesystem image
 * - srcpath:   absolute path inside the MINIX filesystem
 * - dstpath:   optional; if absent, write to stdout
 *
 * All low-level filesystem logic lives in utils.c:
 *   - partition selection
 *   - superblock reading
 *   - path lookup
 *   - inode reading
 *   - data / hole handling (fs_read_file)
 */

int main(int argc, char *argv[]) {
  /* --------------------------------------------------
   * 1. Parse command-line arguments
   * -------------------------------------------------- */
  args_struct_t *args = Getopts(argc, argv);
  if (args == NULL) {
    /* Getopts already printed a usage message on error */
    return EXIT_FAILURE;
  }

  if (args->type != MINGET_TYPE) {
    fprintf(stderr, "minget: internal error: wrong args type\n");
    free_args(args);
    return EXIT_FAILURE;
  }

  minget_input_t *mi = args->struct_var.minget_struct;

  /* --------------------------------------------------
   * 2. Open the filesystem image
   * -------------------------------------------------- */
  fs_t fs = fs_open(mi->imgfile);
  if (fs.img == NULL) {
    /* fs_open already printed an error via perror */
    free_args(args);
    return EXIT_FAILURE;
  }

  /* --------------------------------------------------
   * 3. Handle primary partition (-p) and subpartition (-s)
   * -------------------------------------------------- */
  partition_table_entry_t parts[4];

  /* Primary partition selection (if requested) */
  if (mi->part >= 0) {
    if (read_partition_table(&fs, 0, parts) != 0) {
      fprintf(stderr, "minget: failed to read primary partition table\n");
      fclose(fs.img);
      free_args(args);
      return EXIT_FAILURE;
    }

    if (select_partition_table(mi->part, &fs, parts) != 0) {
      /* select_partition_table prints its own error message */
      fclose(fs.img);
      free_args(args);
      return EXIT_FAILURE;
    }

    /* Subpartition selection (if requested) */
    if (mi->subpart >= 0) {
      if (read_partition_table(&fs, fs.fs_start, parts) != 0) {
        fprintf(stderr, "minget: failed to read subpartition table\n");
        fclose(fs.img);
        free_args(args);
        return EXIT_FAILURE;
      }

      if (select_partition_table(mi->subpart, &fs, parts) != 0) {
        fclose(fs.img);
        free_args(args);
        return EXIT_FAILURE;
      }
    }
  }

  /* --------------------------------------------------
   * 4. Read superblock
   * -------------------------------------------------- */
  if (read_superblock(&fs) != 0) {
    /* read_superblock prints a descriptive error */
    fclose(fs.img);
    free_args(args);
    return EXIT_FAILURE;
  }

  if (mi->verbose) {
    fprintf(
        stderr,
        "Superblock: ninodes=%u, zones=%u, blocksize=%u, log_zone_size=%d\n",
        fs.sb.ninodes, fs.sb.zones, fs.sb.blocksize, fs.sb.log_zone_size);
  }

  /* --------------------------------------------------
   * 5. Resolve srcpath to an inode
   * -------------------------------------------------- */
  inode_t inode;
  uint32_t inum = 0;

  if (fs_lookup_path(&fs, mi->srcpath, &inode, &inum) != 0) {
    fprintf(stderr, "minget: cannot find path '%s'\n", mi->srcpath);
    fclose(fs.img);
    free_args(args);
    return EXIT_FAILURE;
  }

  if (mi->verbose) {
    fprintf(stderr, "minget: path '%s' -> inode %u, size %u bytes\n",
            mi->srcpath, inum, inode.size);
  }

  /* --------------------------------------------------
   * 6. Make sure it is not a directory
   * -------------------------------------------------- */
  if (inode_is_directory(&inode)) {
    fprintf(stderr, "minget: '%s' is a directory\n", mi->srcpath);
    fclose(fs.img);
    free_args(args);
    return EXIT_FAILURE;
  }

  if (!inode_is_regular(&inode)) {
    fprintf(stderr, "minget: '%s' is not a regular file\n", mi->srcpath);
    fclose(fs.img);
    free_args(args);
    return EXIT_FAILURE;
  }

  /* --------------------------------------------------
   * 7. Open output destination
   * -------------------------------------------------- */
  FILE *out = stdout;

  if (mi->dstpath != NULL) {
    out = fopen(mi->dstpath, "wb");
    if (out == NULL) {
      perror("fopen dstpath");
      fclose(fs.img);
      free_args(args);
      return EXIT_FAILURE;
    }

    if (mi->verbose) {
      fprintf(stderr, "minget: writing output to '%s'\n", mi->dstpath);
    }
  } else if (mi->verbose) {
    fprintf(stderr, "minget: writing output to stdout\n");
  }

  /* --------------------------------------------------
   * 8. Read file contents and write to out
   * -------------------------------------------------- */
  ssize_t written = fs_read_file(&fs, &inode, out);
  if (written < 0) {
    fprintf(stderr, "minget: error reading file '%s'\n", mi->srcpath);
    if (out != stdout) {
      fclose(out);
    }
    fclose(fs.img);
    free_args(args);
    return EXIT_FAILURE;
  }

  if (mi->verbose) {
    fprintf(stderr, "minget: successfully wrote %zd bytes\n", written);
  }

  /* --------------------------------------------------
   * 9. Cleanup
   * -------------------------------------------------- */
  if (out != stdout) {
    fclose(out);
  }

  fclose(fs.img);
  free_args(args);

  return EXIT_SUCCESS;
}
