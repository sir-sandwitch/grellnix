#ifndef FS_H
#define FS_H
#include <stdint.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_PIPE        0x05
#define FS_SYMLINK     0x06
#define FS_MOUNTPOINT  0x08 // Is the file an active mountpoint?

typedef uint32_t (*read_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef void (*open_type_t)(struct fs_node*);
typedef void (*close_type_t)(struct fs_node*);
typedef struct dirent* (*readdir_type_t)(struct fs_node*, uint32_t);
typedef struct fs_node* (*finddir_type_t)(struct fs_node*, char* name);

struct dirent // One of these is returned by the readdir call, according to POSIX.
{
  char name[128]; // Filename.
  uint32_t ino;     // Inode number. Required by POSIX.
}; 

typedef struct fs_node {
    uint32_t mask; // Permission mask
    uint32_t uid; // User ID
    uint32_t gid; // Group ID
    uint32_t flags; // Flags
    uint32_t inode; // Inode number
    uint32_t length; // Size of file
    uint32_t impl; // Implementation defined number
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    readdir_type_t readdir;
    finddir_type_t finddir;
    struct fs_node* ptr; // Used by mountpoints and symlinks
} fs_node_t;

extern fs_node_t* fs_root; // The root of the filesystem.

extern uint32_t read_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
extern uint32_t write_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
extern void open_fs(fs_node_t* node, uint8_t read, uint8_t write);
extern void close_fs(fs_node_t* node);
extern struct dirent* readdir_fs(fs_node_t* node, uint32_t index);
extern fs_node_t* finddir_fs(fs_node_t* node, char* name);

#endif