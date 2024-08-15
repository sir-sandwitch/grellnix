#include <kheap.h>
#include <stdint.h>
#include <stddef.h>
#include <ordered_array.h>
#include <paging.h>
#include <common.h>

extern uint32_t end; //end of kernel - declared in linker script
uintptr_t placement_address = (uintptr_t)&end;
heap_t *kheap = 0;

static int32_t find_smallest_hole(uint32_t size, uint8_t page_align, heap_t *heap)
{
    // Find the smallest hole that will fit.
    uint32_t iterator = 0;
    while (iterator < heap->index.size)
    {
        header_t *header = (header_t *)lookup_ordered_array(iterator, &heap->index);
        // If the user has requested the memory be page-aligned
        if (page_align > 0)
        {
            // Page-align the starting point of this header.
            uint32_t location = (uint32_t)header;
            int32_t offset = 0;
            if ((location+sizeof(header_t) & 0xFFFFF000) != 0)                
                offset = 0x1000 /* page size */  - (location + sizeof(header_t)) % 0x1000;
            int32_t hole_size = (int32_t)header->size - offset;
            // Can we fit now?
            if (hole_size >= (int32_t)size)
                break;
        }
        else if (header->size >= size)
            break;
        iterator++;
    }
    // Why did the loop exit?
    if (iterator == heap->index.size)
        return -1; // We got to the end and didn't find anything.
    else
        return iterator;
}

static int8_t header_t_less_than(void *a, void *b)
{
    return (((header_t *)a)->size < ((header_t *)b)->size) ? 1 : 0;
}

heap_t *create_heap(uint32_t start, uint32_t end_addr, uint32_t max, uint8_t supervisor, uint8_t readonly)
{
    heap_t *heap = (heap_t *)kmalloc(sizeof(heap_t));

    // monitor_printf("heap: %x\n", heap);

    // All our assumptions are made on startAddress and endAddress being page-aligned.
    ASSERT(start % 0x1000 == 0);
    ASSERT(end_addr % 0x1000 == 0);

    // monitor_printf("making ordered array\n");

    //load bearing print statement...
    monitor_printf("");

    //asm volatile("xchgw %bx, %bx");
    // Initialise the index.
    heap->index = place_ordered_array((void *)start, HEAP_INDEX_SIZE, &header_t_less_than);

    //this print statement is also load bearing for some reason
    monitor_printf("");
   
    // monitor_printf("heap->index.size: %d\n", heap->index.size);

    // monitor_printf("heap->index: %x\n", heap->index.array);

    // Shift the start address forward to resemble where we can start putting data.
    start += sizeof(type_t) * HEAP_INDEX_SIZE;

    // monitor_printf("start: %x\n", start);

    // Make sure the start address is page-aligned.
    if (start & 0xFFFFF000 != 0)
    {
        start &= 0xFFFFF000;
        start += 0x1000;
    }
    // Write the start, end and max addresses into the heap structure.
    heap->start_address = start;
    heap->end_address = end_addr;
    heap->max_address = max;
    heap->supervisor = supervisor;
    heap->readonly = readonly;
    // We start off with one large hole in the index.
    header_t *hole = (header_t *)start;
    hole->size = end_addr - start;
    hole->magic = HEAP_MAGIC;
    hole->is_hole = 1;
    insert_ordered_array((type_t)hole, &heap->index);
    // monitor_printf("heap->index.size: %d\n", heap->index.size);
    // asm volatile("xchgw %bx, %bx");
    return heap;
}

static void expand(uint32_t new_size, heap_t *heap)
{
    // Sanity check.
    ASSERT(new_size > heap->end_address - heap->start_address);
    // Get the nearest following page boundary.
    if (new_size & 0xFFFFF000 != 0)
    {
        new_size &= 0xFFFFF000;
        new_size += 0x1000;
    }
    // Make sure we are not overreaching ourselves.
    ASSERT(heap->start_address + new_size <= heap->max_address);

    // This should always be on a page boundary.
    uint32_t old_size = heap->end_address - heap->start_address;
    uint32_t i = old_size;
    while (i < new_size)
    {
        alloc_frame(get_page(heap->start_address + i, 1, master_kernel_directory), (heap->supervisor) ? 1 : 0, (heap->readonly) ? 0 : 1);
        i += 0x1000 /* page size */;
    }
    heap->end_address = heap->start_address + new_size;
}

