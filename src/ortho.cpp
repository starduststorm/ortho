#define DEBUG 0

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
#include "PatternManager.h"
#include "HomeBridgeListener.h"

#define SERIAL_LOGGING 0
#define UNCONNECTED_PIN 14

opc_sink sink;

DrawingContext ctx;
PatternManager<DrawingContext> patternManager(ctx);

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

bool allPixelsOff() {
  for (int i = 0; i < NUM_LEDS; ++i) {
    if (ctx.leds[i].r != 0 || ctx.leds[i].g != 0 || ctx.leds[i].b != 0) {
      return false;
    }
  }
  return true;
}

void setup() {
#if RASPBERRY_PI
  sink = opc_new_sink((char *)"127.0.0.1:7890");
#else
  sink = opc_new_sink((char *)"10.0.0.100:7890");
#endif
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

  patternManager.setup();

  fc.tick();
}

void setDisplayOn(bool on) {
  if (on) {
    patternManager.nextPattern();
  } else {
    printf("Turning off...\n");
    patternManager.stopPattern();
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
    patternManager.nextPattern();
    displayOn = true;
  }
  if (displayOn && modeButtonPressed && millis() - modeButtonPressedMillis > powerDownInterval) {
    setDisplayOn(false);
  }
#endif
}

void loop() {
  checkButtons();

  if (!displayOn) {
    if (allPixelsOff()) {
      sleep(3);
    }
    fadeDownBy(0.1, ctx);
  } else {
    patternManager.loop();
  }

  if (0 == opc_put_pixels(sink, 0, NUM_LEDS, (pixel*)ctx.leds)) {
    // Failed to connect to fadecandy, don't spam it
    printf("opc_put_pixels failed");
    sleep(2);
  }

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
    if (allPixelsOff()) {
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

