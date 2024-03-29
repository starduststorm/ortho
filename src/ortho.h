#ifndef ORTHO_H
#define ORTHO_H

#define STRIP_LENGTH 64
#define STRIP_COUNT 12
#define STICK_LENGTH 8
#define NUM_LEDS (STRIP_LENGTH * STRIP_COUNT)

#include "opc/opc.h"
#include "util.h"
#include "drawing.h"

typedef CustomDrawingContext<NUM_LEDS, 1, CRGB, CRGB[NUM_LEDS] > DrawingContext;

#endif
