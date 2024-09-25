#include <monitor.h>
#include <timer.h>
#include <descriptor_tables.h>
#include <paging.h>
#include <kheap.h>
#include <fs.h>
#include <task.h>
#include <initrd.h>
#include <multiboot.h>
#include <common.h>

extern uint32_t placement_address;

extern tcb_t *current_task;

uint32_t initial_esp;

page_directory_t __attribute__((section(".usrdata"))) *user_mode_page_dir;
page_directory_t __attribute__((section(".usrdata"))) *user_task_page_dir;


int kernel_main(multiboot_info_t *mboot_ptr, uint32_t initial_stack){
    //stack pointer
    initial_esp = initial_stack;

    //initialize all the ISRs and segmentation
    init_descriptor_tables();

    //initialize the screen (by clearing it)
    monitor_clear();

    //initrd
    monitor_write("Initializing initrd... \n");
    ASSERT(mboot_ptr);
    ASSERT(mboot_ptr->mods_count > 0);

    uint32_t initrd_location = *((uint32_t*)mboot_ptr->mods_addr);
    uint32_t initrd_end = *(uint32_t*)(mboot_ptr->mods_addr+4);

    // Don't trample our module with placement accesses, please!
    placement_address = initrd_end;

    // monitor_printf("initrd_location: %x, initrd_end: %x\n", initrd_location, initrd_end);

    // foreground_color = 0x0A;
    // background_color = 0x01;
    // monitor_write("OK\n");

    // foreground_color = 0x02;
    // background_color = 0x06;

    //initialize paging
    monitor_write("Initializing paging \n");
    initialise_paging();

    // foreground_color = 0x0A;
    // background_color = 0x01;
    // monitor_write("OK\n");

    // foreground_color = 0x02;
    // background_color = 0x06;

    //initrd contd
    monitor_write("Initializing initramfs \n");
    fs_root = initialise_initrd(initrd_location);
    // asm volatile("xchgw %bx, %bx");

    // foreground_color = 0x0A;
    // background_color = 0x01;
    // monitor_write("OK\n");
    
    // foreground_color = 0x02;
    // background_color = 0x06;

    // monitor_printf("switching to user mode\n");

    // //make user mode page directory
    // user_mode_page_dir = create_new_page_directory(1);

    // monitor_printf("user mode page directory physical: %x\n", user_mode_page_dir->physicalAddr);
    // monitor_printf("test");

    // //switch to user mode page directory
    // switch_page_directory(user_mode_page_dir);

    // //switch to user mode
    // switch_to_user_mode();

    // foreground_color = 0x0A;
    // background_color = 0x01;
    // monitor_write("OK\n");
    // asm volatile("xchgw %bx, %bx");

    //enable timer
    monitor_write("Enabling timer \n");
    init_timer(1000);

    asm volatile("sti");

    // foreground_color = 0x0A;
    // background_color = 0x01;
    // monitor_write("OK\n");

    // // asm volatile("xchgw %bx, %bx");

    // foreground_color = 0x02;
    // background_color = 0x06;

    //enable multitasking
    // create_kernel_task(test_task);

    // create_user_task(user_mode, user_mode_page_dir);

    monitor_write("Enabling multitasking \n");
    // monitor_printf("%x\n", &initialize_tasking);
    initialize_tasking();

    // asm volatile("int $0x3");

    // for(;;){asm volatile("hlt");}

    return 0;
}