static uint32_t contract(uint32_t new_size, heap_t *heap)
{
    // Sanity check.
    ASSERT(new_size < heap->end_address - heap->start_address);
    // Get the nearest following page boundary.
    if (new_size & 0x1000)
    {
        new_size &= 0x1000;
        new_size += 0x1000;
    }
    // Don't contract too far!
    if (new_size < HEAP_MIN_SIZE)
        new_size = HEAP_MIN_SIZE;
    uint32_t old_size = heap->end_address - heap->start_address;
    uint32_t i = old_size - 0x1000;
    while (new_size < i)
    {
        free_frame(get_page(heap->start_address + i, 0, master_kernel_directory));
        i -= 0x1000 /* page size */;
    }
    heap->end_address = heap->start_address + new_size;
    return new_size;
}

void *alloc(uint32_t size, uint8_t page_align, heap_t *heap)
{
    // Make sure we take the size of header/footer into account.
    uint32_t new_size = size + sizeof(header_t) + sizeof(footer_t);
    // Find the smallest hole that will fit.
    int32_t iterator = find_smallest_hole(new_size, page_align, heap);
    if (iterator == -1) // If we didn't find a suitable hole
    {
        // monitor_printf("no suitable hole\n");
        // asm volatile("xchgw %bx, %bx");
        // Save some previous data.
        uint32_t old_length = heap->end_address - heap->start_address;
        uint32_t old_end_address = heap->end_address;
        // We need to allocate some more space.
        expand(old_length + new_size, heap);
        uint32_t new_length = heap->end_address - heap->start_address;
        // Find the endmost header. (Not endmost in size, but in location).
        iterator = 0;
        // Vars to hold the index of, and value of, the endmost header found so far.
        uint32_t idx = -1; uint32_t value = 0x0;
        while (iterator < heap->index.size)
        {
            uint32_t tmp = (uint32_t)lookup_ordered_array(iterator, &heap->index);
            if (tmp > value)
            {
                value = tmp;
                idx = iterator;
            }
            iterator++;
        }
        // If we didn't find ANY headers, we need to add one.
        if (idx == -1)
        {
            header_t *header = (header_t *)old_end_address;
            header->magic = HEAP_MAGIC;
            header->size = new_length - old_length;
            header->is_hole = 1;
            footer_t *footer = (footer_t *)(old_end_address + header->size - sizeof(footer_t));
            footer->magic = HEAP_MAGIC;
            footer->header = header;
            insert_ordered_array((type_t)header, &heap->index);
        }
        else
        {
            // The last header needs adjusting.
            header_t *header = lookup_ordered_array(idx, &heap->index);
            header->size += new_length - old_length;
            // Rewrite the footer.
            footer_t *footer = (footer_t *)((uint32_t)header + header->size - sizeof(footer_t));
            footer->header = header;
            footer->magic = HEAP_MAGIC;
        }
        // We now have enough space. Recurse, and call the function again.
        return alloc(size, page_align, heap);
    }
    header_t *orig_hole_header = (header_t *)lookup_ordered_array(iterator, &heap->index);
    uint32_t orig_hole_pos = (uint32_t)orig_hole_header;
    uint32_t orig_hole_size = orig_hole_header->size;
    // Here we work out if we should split the hole we found into two parts.
    // Is the original hole size - requested hole size less than the overhead for adding a new hole?
    if (orig_hole_size - new_size < sizeof(header_t) + sizeof(footer_t))
    {
        // Then just increase the requested size to the size of the hole we found.
        size += orig_hole_size - new_size;
        new_size = orig_hole_size;
    }
    // If we need to page-align the data, do it now and make a new hole in front of our block.
    if (page_align && orig_hole_pos & 0xFFFFF000)
    {
        // monitor_printf("page align\n");
        // asm volatile("xchgw %bx, %bx");
        uint32_t new_location   = orig_hole_pos + 0x1000 /* page size */  - (orig_hole_pos & 0xFFF) - sizeof(header_t);
        header_t *hole_header = (header_t *)orig_hole_pos;
        hole_header->size     = 0x1000 /* page size */  - (orig_hole_pos & 0xFFF) - sizeof(header_t);
        hole_header->magic    = HEAP_MAGIC;
        hole_header->is_hole  = 1;
        footer_t *hole_footer = (footer_t *)((uint32_t)new_location - sizeof(footer_t));
        hole_footer->magic    = HEAP_MAGIC;
        hole_footer->header   = hole_header;
        orig_hole_pos         = new_location;
        orig_hole_size        = orig_hole_size - hole_header->size;
    }
    else
    {
        // monitor_printf("no page align\n");
        // asm volatile("xchgw %bx, %bx");
        // Else we don't need this hole any more, delete it from the index.
        remove_ordered_array(iterator, &heap->index);
        // monitor_printf("removed\n");
    }
    // Overwrite the original header...
    header_t *block_header  = (header_t *)orig_hole_pos;
    block_header->magic     = HEAP_MAGIC;
    block_header->is_hole   = 0;
    block_header->size      = new_size;
    // ...And the footer
    footer_t *block_footer  = (footer_t *)(orig_hole_pos + sizeof(header_t) + size);
    block_footer->magic     = HEAP_MAGIC;
    block_footer->header    = block_header;
    // We may need to write a new hole after the allocated block.
    // We do this only if the new hole would have positive size...
    if (orig_hole_size - new_size > 0)
    {
        // monitor_printf("new hole\n");
        // asm volatile("xchgw %bx, %bx");
        header_t *hole_header = (header_t *)(orig_hole_pos + sizeof(header_t) + size + sizeof(footer_t));
        hole_header->magic    = HEAP_MAGIC;
        hole_header->is_hole  = 1;
        hole_header->size     = orig_hole_size - new_size;
        footer_t *hole_footer = (footer_t *)((uint32_t)hole_header + orig_hole_size - new_size - sizeof(footer_t));
        if ((uint32_t)hole_footer < heap->end_address)
        {
            hole_footer->magic = HEAP_MAGIC;
            hole_footer->header = hole_header;
        }
        // Put the new hole in the index;
        insert_ordered_array((type_t)hole_header, &heap->index);
    }
    // monitor_printf("mem address: %x\n", orig_hole_pos + sizeof(header_t));
    // asm volatile("xchgw %bx, %bx");
    // ...And we're done!
    return (void *)((uint32_t)block_header + sizeof(header_t));
}

