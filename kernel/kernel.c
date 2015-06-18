#define KERNEL_MAIN
#include <kernel/scheduler.h>
#include <kernel/message_passing.h>
#include <user/syscall.h>
#undef KERNEL_MAIN
#include <kernel/interrupts.h>
#include <kernel/context_switch.h>
#include <kernel/uart.h>
#include <kernel/timer.h>
#include <user/user_tasks.h>
#include <user/io.h>
#include <debug.h>
#include <events.h>
#include <user/clockserver.h>
#include <user/nameserver.h>
#include <priority.h>
#include <user/clock_drawer.h>
#include <user/parser.h>
#include <user/sensor.h>


static Syscall *request = NULL;

void enableCache()
{
    asm volatile(
        "mrc p15, 0, r0, c1, c0, 0\n\t"
        "ldr r1, =0xc0001004\n\t"
        "orr r0, r0, r1\n\t"
        "mcr p15, 0, r0, c1, c0, 0\n\t"
    );
}

void disableCache()
{
    asm volatile(
        "mrc p15, 0, r0, c1, c0, 0\n\t"
        "ldr r1, =0x1fffeffb\n\t"
        "and r0, r0, r1\n\t"
        "mcr p15, 0, r0, c1, c0, 0\n\t"
    );
}

void idle() {
    for (;;) {
        Pass();
    }
}

void bootstrap()
{
    // Create name server
    Create (PRIORITY_NAMESERVER, nameserverTask);

    // Create clock server
    Create(PRIORITY_CLOCK_SERVER, clockServerTask);

    // Create IO Servers
    Create(PRIORITY_TRAIN_OUT_SERVER, trainOutServer);
    Create(PRIORITY_TRAIN_IN_SERVER, trainInServer);
    Create(PRIORITY_MONITOR_OUT_SERVER, monitorOutServer);
    Create(PRIORITY_MONITOR_IN_SERVER, monitorInServer);

    // Create user task
    Create(PRIORITY_CLOCK_DRAWER, clockDrawer);
    Create(PRIORITY_PARSER, parserTask);
    Create(PRIORITY_SENSOR_TASK, sensorTask);

    // Create idle task
    Create(PRIORITY_IDLE, idle);

    // quit
    Exit();
}

static void initKernel() {
    enableCache();
    initTaskSystem();
    initScheduler();
    initMessagePassing();
    request = initSyscall();
    initInterrupts();
    initUART();
    initTimer();

    //int create_ret = taskCreate(1, userTaskMessage, 0);
    // int create_ret = taskCreate(1, userTaskHwiTester, 0);
    // int create_ret = taskCreate(1, runBenchmarkTask, 0);
    // int create_ret = taskCreate(1, interruptRaiser, 0);
    // int create_ret = taskCreate(1, userTaskK3, 0);
    // int create_ret = taskCreate(1, userTaskIdle, 31);
    int create_ret = taskCreate(1, bootstrap, 0);

    assert(create_ret >= 0);
    queueTask(taskGetTDById(create_ret));
}

static void resetKernel() {
    resetTimer();
    // resetUART();
    resetInterrupts();
    disableCache();
}

static inline int handleRequest(TaskDescriptor *td) {
    switch (request->type) {
        case INT_IRQ:
            handleInterrupt();
            break;
        case SYS_AWAIT_EVENT:
            td->ret = awaitInterrupt(td, request->arg1);
            // we don't want to reschedule if the task is event blocked
            if (td->ret == 0) return 0;
        case SYS_SEND:
            handleSend(td, request);
            return 0;
        case SYS_RECEIVE:
            handleReceive(td, request);
            return 0;
        case SYS_REPLY:
            handleReply(td, request);
            return 0;
        case SYS_CREATE: {
            int create_ret = taskCreate(request->arg1,
                (void*)(request->arg2),
                taskGetIndex(td));
            if (create_ret >= 0) {
                td->ret = taskGetIndexById(create_ret);
                queueTask(taskGetTDById(create_ret));
            } else {
                td->ret = create_ret;
            }
            break;
        }
        case SYS_MY_TID:
            td->ret = taskGetIndex(td);
            break;
        case SYS_MY_PARENT_TID:
            td->ret = taskGetMyParentIndex(td);
            break;
        case SYS_PASS:
            break;
        case SYS_EXIT:
            return 0;
        case SYS_HALT:
            return -1;
        default:
            debug("Invalid syscall %u!", request->type);
            break;
    }
    // requeue the task if we haven't returned (from SYS_EXIT)
    queueTask(td);
    return 0;
}

int main() {
    initKernel();
    TaskDescriptor *task = NULL;
    for(;;) {
        task = schedule();
        if (task == NULL) {
            break;
        }
        kernelExit(task);
        if(handleRequest(task)) {
            bwprintf(COM2, "Halt\n\r");
            break;
        }
        request->type = INT_IRQ;
    }
    resetKernel();
    debug("No tasks scheduled; exiting...");
    bwprintf(COM1, "%c", 0x61);
    return 0;
}
