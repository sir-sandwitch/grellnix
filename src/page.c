#include <paging.h>
#include <kheap.h>
#include <common.h>

page_directory_t page_directory __attribute__((aligned(4096)));
page_table_t kernel_page_table __attribute__((aligned(4096)));
// page_table_t user_page_table __attribute__((aligned(4096)));
page_table_t kernel_program_page_table __attribute__((aligned(4096)));
// page_table_t user_program_page_table __attribute__((aligned(4096)));

extern void load_page_directory(uint32_t*);
extern void copy_page_physical(uint32_t, uint32_t);

// The kernel's page directory
page_directory_t *master_kernel_directory=0;

// The current page directory;
page_directory_t *current_directory=0;

//the page directory that gets used by the kernel
page_directory_t *kernel_directory=0;

// bitset of frames - used or free
uint32_t *frames;
uint32_t nframes;

uint32_t npageDirs = 1;
uint32_t npageTables = 2;

// Defined in kheap.c
extern uint32_t placement_address;
extern heap_t *kheap;
extern heap_t *uheap;

// Macros used in the bitset algorithms.
#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))

// Static function to set a bit in the frames bitset
static void set_frame(uint32_t frame_addr)
{
    uint32_t frame = frame_addr/0x1000;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    frames[idx] |= (0x1 << off);
}

// Static function to clear a bit in the frames bitset
static void clear_frame(uint32_t frame_addr)
{
    uint32_t frame = frame_addr/0x1000;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    frames[idx] &= ~(0x1 << off);
}

// Static function to test if a bit is set.
static uint32_t test_frame(uint32_t frame_addr)
{
    uint32_t frame = frame_addr/0x1000;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    return (frames[idx] & (0x1 << off));
}

// Static function to find the first free frame.
static uint32_t first_frame()
{
    uint32_t i, j;
    for (i = 0; i < INDEX_FROM_BIT(nframes); i++)
    {
        if (frames[i] != 0xFFFFFFFF) // nothing free, exit early.
        {
            // at least one bit is free here.
            for (j = 0; j < 32; j++)
            {
                uint32_t toTest = 0x1 << j;
                if ( !(frames[i]&toTest) )
                {
                    return i*4*8+j;
                }
            }
        }
    }
    return (uint32_t)-1;
}

// Function to allocate a frame.
void alloc_frame(page_t *page, int is_kernel, int is_writeable)
{
    if (page->frame != 0)
    {
        return; // Frame was already allocated, return straight away.
    }
    else
    {
        uint32_t idx = first_frame(); // idx is now the index of the first free frame.
        if (idx == (uint32_t)-1)
        {
            // PANIC is just a macro that prints a message to the screen then hits an infinite loop.
            PANIC("No free frames!");
        }
        set_frame(idx*0x1000); // this frame is now ours!
        page->present = 1; // Mark it as present.
        page->rw = (is_writeable)?1:0; // Should the page be writeable?
        page->user = (is_kernel)?0:1; // Should the page be user-mode?
        page->frame = idx;
    }
}

// Function to deallocate a frame.
void free_frame(page_t *page)
{
    uint32_t frame;
    if (!(frame=page->frame))
    {
        return; // The given page didn't actually have an allocated frame!
    }
    else
    {
        clear_frame(frame); // Frame is now free again.
        page->frame = 0x0; // Page now doesn't have a frame.
    }
}

