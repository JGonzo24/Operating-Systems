
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    args_struct_t *args = Getopts(argc, argv);
    if (!args) return EXIT_FAILURE;

    minget_input_t *m = args->struct_var.minget_struct;
    fs_t fs = fs_open(m->imgfile);

    int p_index = m->part;

    if (p_index >= 0)
    {
        partition_table_entry_t parts[4];

        if (read_partition_table(&fs, 0, parts) != 0)
        {
            fprintf(stderr, "Error in read_partition_table()!\n");
            exit(EXIT_FAILURE);
        }
        if (select_partition_table(p_index, &fs, parts) != 0)
        {
            fprintf(stderr, "Error in select_partition_table()!\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (m->subpart >= 0)
    {
        fprintf(stderr, "-s given without -p\n");
        exit(EXIT_FAILURE);
    }

    if (m->subpart >= 0)
    {
        partition_table_entry_t subparts[4];
        int s_index = m->subpart;

        if (read_partition_table(&fs, fs.fs_start, subparts) != 0)
        {
            fprintf(stderr, "Error in read_partition_table()!\n");
            exit(EXIT_FAILURE);
        }
        if (select_partition_table(s_index, &fs, subparts) != 0)
        {
            fprintf(stderr, "Error in select_partition_table()!\n");
            exit(EXIT_FAILURE);
        }
    }

    if (read_superblock(&fs) != 0)
    {
        fprintf(stderr, "Error in read_superblock()!\n");
        exit(EXIT_FAILURE);
    }

    // Use fs_lookup_path on srcpath
    inode_t file_inode;
    uint32_t file_inum;

    if (fs_lookup_path(&fs, m->srcpath, &file_inode, &file_inum) != 0)
    {
        fprintf(stderr, "Path not found: %s\n", m->srcpath);
        exit(EXIT_FAILURE);
    }
    // ADD THIS:
    fprintf(stderr, "Raw inode bytes for size field:\n");
    unsigned char *size_bytes = (unsigned char *)&file_inode.size;
    for (int i = 0; i < 4; i++) {
        fprintf(stderr, "  byte[%d] = 0x%02x\n", i, size_bytes[i]);
    }
    fprintf(stderr, "Interpreted as uint32_t: %u (0x%x)\n", file_inode.size, file_inode.size);

    // In minget.c, after fs_lookup_path, add this:
    fprintf(stderr, "DEBUG: file size = %u (0x%x)\n", file_inode.size, file_inode.size);
    fprintf(stderr, "DEBUG: direct zones: ");
    for (int i = 0; i < 7; i++) {
        fprintf(stderr, "%u ", file_inode.zone[i]);
    }
    fprintf(stderr, "\nDEBUG: indirect = %u, two_indirect = %u\n", 
            file_inode.indirect, file_inode.two_indirect);

    if (inode_is_directory(&file_inode))
    {
        fprintf(stderr, "%s is a directory\n", m->srcpath);
        exit(EXIT_FAILURE);
    }

    FILE *out = stdout;
    if (m->dstpath)
    {
        out = fopen(m->dstpath, "w");
        if (!out) 
        {
            perror(m->dstpath);
            exit(EXIT_FAILURE);
        }
    }

    if (fs_read_file(&fs, &file_inode, out) < 0) 
    {
        fprintf(stderr, "Error reading file data\n");
        if (out != stdout) fclose(out);
        free_args(args);
        fclose(fs.img);
        return EXIT_FAILURE;
    }

    if (out != stdout) 
    {
        fclose(out);
    }

    free_args(args);
    fclose(fs.img);
    return 0;
}



