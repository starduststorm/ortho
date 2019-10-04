#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if RASPBERRY_PI
#include <wiringPi.h>
#endif
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
Bits bitsPattern;
Undulation undulationPattern;

Pattern *idlePatterns[] = {&raverPlaid, &needles, &bitsPattern, &undulationPattern};
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

#if RASPBERRY_PI
const int modeButtonPin = 18;
#endif
bool displayOn = true;

void setup() {
  sink = opc_new_sink((char *)"127.0.0.1:7890");
  // printf("sizeof(short) = %lu\n", sizeof(short));
  // printf("sizeof(int) = %lu\n", sizeof(int));
  // printf("sizeof(long) = %lu\n", sizeof(long));
  // printf("sizeof(long long) = %lu\n", sizeof(long long));
  // printf("sizeof(float) = %lu\n", sizeof(float));
  // printf("sizeof(double) = %lu\n", sizeof(double));

  FILE *frand = fopen("/dev/urandom","r");
  if (frand != NULL) {
    unsigned int r;
    fread(&r,sizeof(unsigned int),1,frand);
    srandom(r);
    fclose(frand);
  }

#if RASPBERRY_PI
  wiringPiSetupGpio();
  pinMode(modeButtonPin, INPUT);
  pullUpDnControl(modeButtonPin, PUD_DOWN); // pull pin down when button is not pressed
  printf("Initialized mode pin\n");
#endif

  fc.tick();
}

void nextPattern() {
  if (activePattern) {
    activePattern->lazyStop();
    lastPattern = activePattern;
  }
  activePattern = NULL;
}

void checkButtons() {
#if RASPBERRY_PI
  static bool modeButtonPressed = false;
  static long modeButtonPressedMillis = -1;

  bool oldModeButtonPressed = modeButtonPressed;
  modeButtonPressed = (digitalRead(modeButtonPin) == HIGH);
  if (modeButtonPressed && !oldModeButtonPressed) {
    printf("Mode button pressed\n");
    modeButtonPressedMillis = millis();
    nextPattern();
    displayOn = true;
  }
  if (displayOn && modeButtonPressed && millis() - modeButtonPressedMillis > 2000) {
    printf("Turning off...\n");
    if (activePattern) {
      activePattern->stop();
      activePattern = NULL;
    }
    displayOn = false;
  }
#endif
}

void runPatterns() {
  pixel oldPixels[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; ++i) {
    oldPixels[i] = pixels[i];
  }

  for (unsigned i = 0; i < kIdlePatternsCount; ++i) {
    Pattern *pattern = idlePatterns[i];
    if (pattern->isRunning() || pattern->isStopping()) {
      pattern->loop(pixels);

      float runTime = pattern->runTime();
      float runtimeAlpha = (runTime < 1000 ? runTime / 1000. : 1.0);
      for (int i = 0; i < NUM_LEDS; ++i) {
        pixels[i].r = runtimeAlpha * pixels[i].r + (1 - runtimeAlpha) * oldPixels[i].r;
        pixels[i].g = runtimeAlpha * pixels[i].g + (1 - runtimeAlpha) * oldPixels[i].g;
        pixels[i].b = runtimeAlpha * pixels[i].b + (1 - runtimeAlpha) * oldPixels[i].b;
      }
    }
  }

  // clear out patterns that have stopped themselves
  if (activePattern != NULL && !activePattern->isRunning()) {
    logf("Clearing inactive pattern %s", activePattern->description());
    activePattern = NULL;
    triggerIsActive = false; // TODO: better way to detect?
  }

  // time out idle patterns
  // Disabled for tranciness
  // if (!triggerIsActive && activePattern != NULL && activePattern->isRunning() && activePattern->runTime() > kIdlePatternTimeout) {
  //   if (activePattern != testIdlePattern && activePattern->wantsToIdleStop()) {
  //     activePattern->lazyStop();
  //     lastPattern = activePattern;
  //     activePattern = NULL;
  //   }
  // }

  // start a new idle pattern
  if (activePattern == NULL) {
    Pattern *nextPattern;
    if (testIdlePattern != NULL) {
      nextPattern = testIdlePattern;
    } else {
      int choice = (first_pattern != -1 ? first_pattern : random() % kIdlePatternsCount);
      first_pattern = -1;
      nextPattern = idlePatterns[choice];
    }
    if ((nextPattern != lastPattern || nextPattern == testIdlePattern) && !nextPattern->isRunning() && !nextPattern->isStopping() && nextPattern->wantsToRun()) {
      nextPattern->start();
      activePattern = nextPattern;
    }
  }
}

void loop() {
  checkButtons();

  if (!displayOn) {
    fadeDownBy(0.1);
  } else {
    runPatterns();
  }
#if RASPBERRY_PI
  if (0 == opc_put_pixels(sink, 0, NUM_LEDS, pixels)) {
    // Failed to connect to fadecandy, don't spam it
    sleep(5);
  }
#endif

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

