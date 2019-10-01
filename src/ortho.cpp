#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// #include <wiringPi.h>
#include "opc/opc.h"


#define SERIAL_LOGGING 0
#define STRIP_LENGTH 64
#define STRIP_COUNT 12
#define NUM_LEDS (STRIP_LENGTH * STRIP_COUNT)
#define UNCONNECTED_PIN 14

#include "util.h"
#include "patterns.h"


pixel pixels[NUM_LEDS];
opc_sink sink;

// CenterPulse centerPulsePattern;
// StandingWaves standingWavesPattern;
// Droplets dropletsPattern;
// Bits bitsPattern;
// SmoothPalettes smoothPalettes;
RaverPlaid raverPlaid;
Needles needles;

Pattern *idlePatterns[] = {&raverPlaid, &needles};
//&centerPulsePattern, &standingWavesPattern, &dropletsPattern, &bitsPattern, &smoothPalettes};
const unsigned int kIdlePatternsCount = ARRAY_SIZE(idlePatterns);

Pattern *activePattern = NULL;
Pattern *lastPattern = NULL;

bool triggerIsActive = false;

/* ---- Test Options ---- */
const bool kTestPatternTransitions = false;
const int kIdlePatternTimeout = 1000 * (kTestPatternTransitions ? 20 : 60 * 2);
const unsigned long kTestTriggerAtInterval = 0;//10000;//1000 * 35;//1000 * 10; // 0 for no test

Pattern *testIdlePattern = NULL;//&bitsPattern;

/* ---------------------- */

unsigned long lastTrigger = 0;
FrameCounter fc;

int first_pattern = -1;
long lastFrame = 0;
const int fps_cap = 60;

void setup() {
  sink = opc_new_sink((char *)"127.0.0.1:7890");
  printf("sizeof(short) = %u\n", sizeof(short));
  printf("sizeof(int) = %u\n", sizeof(int));
  printf("sizeof(long) = %u\n", sizeof(long));
  printf("sizeof(long long) = %u\n", sizeof(long long));
  printf("sizeof(float) = %u\n", sizeof(float));
  printf("sizeof(double) = %u\n", sizeof(double));

    FILE *frand = fopen("/dev/urandom","r");
    if (frand != NULL) {
      unsigned int r;
      fread(&r,sizeof(unsigned int),1,frand);
      srandom(r);
      fclose(frand);
    }

  // pinMode( 7, INPUT );
  // printf("read on pin = %i", digitalRead(7));

  fc.tick();
}

void loop() {
  for (unsigned i = 0; i < kIdlePatternsCount; ++i) {
    Pattern *pattern = idlePatterns[i];
    if (pattern->isRunning() || pattern->isStopping()) {
      pattern->loop(pixels);
    }
  }

  // clear out patterns that have stopped themselves
  if (activePattern != NULL && !activePattern->isRunning()) {
    logf("Clearing inactive pattern %s", activePattern->description());
    activePattern = NULL;
    triggerIsActive = false; // TODO: better way to detect?
  }

  // time out idle patterns
  if (!triggerIsActive && activePattern != NULL && activePattern->isRunning() && activePattern->runTime() > kIdlePatternTimeout) {
    if (activePattern != testIdlePattern && activePattern->wantsToIdleStop()) {
      activePattern->lazyStop();
      lastPattern = activePattern;
      activePattern = NULL;
    }
  }

  // start a new idle pattern
  if (activePattern == NULL) {
    Pattern *nextPattern;
    if (testIdlePattern != NULL) {
      nextPattern = testIdlePattern;
    } else {
      int choice = (first_pattern != -1 ? first_pattern : random() % kIdlePatternsCount);
      nextPattern = idlePatterns[choice];
    }
    if ((nextPattern != lastPattern || nextPattern == testIdlePattern) && !nextPattern->isRunning() && !nextPattern->isStopping() && nextPattern->wantsToRun()) {
      nextPattern->start();
      activePattern = nextPattern;
    }
  }
  opc_put_pixels(sink, 0, NUM_LEDS, pixels);


  long frameTime = millis() - lastFrame;
  if (frameTime < 1000./fps_cap) {
    usleep((1000./fps_cap - frameTime) * 1000);
  }
  lastFrame = millis();
  
  fc.tick();
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    first_pattern = atoi(argv[1]);
  }
  setup();
  while (1) {
    loop();
  }
}

