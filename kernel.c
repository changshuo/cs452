#include <ts7200.h>
#include <stdbool.h>
#include <cpsr.h>
#include <task.h>
#include <syscall.h>
#include <context_switch.h>
#include <scheduler.h>
#include <bwio.h>
#include <user_task.h>

void initKernel() {
    // Initialize swi jump table to kernel entry point
    *(unsigned int *)(0x28) = (unsigned int)(&KernelEnter);
    initTaskSystem();
    initScheduler();

    TaskDescriptor *initialTask = taskCreate(1, &userModeTask, -1);
    if( ! initialTask ) {
        bwprintf( COM2, "FATAL: fail creating first task.\n\r" );
        return;
    }
    queueTask(initialTask);
}

static TaskQueue sendQueues[128];

void handleSend(TaskDescriptor *sending_task, Syscall *request)
{
    int tid = (int)request->arg1;
    void *msg = (void *)request->arg2;
    unsigned int msglen = request->arg3;
    void *reply = (void *)request->arg4;
    unsigned int replylen = request->arg5;
    TaskDescriptor *receiving_task = NULL; // FIXME

    //1) receiving task is rcv_blk (receive first)
    // copy msg from here to receiving task's stack
    if (receiving_task->status == receive_block)
    {
        // update sending_task's status to reply_block
        sending_task->status = reply_block;
    }
    // 2) tid task is not receive_block (send first)
    // queue sending_task to send_queue of receiving_task
    else
    {
        // get receiving_task's send queue
        TaskQueue *q = &sendQueues[tid];
    
        if (q->tail == NULL)
        {
            // empty queue
            q->head = sending_task;
            q->tail = sending_task;
            sending_task->send_next = NULL;
        }
        else
        {
            // non-empty queue
            q->tail->send_next = sending_task;
            q->tail = sending_task;
        }
        
        // update sending_task status to send_block
        sending_task->status = send_block;
    }
}

void handleReceive(TaskDescriptor *receiving_task, Syscall *request)
{
    int *tid = (int *)request->arg1;
    void *msg = (void *)request->arg2;
    unsigned int msglen = request->arg3;

    TaskQueue *q = &sendQueues[*tid];

    // if send queue empty, receive block
    if (q->head == NULL)
    {
        receiving_task->status = receive_block;
    }
    // if send queue not empty, dequeue first task, copy msg to msg
    else
    {
        // dequeue first sending task
        TaskDescriptor *sending_task = q->head;

        // copy msg
        // sending_task->msg

        // ready to reply at this point
    }
}

void handleRequest(TaskDescriptor *td, Syscall *request) {
    switch (request->type) {
        case SYS_CREATE: {
            TaskDescriptor *task = taskCreate(request->arg1, (void*)request->arg2, td->parent_id);
            if (task) {
                queueTask(task);
                td->ret = taskGetUnique(task);
            }
            else {
                td->ret = -1;
            }
            break;
        }
        case SYS_MY_TID:
            td->ret = taskGetUnique(td);
            break;
        case SYS_MY_PARENT_TID:
            td->ret = taskGetMyParentUnique(td);
            break;
        case SYS_SEND:
            handleSend(td, request);
            break;
        case SYS_RECEIVE:
            // int *tid, void *msg, unsigned int msglen
            handleReceive(td, request);
            break;
        case SYS_REPLY:
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

int main() {
    initKernel();
    volatile TaskDescriptor *task = NULL;

    for(task = schedule() ; ; task = schedule()) {
        Syscall **request = NULL;
        if (task == NULL) {
            bwprintf(COM2, "No tasks scheduled; exiting...\n\r");
            break;
        }
        KernelExit(task, request);
        handleRequest((TaskDescriptor *)task, *request);
    }

    return 0;
}