void initialise_paging()
{
    // The size of physical memory. For the moment we
    // assume it is 16MB big.
    uint32_t mem_end_page = 0x1000000;
    
    // initialise frames to be all free
    nframes = mem_end_page / 0x1000;
    frames = (uint32_t*)kmalloc(INDEX_FROM_BIT(nframes));
    memset(frames, 0, INDEX_FROM_BIT(nframes));

    // set entries of page directory to be kernel page table and not present
    uint32_t i;
    for (i = 0; i < 1024; i++)
    {
        // This sets the following flags to the pages:
        //   Supervisor: Only kernel-mode can access them
        //   Write Enabled: It can be both read from and written to
        //   Not Present: The page table is not present
        page_directory.tables[i] = 0;
        page_directory.tablesPhysical[i] = 0x00000002;
    }

    // monitor_printf("page_directory.tables: %x\n", page_directory.tables);

    // Attributes: supervisor level, read/write, present
    uint32_t j;
    for (j = 0; j < 1024; j++)
    {
        // This sets the following flags to the pages:
        //   Supervisor: Only kernel-mode can access them
        //   Write Enabled: It can be both read from and written to
        //   Not Present: The page table is not present
        kernel_page_table.pages[j].frame = j;
        kernel_page_table.pages[j].present = 1;
        kernel_page_table.pages[j].rw = 1;
        kernel_page_table.pages[j].user = 0;
        kernel_page_table.pages[j].accessed = 0;
        kernel_page_table.pages[j].dirty = 0;
        kernel_page_table.pages[j].unused = 0;

        // user_page_table.pages[j].frame = j;
        // user_page_table.pages[j].present = 1;
        // user_page_table.pages[j].rw = 1;
        // user_page_table.pages[j].user = 1;
        // user_page_table.pages[j].accessed = 0;
        // user_page_table.pages[j].dirty = 0;
        // user_page_table.pages[j].unused = 0;

        kernel_program_page_table.pages[j].frame = j;
        kernel_program_page_table.pages[j].present = 1;
        kernel_program_page_table.pages[j].rw = 1;
        kernel_program_page_table.pages[j].user = 0;
        kernel_program_page_table.pages[j].accessed = 0;
        kernel_program_page_table.pages[j].dirty = 0;
        kernel_program_page_table.pages[j].unused = 0;

        // user_program_page_table.pages[j].frame = j;
        // user_program_page_table.pages[j].present = 1;
        // user_program_page_table.pages[j].rw = 1;
        // user_program_page_table.pages[j].user = 1;
        // user_program_page_table.pages[j].accessed = 0;
        // user_program_page_table.pages[j].dirty = 0;
        // user_program_page_table.pages[j].unused = 0;
    }

    // monitor_printf("kernel_page_table.pages: %x\n", kernel_page_table.pages);

    // Put the page table in the page directory
    // Attributes: supervisor level, read/write, present
    page_directory.tables[0] = &kernel_page_table;
    page_directory.tablesPhysical[0] = ((uint32_t)&kernel_page_table) | 3;
    page_directory.tables[1] = &kernel_program_page_table;
    page_directory.tablesPhysical[1] = ((uint32_t)&kernel_program_page_table) | 3;
    // page_directory.tables[2] = &user_page_table;
    // page_directory.tablesPhysical[2] = ((uint32_t)&user_page_table) | 7;
    // page_directory.tables[3] = &user_program_page_table;
    // page_directory.tablesPhysical[3] = ((uint32_t)&user_program_page_table) | 7;

    // monitor_printf("page_directory.tables: %x\n", page_directory.tables);

    master_kernel_directory = &page_directory;

    if (placement_address % 0x1000 != 0)
    {
        // placement_address is not page-aligned, align it
        placement_address = (placement_address & 0xFFFFF000) + 0x1000;
    }

    // identity map the first 4MB
    i = 0;
    while (i < placement_address+0x1000)
    {
        // Kernel code is readable but not writeable from userspace.
        alloc_frame(get_page(i, 1, &page_directory), 0, 0);
        i += 0x1000;
    }
    
    // monitor_printf("placement_address: %x\n", placement_address);
    // monitor_printf("frames allocated\n");

    // set up interrupt handler for page faults
    register_interrupt_handler(14, page_fault);

    // monitor_printf("interrupt 14 registered\n");

    // asm volatile("xchgw %bx, %bx");

    // Load the page directory
    switch_page_directory(&page_directory);

    // monitor_printf("page directory loaded\n");
    // asm volatile("xchgw %bx, %bx");

    //set up heap
    uint32_t start = placement_address;
    placement_address += 0x100000; // 1MB    
    kheap = create_heap(start, placement_address, 0xCFFFF000, 0, 0);

    //clone kernel directory
    current_directory = clone_page_directory(master_kernel_directory);

    kernel_directory = current_directory;

    // monitor_printf("current_directory: %x\n", current_directory);
    // asm volatile("xchgw %bx, %bx");
    switch_page_directory(current_directory);

    // monitor_printf("heap created\n");
    // asm volatile("xchgw %bx, %bx");
}

void switch_page_directory(page_directory_t *new)
{
    current_directory = new;
    load_page_directory(new->tablesPhysical);
}

