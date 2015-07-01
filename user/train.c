#include <utils.h>
#include <user/train.h>
#include <user/syscall.h>

#define REVERSE   15
#define STRAIGHT 33
#define CURVED 34
#define SOLENOID_OFF 32
#define NUM_TRAINS 80

static unsigned short speeds[NUM_TRAINS];

void trainSetSpeed(int train_number, int train_speed) {
    Putc(COM1, train_speed);
    Putc(COM1, train_number);
    speeds[train_number] = train_speed;
}

void trainSetReverse(int train_number) {
    Putc(COM1, REVERSE);
    Putc(COM1, train_number);
}

void trainSetReverseNicely(int train_number) {
    unsigned short prev_speed = speeds[train_number];
    trainSetSpeed(train_number, 0);
    Delay(speeds[train_number] * 200);
    trainSetReverse(train_number);
    trainSetSpeed(train_number, prev_speed);
}

// DEPRECATED, only used by turnout.c
void trainSetSwitch(int switch_number, char direction) {
    char operation = (direction == 'c' || direction == 'C') ? CURVED : STRAIGHT;
    Putc(COM1, operation);
    Putc(COM1, switch_number);
    Putc(COM1, SOLENOID_OFF);
}

void initTrain() {
    for(int i = 0; i < NUM_TRAINS; i++) speeds[i] = 0;
}
