#include <task.h>
#include <stdint.h>
#include <paging.h>
#include <kheap.h>
#include <monitor.h>
#include <initrd.h>

tcb_t *current_task;
tcb_t *ready_queue_start=0;
tcb_t *ready_queue_end=0;

// extern void test_task();

extern void switch_to_task(tcb_t *next_thread);
extern void switch_to_first_task(tcb_t *next_thread);
extern page_directory_t *kernel_directory;

extern void post_tasking_init();

// void switch_to_user_mode()
// {
//    // Set up a stack structure for switching to user mode.
//    asm volatile(
//     "movw $((4 * 8) | 3), %ax\n\t" // ring 3 data with bottom 2 bits set for ring 3
//     "movw %ax, %ds\n\t"
//     "movw %ax, %es\n\t"
//     "movw %ax, %fs\n\t"
//     "movw %ax, %gs\n\t" // SS is handled by iret
//     // set up the stack frame iret expects
//     "movl %esp, %eax\n\t"
//     "pushl $((4 * 8) | 3)\n\t" // data selector
//     "pushl %eax\n\t"           // current esp
//     "pushfl\n\t"               // eflags
//     "pushl $((3 * 8) | 3)\n\t" // code selector (ring 3 code with bottom 2 bits set for ring 3)
//     "pushl $1f\n\t"            // instruction address to return to
//     "iret\n\t"
//     "1:\n\t"
//     );
// }


extern page_directory_t *user_mode_page_dir;
extern page_directory_t *user_task_page_dir;

void __attribute__((section(".usrtext"))) user_mode();

void user_mode(){
    // monitor_write("User mode\n");
    for(;;){
        // asm volatile("hlt");
    }
}

void post_tasking_init(){

    monitor_printf("Post tasking init\n");

    // foreground_color = 0x0A;
    // background_color = 0x01;
    // monitor_write("OK\n");

    // // asm volatile("xchgw %bx, %bx");

    // foreground_color = 0x02;
    // background_color = 0x06;

    // //test memory allocation
    // monitor_write("Testing memory allocation ");
    // uint32_t a = kmalloc(8);
    // uint32_t b = kmalloc(8);
    // uint32_t c = kmalloc(8);
    // // asm volatile("xchgw %bx, %bx");
    // uint32_t oldb = b;

    // kfree(c);
    // // asm volatile("xchgw %bx, %bx");
    // kfree(b);

    // uint32_t d = kmalloc(12);

    // //if the memory allocation is not working, the kernel will hang here
    // if(d != oldb){
    //     foreground_color = 0x0C;
    //     background_color = 0x01;
    //     monitor_write("Failed\n");
    //     while(1){}
    // }
    // else{
    //     foreground_color = 0x0A;
    //     background_color = 0x01;
    //     monitor_write("OK\n");
    // }

    // foreground_color = 0x02;
    // background_color = 0x06;

    // list the contents of /
    int i = 0;
    struct dirent *node = 0;
    while ( (node = readdir_fs(fs_root, i)) != 0)
    {
        monitor_write("Found file ");
        monitor_write(node->name);
        // asm volatile("xchgw %bx, %bx");
        fs_node_t *fsnode = finddir_fs(fs_root, node->name);
        // monitor_printf("file: %s, fsnode: %x\n", node->name, fsnode);
        // asm volatile("xchgw %bx, %bx");

        if ((fsnode->flags&0x7) == FS_DIRECTORY){
            monitor_write("\n\t(directory)\n");
            // asm volatile("xchgw %bx, %bx");
        }
        else
        {
            char msg[15] = "\n\t contents: \"";
            for(int j = 0; j < 15; j++){
                monitor_put(msg[j]);
            }
            // asm volatile("xchgw %bx, %bx");
            char buf[256];
            uint32_t sz = read_fs(fsnode, 0, 256, buf);
            int j;
            for (j = 0; j < sz; j++)
            monitor_put(buf[j]);

            monitor_write("\"\n");
        }
        i++;
    }

    user_mode_page_dir = create_new_page_directory(1);

    switch_page_directory(user_mode_page_dir);

    user_task_page_dir = create_new_page_directory(1);

    create_user_task(user_mode, user_task_page_dir);

    monitor_write("Switching to user mode \n");
    // asm volatile("xchgw %bx, %bx");

    switch_to_user_mode();

    // end_task(current_task);
}

void switch_to_user_mode()
{
   // Set up a stack structure for switching to user mode.
   asm volatile("  \
     cli; \
     mov $0x23, %ax; \
     mov %ax, %ds; \
     mov %ax, %es; \
     mov %ax, %fs; \
     mov %ax, %gs; \
     xchgw %bx, %bx; \
                   \
     mov %esp, %eax; \
     pushl $0x23; \
     pushl %eax; \
     pushf; \
     pop %eax; \
     or $0x200, %eax; \
     push %eax; \
     pushl $0x1B; \
     push $1f; \
     iret; \
     1: \
     ");
} 

