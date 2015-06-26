#ifndef __INTERRUPTS_H
#define __INTERRUPTS_H

#define INT_POKE_MASK   0xff000000  // see -mpoke-function-name

struct TaskDescriptor;

void initInterrupts();
// puts a task onto interrupt table to wait for interruptID
int awaitInterrupt(struct TaskDescriptor *active, int interruptID);
// reschedules tasks that were waiting on interrupts
void handleInterrupt() __attribute__ ((interrupt));
// resets PL190
void resetInterrupts();

#endif
