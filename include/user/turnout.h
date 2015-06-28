#ifndef __SWITCHES_H
#define __SWITCHES_H
#include <utils.h> // bool
// handles switches manipulation, and draws onto screen

void initSwitches();
// sets the switch curved or straight
void switchesSetCurved(int location);
void switchesSetStraight(int location);
bool switchesIsCurved(int switch_number);

#endif