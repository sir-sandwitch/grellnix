#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <paging.h>

#define KERNEL_STACK_SIZE 16384

typedef struct thread_control_block {
    uint32_t esp0;
    uint32_t esp;
    uint32_t cr3;
    struct thread_control_block* next;
} tcb_t;

extern void initialize_tasking();

extern void create_user_task(void (*entry_point)(), page_directory_t *dir);
extern void create_kernel_task(void (*entry_point)());
extern void Schedule();

extern void switch_to_user_mode();

extern void end_task(tcb_t *task);

#endif