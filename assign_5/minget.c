#define _POSIX_C_SOURCE 200809L /* must be before any #include */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

/*
 * Handle partition setup - primary and subpartition if needed
 * Returns 0 on success, -1 on failure
 */
int setup_partitions(fs_t *fs, minget_input_t *mi)
{
  partition_table_entry_t parts[4];

  /* Skip partition handling if no partition specified */
  if (mi->part < 0)
  {
    return 0;
  }

  /* Read and process primary partition */
  if (read_partition_table(fs, 0, parts) != 0)
  {
    fprintf(stderr, "minget: failed to read primary partition table\n");
    return -1;
  }

  /* Show partition info if we're being verbose */
  for (int i = 0; i < 4; i++)
  {
    print_part(&parts[i], i, "minget", mi->verbose);
  }

  if (select_partition_table(mi->part, fs, parts) != 0)
  {
    return -1;
  }

  /* Handle subpartition if they want one */
  if (mi->subpart >= 0)
  {
    if (read_partition_table(fs, fs->fs_start, parts) != 0)
    {
      fprintf(stderr, "minget: failed to read subpartition table\n");
      return -1;
    }

    if (select_partition_table(mi->subpart, fs, parts) != 0)
    {
      return -1;
    }
  }

  return 0;
}

/*
 * Find the file we want and validate it's actually a regular file
 * Returns 0 on success, -1 on failure
 */
int find_and_validate_file(fs_t *fs, minget_input_t *mi, inode_t *inode, uint32_t *inum)
{
  /* Try to find the file they asked for */
  if (fs_lookup_path(fs, mi->srcpath, inode, inum) != 0)
  {
    fprintf(stderr, "minget: cannot find path '%s'\n", mi->srcpath);
    return -1;
  }

  /* Make sure it's not a directory - we can't copy those */
  if (inode_is_directory(inode))
  {
    fprintf(stderr, "minget: '%s' is a directory\n", mi->srcpath);
    return -1;
  }

  /* And make sure it's actually a regular file */
  if (!inode_is_regular(inode))
  {
    fprintf(stderr, "minget: '%s' is not a regular file\n", mi->srcpath);
    return -1;
  }

  return 0;
}

/*
 * Open the output file - either stdout or the destination they gave us
 * Returns file pointer on success, NULL on failure
 */
FILE *open_output_file(minget_input_t *mi)
{
  FILE *out = stdout;

  /* If they specified a destination file, open it for writing */
  if (mi->dstpath != NULL)
  {
    out = fopen(mi->dstpath, "wb");
    if (out == NULL)
    {
      perror("fopen dstpath");
      return NULL;
    }
  }

  return out;
}

/*
 * Copy the file data to the output destination
 * Returns 0 on success, -1 on failure
 */
int copy_file_data(fs_t *fs, inode_t *inode, FILE *out, const char *srcpath)
{
  ssize_t written = fs_read_file(fs, inode, out);
  if (written < 0)
  {
    fprintf(stderr, "minget: error reading file '%s'\n", srcpath);
    return -1;
  }
  return 0;
}

/*
 * Clean up resources and close files
 */
void cleanup_resources(fs_t *fs, args_struct_t *args, FILE *out)
{
  if (out != stdout && out != NULL)
  {
    fclose(out);
  }

  if (fs->img != NULL)
  {
    fclose(fs->img);
  }

  if (args != NULL)
  {
    free_args(args);
  }
}

/*
 * minget:
 *   minget [ -v ] [ -p part [ -s subpart ] ] imagefile srcpath [ dstpath ]
 *
 * Copies the data from the given sourcepath to the desired destination path.
 * If there's no destination path given, then copy to stdout
 */
int main(int argc, char *argv[])
{
  /* Parse command line arguments */
  args_struct_t *args = Getopts(argc, argv);
  if (args == NULL)
  {
    /* Getopts already printed a usage message on error */
    return EXIT_FAILURE;
  }

  /* Sanity check - make sure we got the right kind of args */
  if (args->type != MINGET_TYPE)
  {
    fprintf(stderr, "minget: internal error: wrong args type\n");
    cleanup_resources(NULL, args, NULL);
    return EXIT_FAILURE;
  }

  minget_input_t *mi = args->struct_var.minget_struct;

  /* Open the file system image */
  fs_t fs = fs_open(mi->imgfile);
  if (fs.img == NULL)
  {
    cleanup_resources(NULL, args, NULL);
    return EXIT_FAILURE;
  }

  /* Handle partition setup if needed */
  if (setup_partitions(&fs, mi) != 0)
  {
    cleanup_resources(&fs, args, NULL);
    return EXIT_FAILURE;
  }

  /* Read the superblock to understand the file system */
  if (read_superblock(&fs) != 0)
  {
    cleanup_resources(&fs, args, NULL);
    return EXIT_FAILURE;
  }

  /* Show superblock info if we're being verbose */
  print_superblock(&fs, "minget", mi->verbose);

  /* Find the file and make sure it's valid */
  inode_t inode;
  uint32_t inum = 0;
  if (find_and_validate_file(&fs, mi, &inode, &inum) != 0)
  {
    cleanup_resources(&fs, args, NULL);
    return EXIT_FAILURE;
  }

  /* Open the output destination */
  FILE *out = open_output_file(mi);
  if (out == NULL)
  {
    cleanup_resources(&fs, args, NULL);
    return EXIT_FAILURE;
  }

  /* Actually copy the file data */
  if (copy_file_data(&fs, &inode, out, mi->srcpath) != 0)
  {
    cleanup_resources(&fs, args, out);
    return EXIT_FAILURE;
  }

  /* All done - clean up and exit */
  cleanup_resources(&fs, args, out);
  return EXIT_SUCCESS;
}
