#ifndef PATTERNMANAGER_H
#define PATTERNMANAGER_H

#include <vector>

#include "patterns.h"

static const bool patternAutoRotateDefault = true;

template <typename BufferType>
class PatternManager {
  int patternIndex = -1;

  Pattern *activePattern = NULL;
  Pattern *previousActivePattern = NULL;
  const unsigned long crossfadeDuration = 1000;
  unsigned long activePatternStart = 0;

  bool patternAutoRotate = patternAutoRotateDefault;

  std::vector<Pattern * (*)(void)> patternConstructors;

  BufferType &ctx;

  template<class T>
  static Pattern *construct() {
    return new T();
  }

  // Make testIdlePattern in this constructor instead of at global so the Pattern doesn't get made at launch
  Pattern *TestIdlePattern() {
    static Pattern *testIdlePattern = NULL;
    if (testIdlePattern == NULL) {

    }
    return testIdlePattern;
  }

  void paletteAutorotateWelcome() {
  }

  void patternAutorotateWelcome() {
  }

public:
  PatternManager(BufferType &ctx) : ctx(ctx) {
    patternConstructors.push_back(&(construct<RaverPlaid>));
    patternConstructors.push_back(&(construct<Needles>));
    patternConstructors.push_back(&(construct<Bits>));
    patternConstructors.push_back(&(construct<Undulation>));
    patternConstructors.push_back(&(construct<Breathe>));
  }

  ~PatternManager() {
    delete activePattern;
    delete previousActivePattern;
  }

  void nextPattern() {
    patternAutoRotate = patternAutoRotateDefault;
    patternIndex = (patternIndex + 1) % patternConstructors.size();
    if (!startPatternAtIndex(patternIndex)) {
      nextPattern();
    }
  }

  void previousPattern() {
    patternAutoRotate = patternAutoRotateDefault;
    patternIndex = mod_wrap(patternIndex - 1, patternConstructors.size());
    if (!startPatternAtIndex(patternIndex)) {
      previousPattern();
    }
  }

  void enablePatternAutoRotate() {
    logf("Enable pattern autorotate");
    patternAutoRotate = true;
    patternAutorotateWelcome();
    prepareForNextPattern();
  }

  void prepareForNextPattern() {
    if (activePattern) {
      previousActivePattern = activePattern;
      activePattern = NULL;
    }
  }

  void stopPattern() {
    if (activePattern) {
      activePattern->stop();
      delete activePattern;
      activePattern = NULL;
    }
  }

  void cleanupPreviousPattern() {
    if (previousActivePattern) {
      previousActivePattern->stop();
      delete previousActivePattern;
      previousActivePattern = NULL;
    }
  }
  
  // Palettes

  void nextPalette() {
    if (activePattern) {
      activePattern->colorModeChanged();
    }
  }

  void previousPalette() {
    if (activePattern) {
      activePattern->colorModeChanged();
    }
  }

  void enablePaletteAutoRotate() {
    logf("Enable palette autorotate");
    paletteAutorotateWelcome();
    if (activePattern) {
      activePattern->colorModeChanged();
    }
  }

private:
  bool startPatternAtIndex(int index) {
    prepareForNextPattern();
    auto ctor = patternConstructors[index];
    Pattern *nextPattern = ctor();
    if (startPattern(nextPattern)) {
      patternIndex = index;
      return true;
    } else {
      delete nextPattern; // patternConstructors returns retained
      return false;
    }
  }
public:
  bool startPattern(Pattern *pattern) {
    prepareForNextPattern();
    if (pattern->wantsToRun()) {
      pattern->start();
      activePatternStart = millis();
      activePattern = pattern;
      return true;
    } else {
      return false;
    }
  }

  void setup() {
    startPatternAtIndex(1);
  }

  void loop() {
    for (int i = 0; i < NUM_LEDS; ++i) {
      ctx.leds[i] = CRGB::Black;
    }

    if (activePattern && activePattern->runTime() > crossfadeDuration) {
      cleanupPreviousPattern();
    }

    uint8_t activePatternBrightness = 0xFF;
    if (previousActivePattern || activePatternStart != 0 && millis() - activePatternStart < crossfadeDuration) {
      activePatternBrightness = (activePattern ? 0xFF * (activePattern->runTime() / (float)crossfadeDuration) : 0);
    }

    if (previousActivePattern) {  
      previousActivePattern->loop();
      previousActivePattern->ctx.blendIntoContext(ctx, BlendMode::blendBrighten, dim8_raw(0xFF - activePatternBrightness));
    }

    if (activePattern) {
      activePattern->loop();
      activePattern->ctx.blendIntoContext(ctx, BlendMode::blendBrighten, dim8_raw(activePatternBrightness));
    }

    // time out idle patterns
    if (patternAutoRotate && activePattern != NULL && activePattern->isRunning() && activePattern->runTime() > activePattern->expectedRunDuration * 1000) {
      if (activePattern != TestIdlePattern() && activePattern->wantsToIdleStop()) {
        prepareForNextPattern();
      }
    }

    // start a new random pattern if there is none
    if (activePattern == NULL) {
      Pattern *testPattern = TestIdlePattern();
      if (testPattern) {
        startPattern(testPattern);
      } else {
        int choice = (int)random8(patternConstructors.size());
        startPatternAtIndex(choice);
      }
    }
  }
};

#endif
