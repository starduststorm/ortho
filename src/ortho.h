#ifndef ORTHO_H
#define ORTHO_H

#define STRIP_LENGTH 64
#define STRIP_COUNT 12
#define STICK_LENGTH 8
#define NUM_LEDS (STRIP_LENGTH * STRIP_COUNT)

#include "opc/opc.h"
#include "util.h"

// FIXME: need a proper drawing solution
void set(int index, Color c, float bright);
void set(int index, Color c);

#endif
