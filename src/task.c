#include <task.h>
#include <stdint.h>
#include <paging.h>
#include <kheap.h>
#include <monitor.h>

tcb_t *current_task;
tcb_t *ready_queue_start=0;
tcb_t *ready_queue_end=0;

// extern void test_task();

extern void switch_to_task(tcb_t *next_thread);
extern void switch_to_first_task(tcb_t *next_thread);
extern page_directory_t *kernel_directory;

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
        ready_queue_start = (tcb_t *)kmalloc(sizeof(tcb_t));
        ready_queue_end = ready_queue_start;
        ready_queue_start->next = ready_queue_start; // Make it circular
    } else {
        ready_queue_end->next = (tcb_t *)kmalloc(sizeof(tcb_t));
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


void kernel_task(){
    while(1){
        monitor_printf("B");
    }
}

void test_task(){
    while(1){
        monitor_printf("A");
    }
}

void initialize_tasking(){
    create_kernel_task(kernel_task);
    create_kernel_task(test_task);
    current_task = ready_queue_start;
    asm volatile("cli");
    // asm volatile("xchgw %bx, %bx");
    switch_to_first_task(current_task);
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
