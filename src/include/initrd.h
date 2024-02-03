#ifndef INITRD_H
#define INITRD_H

#include <stdint.h>
#include <fs.h>

typedef struct {
    uint32_t nfiles; // The number of files in the ramdisk.
} initrd_header_t;

typedef struct {
    uint8_t magic; // Magic number, for error checking.
    int8_t name[64]; // Filename.
    uint32_t offset; // Offset in the initrd that the file starts.
    uint32_t length; // Length of the file.
} initrd_file_header_t;

fs_node_t* initialise_initrd(uint32_t location);

#endif