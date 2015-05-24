#include <ts7200.h>
#include <stdbool.h>
#include <cpsr.h>
#include <task.h>
#include <syscall.h>
#include <context_switch.h>
#include <scheduler.h>

void firstUserTask() {
    bwprintf(COM2, "First task!\n\r");
    int tid = MyParentTid();
    bwprintf(COM2, "Back! My parent_tid: %d\n\r", tid);
    Exit();
    bwprintf(COM2, "Back again!");
    Exit();
}

void initKernel() {
    // Initialize swi jump table to kernel entry point
    *(unsigned int *)(0x28) = (unsigned int)(&KernelEnter);
    
    initTaskSystem();

    initScheduler();

    // Initialize first user task
    queueTask(taskCreate(0, &firstUserTask, -1));
}

void handleRequest(TaskDescriptor *td, Syscall *request) {
    bwprintf(COM2, "Handling tid: %d\n\r", td->id);

    switch (request->type) {
    case SYS_CREATE:
    {
        TaskDescriptor *task = taskCreate(request->arg1, (void*)request->arg2, td->parent_id);
        if (task)
        {
            queueTask(task);
            td->ret = 0;
        }
        else
        {
            td->ret = -1;
        }
        break;
    }
    case SYS_MY_TID:
        bwprintf(COM2, "Setting return value for mytid: %d\n\r", td->id);
        td->ret = taskGetMyId(td);
        break;
    case SYS_MY_PARENT_TID:
        bwprintf(COM2, "Setting return value parent_tid: %d\n\r", td->parent_id);
        td->ret = taskGetMyParentId(td);
        break;
    case SYS_PASS:
        break;
    case SYS_EXIT:
        return;
    default:
        bwprintf(COM2, "Invalid syscall %u!", request->type);
        break;
    }

    // requeue the task if we haven't returned (from SYS_EXIT)
    queueTask(td);
}

int main()
{
    initKernel();

    for (;;)
    {
         Syscall **request = NULL;
         TaskDescriptor *task = schedule();
         if (task == NULL)
         {
             bwprintf(COM2, "No tasks scheduled; exiting...\n\r");
             break;
         }
         KernelExit(task, request);
         handleRequest(task, *request);
    }

    return 0;
}