void create_kernel_task(void (*entry_point)()) {
    if (ready_queue_start == 0) {
        monitor_write(""); // load bearing print statement
        ready_queue_start = (tcb_t *)kmalloc(sizeof(tcb_t)); //issue here?
        if(!ready_queue_start){
            monitor_write("error in malloc of ready_queue_start\n");
            return;
        }
        monitor_printf(""); // another load bearing print statement
        // asm volatile("xchgw %bx, %bx");
        ready_queue_end = ready_queue_start;
        ready_queue_start->next = ready_queue_start; // Make it circular
    } else {
        ready_queue_end->next = (tcb_t *)kmalloc(sizeof(tcb_t));
        if(!ready_queue_end->next){
            monitor_write("error in malloc of new task");
        }
        ready_queue_end = ready_queue_end->next;
        ready_queue_end->next = ready_queue_start; // Maintain circularity
    }

    // page_directory_t *dir = clone_page_directory(master_kernel_directory);
    page_directory_t *dir = kernel_directory;
    // monitor_printf("Creating task with cr3: %x\n", dir->physicalAddr);
    ready_queue_end->cr3 = dir->physicalAddr;

    ready_queue_end->esp0 = (uint32_t)kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;
    ready_queue_end->esp = ready_queue_end->esp0;

    uint32_t *stack = (uint32_t *)ready_queue_end->esp;

    // Prepare the stack for the initial task switch
    *(--stack) = (uint32_t)entry_point;  // EIP
    *(--stack) = 0x202;                  // EFLAGS (set the IF flag to enable interrupts after task switch)
    *(--stack) = 0x08;                   // CS (assuming kernel code segment selector)
    *(--stack) = (uint32_t)entry_point;  // EIP (entry point of the new task)
    *(--stack) = 0;                      // EBX
    *(--stack) = 0;                      // ESI
    *(--stack) = 0;                      // EDI
    *(--stack) = 0;                      // EBP
    // *(--stack) = (uint32_t)ready_queue_end->esp0; // ESP

    ready_queue_end->esp = (uint32_t)stack;  // Update the task's ESP
}

void end_task(tcb_t *task){
    if(task == ready_queue_start){
        ready_queue_start = ready_queue_start->next;
    }
    tcb_t *t = ready_queue_start;
    while(t->next != task){
        t = t->next;
    }
    t->next = task->next;
    kfree((void *)task->esp0);
    kfree((void *)task);
}


void kernel_task(){
    while(1){
        // monitor_printf("B");
    }
}


void initialize_tasking(){
    create_kernel_task(kernel_task);
    // asm volatile("xchgw %bx, %bx");
    create_kernel_task(post_tasking_init);
    // asm volatile("xchgw %bx, %bx");
    if(!ready_queue_start){
        monitor_write("No tasks to run\n");
        return;
    }
    monitor_printf(""); // a print statement a day keeps the page faults away
    current_task = ready_queue_start;
    asm volatile("cli");
    // monitor_printf("%x\n", current_task->esp);
    // asm volatile("xchgw %bx, %bx");
    switch_to_first_task(current_task);
}

void create_user_task(void (*entry_point)(), page_directory_t *dir){
    if(ready_queue_start == 0){
        ready_queue_start = (tcb_t *)kmalloc(sizeof(tcb_t));
        ready_queue_end = ready_queue_start;
        ready_queue_start->next = ready_queue_start;
    }else{
        ready_queue_end->next = (tcb_t *)kmalloc(sizeof(tcb_t));
        ready_queue_end = ready_queue_end->next;
        ready_queue_end->next = ready_queue_start;
    }

    if(!dir){
        monitor_printf("No page directory provided for user task!\n");
        return;
    }

    ready_queue_end->cr3 = dir->physicalAddr;

    ready_queue_end->esp0 = (uint32_t)kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;
    ready_queue_end->esp = ready_queue_end->esp0;

    uint32_t *stack = (uint32_t *)ready_queue_end->esp;

    *(--stack) = (uint32_t)entry_point;
    *(--stack) = 0x202;
    *(--stack) = 0x18;
    *(--stack) = (uint32_t)entry_point;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;

    ready_queue_end->esp = (uint32_t)stack;
}

void Schedule() {
    if (!ready_queue_start) {
        return;
    }

    // monitor_printf("current_task: %x\n", current_task);
    tcb_t *next_task = current_task ? current_task->next : ready_queue_start;

    if (!next_task) {
        next_task = ready_queue_start;  // Ensure a valid task is picked from the ready queue.
    }

    if (!next_task) {
        // monitor_printf("No valid task found!\n");
        return;
    }

    // monitor_printf("Switching to task with esp: %x\n", next_task->esp);
    // asm volatile("xchgw %bx, %bx");
    asm volatile("cli");
    switch_to_task(next_task);
}