void free(void *p, heap_t *heap){
    // Exit gracefully for null pointers.
    if (p == 0)
        return;
    // Get the header and footer associated with this pointer.
    header_t *header = (header_t *)((uint32_t)p - sizeof(header_t));
    footer_t *footer = (footer_t *)((uint32_t)header + header->size - sizeof(footer_t));
    // Sanity checks.
    ASSERT(header->magic == HEAP_MAGIC);
    ASSERT(footer->magic == HEAP_MAGIC);
    // Make us a hole.
    header->is_hole = 1;
    // Do we want to add this header into the 'free holes' index?
    char do_add = 1;
    // Unify left
    // If the thing immediately to the left of us is a footer...
    footer_t *test_footer = (footer_t *)((uint32_t)header - sizeof(footer_t));
    if (test_footer->magic == HEAP_MAGIC &&
        test_footer->header->is_hole == 1)
    {
        // monitor_printf("unify left\n");
        // asm volatile("xchgw %bx, %bx");
        uint32_t cache_size = header->size; // Cache our current size.
        header = test_footer->header;     // Rewrite our header with the new one.
        footer->header = header;          // Rewrite our footer to point to the new header.
        header->size += cache_size;       // Change the size.
        do_add = 0;                       // Since this header is already in the index, we don't want to add it again.
    }
    // Unify right
    // If the thing immediately to the right of us is a header...
    header_t *test_header = (header_t *)((uint32_t)footer + sizeof(footer_t));
    if (test_header->magic == HEAP_MAGIC &&
        test_header->is_hole)
    {
        // monitor_printf("unify right\n");
        // asm volatile("xchgw %bx, %bx");
        header->size += test_header->size; // Increase our size.
        test_footer = (footer_t *)((uint32_t)test_header + // Rewrite it's footer to point to our header.
            test_header->size - sizeof(footer_t));
        footer = test_footer;
        // Find and remove this header from the index.
        uint32_t iterator = 0;
        while ((iterator < heap->index.size) &&
            (lookup_ordered_array(iterator, &heap->index) != (type_t)test_header))
            iterator++;
        // Make sure we actually found the item.
        ASSERT(iterator < heap->index.size);
        // Remove it.
        remove_ordered_array(iterator, &heap->index);
    }
    // If the footer location is the end address, we can contract.
    if ((uint32_t)footer + sizeof(footer_t) == heap->end_address)
    {
        // monitor_printf("contract\n");
        // asm volatile("xchgw %bx, %bx");
        uint32_t old_length = heap->end_address - heap->start_address;
        uint32_t new_length = contract((uint32_t)header - heap->start_address, heap);
        // monitor_printf("old_length: %d, new_length: %d\n", old_length, new_length);
        // asm volatile("xchgw %bx, %bx");
        // Check how big we will be after resizing.
        if (header->size - (old_length - new_length) > 0)
        {
            // monitor_printf("header->size - (old_length - new_length) > 0\n");
            // asm volatile("xchgw %bx, %bx");
            // We will still exist, so resize us.
            header->size -= old_length - new_length;
            footer = (footer_t *)((uint32_t)header + header->size - sizeof(footer_t));
            footer->magic = HEAP_MAGIC;
            footer->header = header;
        }
        else
        {
            // monitor_printf("header->size - (old_length - new_length) <= 0\n");
            // asm volatile("xchgw %bx, %bx");
            // We will no longer exist :(. Remove us from the index.
            uint32_t iterator = 0;
            while ((iterator < heap->index.size) &&
                (lookup_ordered_array(iterator, &heap->index) != (type_t)test_header))
                iterator++;
            // If we didn't find ourselves, we have nothing to remove.
            if (iterator < heap->index.size)
                remove_ordered_array(iterator, &heap->index);
        }
    }
    if (do_add == 1){
        // monitor_printf("do_add\n");
        // asm volatile("xchgw %bx, %bx");
        insert_ordered_array((type_t)header, &heap->index);
    }
}

