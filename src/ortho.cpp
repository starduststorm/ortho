#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#if RASPBERRY_PI
#include <wiringPi.h>
#endif
#include "opc/opc.h"
#include "ortho.h"

#include <algorithm>
#include "util.h"
#include "patterns.h"
#include "HomeBridgeListener.h"

#define SERIAL_LOGGING 0
#define UNCONNECTED_PIN 14

pixel pixels[NUM_LEDS];
opc_sink sink;

RaverPlaid raverPlaid;
Needles needles;
Bits bitsPattern;
Undulation undulationPattern;
Breathe breathePattern;

Pattern *idlePatterns[] = {&raverPlaid, &needles, &bitsPattern, &undulationPattern, &breathePattern};
const unsigned int kIdlePatternsCount = ARRAY_SIZE(idlePatterns);

Pattern *activePattern = NULL;
Pattern *lastPattern = NULL;

/* ---- Test Options ---- */
const bool kTestPatternTransitions = false;

Pattern *testIdlePattern = NULL;//&bitsPattern;

/* ---------------------- */

unsigned long lastTrigger = 0;
FrameCounter fc;
HomeBridgeListener *hbl;

int first_pattern = -1;
long lastFrame = 0;
const int fps_cap = 60;

#if RASPBERRY_PI
const int modeButtonPin = 18;
#endif
bool displayOn = true;

void set_max(int index, Color c, float bright) {
  pixels[index].r = std::max(pixels[index].r, (uint8_t)(c.red * bright));
  pixels[index].g = std::max(pixels[index].g, (uint8_t)(c.green * bright));
  pixels[index].b = std::max(pixels[index].b, (uint8_t)(c.blue * bright));
}

void set(int index, Color c, float bright) {
  pixels[index].r = c.red * bright;
  pixels[index].g = c.green * bright;
  pixels[index].b = c.blue * bright;
}

void set(int index, Color c) { 
  set(index, c, 1.0);
}

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

  hbl = new HomeBridgeListener();

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

void setDisplayOn(bool on) {
  if (on) {
    nextPattern();
  } else {
    printf("Turning off...\n");
    if (activePattern) {
      activePattern->stop();
      activePattern = NULL;
    }
  }
  displayOn = on;
}

void checkButtons() {
#if RASPBERRY_PI

  static long powerDownInterval = 2000;
  static bool modeButtonPressed = false;
  static long modeButtonPressedMillis = -1;

  bool oldModeButtonPressed = modeButtonPressed;
  modeButtonPressed = (digitalRead(modeButtonPin) == HIGH);

  if (modeButtonPressed && !oldModeButtonPressed) {
    printf("Mode button press down\n");
    modeButtonPressedMillis = millis();
  }
  if (!modeButtonPressed && oldModeButtonPressed 
    && millis() - modeButtonPressedMillis < powerDownInterval) { // don't turn back on if we just turned off
    printf("Mode button press up\n");
    nextPattern();
    displayOn = true;
  }
  if (displayOn && modeButtonPressed && millis() - modeButtonPressedMillis > powerDownInterval) {
    setDisplayOn(false);
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
  }

  // time out idle patterns
  if (activePattern != NULL && activePattern->isRunning() && activePattern->runTime() > (kTestPatternTransitions ? 8 : activePattern->expectedRunDuration * 1000)) {
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
    sleep(2);
  }
#endif

  RemoteCommand command = hbl->poll(displayOn);
  if (command != none) {
    setDisplayOn(command == on);
  }

  long frameTime = millis() - lastFrame;
  if (frameTime < 1000./fps_cap) {
    usleep((1000./fps_cap - frameTime) * 1000);
  }
  lastFrame = millis();
  
  fc.tick();
}

void we_get_signal(int signum)
{
  printf("Caught sig %i!\n", signum);
  setDisplayOn(false);
  long start = millis();
  while (1) {
    loop();
    bool pixelsOn = false;
    for (int i = 0; i < NUM_LEDS; ++i) { \
      if (pixels[i].r != 0 || pixels[i].b != 0 || pixels[i].g != 0) {
        pixelsOn = true;
        break;
      }
    }
    if (!pixelsOn) {
      printf("Turned off.\n");
      break;
    }
    if (millis() - start > 1000) {
      printf("Timed out.\n");
      break;
    }
  }
  exit(signum);
}

void handle_signals() {
    struct sigaction termaction;
    memset(&termaction, 0, sizeof(termaction));
    termaction.sa_handler = we_get_signal;
    sigaction(SIGTERM, &termaction, NULL);

    struct sigaction intaction;
    memset(&intaction, 0, sizeof(intaction));
    intaction.sa_handler = we_get_signal;
    sigaction(SIGINT, &intaction, NULL);
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    first_pattern = atoi(argv[1]);
  }
  handle_signals();
  setup();
  while (1) {
    loop();
  }
}

