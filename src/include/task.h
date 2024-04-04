#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <paging.h>
#include <kheap.h>

#define KERNEL_STACK_SIZE 16384 // 16KB

typedef struct {
    uint32_t id;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eip;
    page_directory_t* page_directory;
    uint32_t kernel_stack;
    heap_t* heap;
} task_t;

typedef struct {
    task_t* tasks; // Pointer to the array of tasks
    size_t capacity; // Total capacity of the array
    size_t size; // Current number of tasks in the array
} TaskArray;

extern TaskArray* task_array;

extern int next_pid;

extern void resize_task_array(TaskArray* arr, size_t new_capacity); // Resize the task array

extern TaskArray* create_task_array(size_t capacity); // Create a new task array
extern void destroy_task_array(TaskArray* arr); // Destroy a task array, freeing all memory
extern void add_task(TaskArray* arr, task_t task); // Add a task to the array
extern void remove_task(TaskArray* arr, size_t index); // Remove a task from the array
extern task_t* get_task(TaskArray* arr, size_t index); // Get a task from the array
extern void create_new_task(void (*entry)()); // Create a new task with the given entry point

extern void initialise_tasking(); // Initialise the tasking system
extern void switch_task(); // Switch to the next task in the task array

extern void fork();
extern void exec(void (*entry)());

extern void move_stack(void* new_stack_start, uint32_t size);
extern void set_kernel_stack(uint32_t stack);

#endif