page_t *get_page(uint32_t address, int make, page_directory_t *dir)
{
    // Turn the address into an index.
    address /= 0x1000;
    // Find the page table containing this address.
    uint32_t table_idx = address / 1024;
    if (dir->tables[table_idx]) // If this table is already assigned
    {
        return &dir->tables[table_idx]->pages[address%1024];
    }
    else if(make)
    {
        uint32_t tmp;
        dir->tables[table_idx] = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &tmp);
        dir->tablesPhysical[table_idx] = tmp | 0x7; // PRESENT, RW, US.
        return &dir->tables[table_idx]->pages[address%1024];
    }
    else
    {
        return 0;
    }
}

void alloc_new_page_table(page_directory_t *dir, int is_user, int is_program)
{
    uint32_t tmp;
    page_table_t *new_table = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &tmp);
    memset(new_table, 0, sizeof(page_table_t));
    uint32_t table_idx = (is_user << 1) | is_program;
    dir->tables[table_idx] = new_table;
    dir->tablesPhysical[table_idx] = tmp | 0x7; // PRESENT, RW, US.
}

void page_fault(registers_t *regs)
{
    // A page fault has occurred.
    // The faulting address is stored in the CR2 register.
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

    // The error code gives us details of what happened.
    int present   = !(regs->err_code & 0x1); // Page not present
    int rw = regs->err_code & 0x2;           // Write operation?
    int us = regs->err_code & 0x4;           // Processor was in user-mode?
    int reserved = regs->err_code & 0x8;     // Overwritten CPU-reserved bits of page entry?
    int id = regs->err_code & 0x10;          // Caused by an instruction fetch?

    // Output an error message.
    monitor_write("Page fault! ( ");
    if (present) {monitor_write("present ");}
    if (rw) {monitor_write("read-only ");}
    if (us) {monitor_write("user-mode ");}
    if (reserved) {monitor_write("reserved ");}
    monitor_write(") at 0x");
    monitor_write_hex(faulting_address);
    monitor_write("\n");
    PANIC("Page fault");
}

page_directory_t *clone_page_directory(page_directory_t *src){
    uint32_t phys;
    page_directory_t *dir = (page_directory_t*)kmalloc_ap(sizeof(page_directory_t), &phys);
    memset(dir, 0, sizeof(page_directory_t));
    npageDirs++;

    uint32_t offset = (uint32_t)dir->tablesPhysical - (uint32_t)dir;
    dir->physicalAddr = phys + offset;

    uint32_t i;
    for(i = 0; i < 1024; i++){
        if (!src->tables[i]){
            continue;
        }
        if (master_kernel_directory->tables[i] == src->tables[i]){
            // we can use the same pointer
            dir->tables[i] = src->tables[i];
            dir->tablesPhysical[i] = src->tablesPhysical[i];
        }
        else{
            // we need to clone the table
            uint32_t phys;
            dir->tables[i] = clone_page_table(src->tables[i], &phys);
            dir->tablesPhysical[i] = phys | 0x07; // PRESENT, RW, US.
        }
    }
    return dir;
}

page_directory_t *alloc_new_page_directory(int is_user){
    uint32_t phys;
    page_directory_t *dir = (page_directory_t*)kmalloc_ap(sizeof(page_directory_t), &phys);
    memset(dir, 0, sizeof(page_directory_t));
    npageDirs++;

    uint32_t offset = (uint32_t)dir->tablesPhysical - (uint32_t)dir;
    dir->physicalAddr = phys + offset;

    uint32_t i;
    for(i = 0; i < 1024; i++){
        if (is_user){
            alloc_new_page_table(dir, 1, 0);
        }
        else{
            alloc_new_page_table(dir, 0, 0);
        }
    }
    return dir;
}

page_directory_t *create_new_page_directory(int is_user){
    page_directory_t *dir = alloc_new_page_directory(is_user);
    uint32_t phys;
    page_table_t *table = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &phys);
    memset(table, 0, sizeof(page_table_t));
    npageTables++;

    uint32_t i;
    for (i = 0; i < 1024; i++)
    {
        table->pages[i].frame = i;
        table->pages[i].present = 1;
        table->pages[i].rw = 1;
        table->pages[i].user = is_user;
        table->pages[i].accessed = 0;
        table->pages[i].dirty = 0;
        table->pages[i].unused = 0;
    }

    uint32_t table_idx = (is_user << 1) | 0;
    dir->tables[table_idx] = table;
    dir->tablesPhysical[table_idx] = phys | 0x07; // PRESENT, RW, US.
    return dir;
}

