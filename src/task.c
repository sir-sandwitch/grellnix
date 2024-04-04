#include <task.h>
#include <stdint.h>
#include <paging.h>

int next_pid = 1;

TaskArray *task_array;

extern uint32_t read_eip();

extern uint32_t initial_esp;

void resize_task_array(TaskArray *arr, size_t new_capacity)
{
    task_t *new_tasks = (task_t *)kmalloc(new_capacity * sizeof(task_t));
    for (size_t i = 0; i < arr->size; i++)
    {
        new_tasks[i] = arr->tasks[i];
    }
    kfree(arr->tasks);
    arr->tasks = new_tasks;
    arr->capacity = new_capacity;
}

TaskArray *create_task_array(size_t capacity)
{
    TaskArray *arr = (TaskArray *)kmalloc(sizeof(TaskArray));
    arr->tasks = (task_t *)kmalloc(capacity * sizeof(task_t));
    arr->capacity = capacity;
    arr->size = 0;
    return arr;
}

void destroy_task_array(TaskArray *arr)
{
    kfree(arr->tasks);
    kfree(arr);
}

void add_task(TaskArray *arr, task_t task)
{
    if (arr->size >= arr->capacity)
    {
        resize_task_array(arr, arr->capacity * 2);
    }
    arr->tasks[arr->size++] = task;
}

void remove_task(TaskArray *arr, size_t index)
{
    for (size_t i = index; i < arr->size - 1; i++)
    {
        arr->tasks[i] = arr->tasks[i + 1];
    }
    arr->size--;
}

task_t *get_task(TaskArray *arr, size_t index)
{
    return &arr->tasks[index];
}

void set_kernel_stack(uint32_t stack)
{
    //set the kernel stack (the stack used by the kernel)
    asm volatile("mov %0, %%esp" : : "r"(stack));
    asm volatile("mov %0, %%ebp" : : "r"(stack));
}

void move_stack(void *new_stack_start, uint32_t size)
{
    uint32_t i;
    // Allocate some space for the new stack.
    for( i = (uint32_t)new_stack_start;
        i >= ((uint32_t)new_stack_start-size);
        i -= 0x1000)
    {
        // General-purpose stack is in user-mode.
        alloc_frame( get_page(i, 1, current_directory), 0 /* User mode */, 1 /* Is writable */ );
    } 
      // Flush the TLB by reading and writing the page directory address again.
    uint32_t pd_addr;
    asm volatile("mov %%cr3, %0" : "=r" (pd_addr));
    asm volatile("mov %0, %%cr3" : : "r" (pd_addr)); 
    // Old ESP and EBP, read from registers.
    uint32_t old_stack_pointer; asm volatile("mov %%esp, %0" : "=r" (old_stack_pointer));
    uint32_t old_base_pointer;  asm volatile("mov %%ebp, %0" : "=r" (old_base_pointer));
    uint32_t offset            = (uint32_t)new_stack_start - initial_esp;
    uint32_t new_stack_pointer = old_stack_pointer + offset;
    uint32_t new_base_pointer  = old_base_pointer + offset;
    // Copy the stack.
    memcpy((void*)new_stack_pointer, (void*)old_stack_pointer, initial_esp-old_stack_pointer);
    // Backtrace through the original stack, copying new values into the new stack.
    for(i = (uint32_t)new_stack_start; i > (uint32_t)new_stack_start-size; i -= 4)
    {
        uint32_t tmp = * (uint32_t*)i;
        // If the value of tmp is inside the range of the old stack, assume it is a base pointer
        // and remap it. This will unfortunately remap ANY value in this range, whether they are
        // base pointers or not.
        if (( old_stack_pointer < tmp) && (tmp < initial_esp))
        {
            tmp = tmp + offset;
            uint32_t *tmp2 = (uint32_t*)i;
            *tmp2 = tmp;
        }
    }
    // Change stacks.
    asm volatile("mov %0, %%esp" : : "r" (new_stack_pointer));
    asm volatile("mov %0, %%ebp" : : "r" (new_base_pointer));
}

void fork()
{
    task_t *parent_task = get_task(task_array, 0);
    task_t child_task = *parent_task;
    child_task.page_directory = clone_directory(parent_task->page_directory);
    add_task(task_array, child_task);
}

void exec(void (*entry)())
{
    task_t *parent_task = get_task(task_array, 0);
    parent_task->eip = (uint32_t)entry;

    // Set up stack.
    move_stack((void *)parent_task->esp, KERNEL_STACK_SIZE);

    switch_page_directory(parent_task->page_directory);
    asm volatile("jmp $0x08, $0x00");
}

void create_new_task(void (*entry)())
{
    task_t task;
    task.id = task_array->size;
    task.eip = (uint32_t)entry;
    task.esp = kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;
    task.ebp = kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;
    task.page_directory = current_directory;
    task.kernel_stack = kmalloc(KERNEL_STACK_SIZE);
    task.heap = create_heap(KHEAP_START, KHEAP_START + KHEAP_INITIAL_SIZE, 0xCFFFF000, 0, 0);
    add_task(task_array, task);
}

void switch_task(){
    // If we're the only task, return.
    if (task_array->size <= 1)
        return;
    // Read esp, ebp now for saving later on.
    uint32_t esp, ebp, eip;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    asm volatile("mov %%ebp, %0" : "=r"(ebp));
    // Read the instruction pointer. We do some cunning logic here:
    // One of two things could have happened when this function exits -
    // 1. We called the function and it returned the EIP as requested.
    // 2. We have just switched tasks, and because the saved EIP is essentially the
    // instruction after read_eip, it will seem as if read_eip has just returned.
    // In the second case we need to return immediately. To detect it we put a dummy
    // value in EAX further down at the end of this function. As C returns values in EAX,
    // it will look like the return of read_eip.
    eip = read_eip();
    // Have we just switched tasks?
    if (eip == 0)
        return;
    // Get the next task to run.
    int current_task = task_array->size - 1;
    current_task++;
    current_task %= task_array->size;
    task_t *next_task = get_task(task_array, current_task);
    // Set the next task's kernel stack.
    set_kernel_stack(next_task->kernel_stack + KERNEL_STACK_SIZE);
    // Here we:
    // 1. Stop interrupts so we don't get interrupted.
    // 2. Temporarily put the next task's kernel stack in the esp register.
    // 3. Restore the next task's state. This includes the next task's kernel stack,
    //    the next task's page directory, and the next task's EIP.
    // 4. Then we just return. When we do this, the iret will restore the next task's registers
    //    and jump to the next task's EIP!
    asm volatile("cli");
    asm volatile("mov %0, %%esp" : : "r"(next_task->esp));
    asm volatile("mov %0, %%ebp" : : "r"(next_task->ebp));
    switch_page_directory(next_task->page_directory);
    eip = next_task->eip;
    asm volatile("sti");
    asm volatile("jmp %0" : : "r"(eip));
}

void idle_task()
{
    while (1)
    {
        // Do nothing.
    }
}

void initialise_tasking()
{
    task_array = create_task_array(1);
}