uint32_t kmalloc_int(size_t size, int align, uint32_t *phys)
{
    if(kheap != 0)
    {
        // monitor_printf("kheap\n");
        // asm volatile("xchgw %bx, %bx");
        void *addr = alloc(size, (uint8_t)align, kheap);
        // monitor_printf("addr: %x\n", addr);
        // asm volatile("xchgw %bx, %bx");
        if(phys != 0)
        {
            page_t *page = get_page((uint32_t)addr, 0, master_kernel_directory);
            *phys = page->frame * 0x1000 + ((uint32_t)addr & 0xFFF);
        }
        return (uint32_t)addr;
    }
    // Pages are aligned to 4K, or 0x1000
    if (align == 1 && (placement_address & 0x00000FFF)) // If the address is not already page-aligned    
    {
        // monitor_printf("align\n");
        // asm volatile("xchgw %bx, %bx");
        placement_address &= 0xFFFFF000;
        placement_address += 0x1000;
    }
    if(phys)
    {
        // monitor_printf("phys\n");
        // asm volatile("xchgw %bx, %bx");
        *phys = placement_address;
    }
    uint32_t tmp = placement_address;
    placement_address += size;
    return tmp;
}

uint32_t kmalloc_a(size_t size)
{
    return kmalloc_int(size, 1, NULL);
}

uint32_t kmalloc_p(size_t size, uint32_t *phys)
{
    return kmalloc_int(size, 0, phys);
}

uint32_t kmalloc_ap(size_t size, uint32_t *phys)
{
    return kmalloc_int(size, 1, phys);
}

uint32_t kmalloc(size_t size)
{
    return kmalloc_int(size, 0, NULL);
}

void kfree(void *p)
{
    free(p, kheap);
}