uint32_t virtual_address_to_physical(uint32_t virtualAddress, page_directory_t *dir){
    uint32_t offset = virtualAddress & 0x00000FFF;
    uint32_t table_idx = virtualAddress >> 22;
    uint32_t page_idx = (virtualAddress >> 12) & 0x3FF;
    page_table_t* table = dir->tables[table_idx];
    if (!table){
        return -1;
    }
    page_t page = table->pages[page_idx];
    if (!page.frame){
        return -1;
    }
    return (page.frame << 12) + offset;
}

uint32_t physical_address_to_virtual(uint32_t physicalAddress, page_directory_t *dir){
    uint32_t frame = physicalAddress >> 12;
    uint32_t offset = physicalAddress & 0x00000FFF;
    uint32_t i;
    for (i = 0; i < 1024; i++)
    {
        if (!dir->tables[i])
        {
            continue;
        }
        if (dir->tables[i]->pages[frame].frame == frame)
        {
            return (i << 22) + (frame << 12) + offset;
        }
    }
    return -1;
}

page_table_t *clone_page_table(page_table_t *src, uint32_t *physAddress){
    //make a new page table, which is page aligned
    page_table_t *table = (page_table_t*)kmalloc_ap(sizeof(page_table_t), physAddress);
    memset(table, 0, sizeof(page_table_t));
    npageTables++;

    uint32_t i;
    for (i = 0; i < 1024; i++)
    {
        if (!src->pages[i].frame)
        {
            continue;
        }
        // get a new frame
        alloc_frame(&table->pages[i], 0, 0);
        // copy the page flags
        if (src->pages[i].present) table->pages[i].present = 1;
        if (src->pages[i].rw)      table->pages[i].rw = 1;
        if (src->pages[i].user)    table->pages[i].user = 1;
        if (src->pages[i].accessed)table->pages[i].accessed = 1;
        if (src->pages[i].dirty)   table->pages[i].dirty = 1;

        // clone the page
        copy_page_physical(src->pages[i].frame*0x1000, table->pages[i].frame*0x1000);
    }
}

// page_t *clone_page(page_t *src, uint32_t *physAddress){
//     page_t *page = (page_t*)kmalloc_ap(sizeof(page_t), physAddress);
//     memset(page, 0, sizeof(page_t));

//     page->frame = src->frame;
//     page->present = src->present;
//     page->rw = src->rw;
//     page->user = src->user;
//     page->accessed = src->accessed;
//     page->dirty = src->dirty;
//     page->unused = src->unused;
//     return page;
// }

void free_page_directory(page_directory_t *dir){
    uint32_t i;
    for (i = 0; i < 1024; i++)
    {
        if (!dir->tables[i])
        {
            continue;
        }
        if (master_kernel_directory->tables[i] == dir->tables[i])
        {
            // we can use the same pointer
            dir->tables[i] = 0;
            dir->tablesPhysical[i] = 0;
        }
        else
        {
            // we need to free the table
            kfree(dir->tables[i]);
            dir->tables[i] = 0;
            dir->tablesPhysical[i] = 0;
        }
    }
    kfree(dir);
    npageDirs--;
}

void free_page_table(page_table_t *table){
    uint32_t i;
    for (i = 0; i < 1024; i++)
    {
        if (!table->pages[i].frame)
        {
            continue;
        }
        if (kernel_page_table.pages[i].frame == table->pages[i].frame)
        {
            // we can use the same pointer
            table->pages[i].frame = 0;
        }
        else
        {
            // we need to free the page
            table->pages[i].frame = 0;
        }
    }
    kfree(table);
    npageTables--;
}

void free_page(page_t *page){
    if (page->frame)
    {
        free_frame(page);
        page->frame = 0;
    }
}

page_table_t *create_new_page_table(page_directory_t *dir, int is_user, int is_program){
    page_table_t *table;
    uint32_t phys;
    table = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &phys);
    memset(table, 0, sizeof(page_table_t));
    npageTables++;

    uint32_t i;
    for (i = 0; i < 1024; i++)
    {
        table->pages[i].frame = 0;
        table->pages[i].present = 0;
        table->pages[i].rw = 0;
        table->pages[i].user = 0;
        table->pages[i].accessed = 0;
        table->pages[i].dirty = 0;
        table->pages[i].unused = 0;
    }

    uint32_t table_idx = (is_user << 1) | is_program;
    dir->tables[table_idx] = table;
    dir->tablesPhysical[table_idx] = phys | 0x07; // PRESENT, RW, US.
    return table;
}