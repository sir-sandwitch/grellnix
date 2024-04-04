#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <isr.h>

typedef struct page
{
    uint32_t present    : 1;    // Page present in memory
    uint32_t rw         : 1;    // Read-only if clear, readwrite if set
    uint32_t user       : 1;    // Supervisor level only if clear
    uint32_t accessed   : 1;    // Has the page been accessed since last refresh?
    uint32_t dirty      : 1;    // Has the page been written to since last refresh?
    uint32_t unused     : 7;    // Amalgamation of unused and reserved bits
    uint32_t frame      : 20;   // Frame address (shifted right 12 bits)
} page_t;

typedef struct page_table
{
    page_t pages[1024];
} page_table_t;

typedef struct page_directory
{
    page_table_t *tables[1024]; // Array of pointers to pagetables
    uint32_t tablesPhysical[1024]; // Array of pointers to pagetables above, but gives their *physical* location, for loading into the CR3 register.
    uint32_t physicalAddr; // The physical address of tablesPhysical. This comes into play when we get our kernel heap allocated and the directory may be in a different location in virtual memory.
} page_directory_t;

// Sets up the environment, page directories etc and enables paging.
extern void initialise_paging();

// Causes the specified page directory to be loaded into the CR3 register.
extern void switch_page_directory(page_directory_t *new);

// Retrieves a pointer to the page required.
extern page_t *get_page(uint32_t address, int make, page_directory_t *dir);

// Handler for page faults.
extern void page_fault(registers_t regs);

extern void alloc_frame(page_t *page, int is_kernel, int is_writeable);

extern void free_frame(page_t *page);

extern void alloc_new_page_table(page_directory_t *dir, int is_user, int is_program);

extern page_directory_t *clone_directory(page_directory_t *src);

extern page_directory_t *kernel_directory;
extern page_directory_t *current_directory;

#endif // PAGING_H