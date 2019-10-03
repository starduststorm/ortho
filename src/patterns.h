#ifndef PATTERN_H
#define PATTERN_H

#include "opc/opc.h"
#include "util.h"
#include "palettes.h"
#include <unistd.h>
#include "math.h"

class Pattern {
  protected:
    long startTime = -1;
    long stopTime = -1;
    Pattern *subPattern = NULL;

    void stopCompleted() {
      if (!readyToStop()) {
        logf("WARNING: stopped %s before subPattern was stopped", description());
      }
      logf("Stopped %s", description());
      stopTime = -1;
      startTime = -1;
      if (subPattern) {
        subPattern->stop();
        delete subPattern;
        subPattern = NULL;
      }
    }

    virtual Pattern *makeSubPattern() {
      return NULL;
    }

    bool readyToStop() {
      return subPattern == NULL || subPattern->isStopped();
    }

  public:
    virtual ~Pattern() { }

    virtual bool wantsToRun() {
      // for idle patterns that require microphone input and may opt not to run if there is no sound
      return true;
    }

    void start() {
      logf("Starting %s", description());
      startTime = millis();
      stopTime = -1;
      setup();
      subPattern = makeSubPattern();
      if (subPattern) {
        subPattern->start();
      }
    }

    void loop(pixel pixels[]) {
      update(pixels);
      if (subPattern) {
        subPattern->update(pixels);
      }
    }

    virtual void setup() { }

    virtual bool wantsToIdleStop() {
      return true;
    }

    virtual void lazyStop() {
      if (isRunning()) {
        logf("Stopping %s", description());
        stopTime = millis();
      }
      if (subPattern) {
        subPattern->lazyStop();
      }
    }

    void stop() {
      if (subPattern) {
        subPattern->stop();
      }
      stopCompleted();
    }

    bool isRunning() {
      return startTime != -1 && isStopping() == false;
    }

    bool isStopped() {
      return !isRunning() && !isStopping();
    }

    long runTime() {
      return startTime == -1 ? 0 : millis() - startTime;
    }

    bool isStopping() {
      return stopTime != -1;
    }

    virtual void update(pixel[]) = 0;
    virtual const char *description() = 0;

    // Sub patterns (for pattern mixing)
    void setSubPattern(Pattern *pattern) {
      subPattern = pattern;
      if (isRunning()) {
        subPattern->start();
      }
    }
};


/* --------------------------- */


class Needles : public Pattern {
  static const int needleLength = 8;

  class Needle {
  public:
    int light;
    int direction;
    bool active;
    Needle() {
      reset();
      active = false; // start out inactive
    }
    
    void reset() {
      direction = (random() % 2) ? 1 : -1;
      light = direction > 0 ? 0 : needleLength - 1;
      active = true;
    }
    
    
    void tick() {
      if (!active) return;
      light += direction;
      if (light < 0 || light >= needleLength) {
        active = false;
      }
    }
  };

  int leader;
  Needle *needles;
  int numNeedles;
  long lastStartMillis = 0;
public:
  Needles() {
    numNeedles = NUM_LEDS / needleLength;
    needles = new Needle[numNeedles];
  }
  ~Needles() {
    delete[] needles;
  }
  void update(pixel pixels[]) {
    long mils = millis();

    fadeDownBy(0.02);

    const float density = 1;//max(0.01, 1 + sin(millis / 1000. / 5));
    
    long elapsed = mils - lastStartMillis;

    if (!isStopping()) {
      for (int wait = 0; wait < elapsed; wait += 20./density) { 
        int stick;
        int attempts = 0;
        do {
          stick = random() % numNeedles;
        } while (needles[stick].active == true && attempts++ < 20);
        if (attempts < 20) {
          needles[stick].reset();
        }
        lastStartMillis = mils;
      }
    }
    
    int activeNeedles = 0;
    for (int i = 0; i < numNeedles; ++i) {
      if (needles[i].active) {
        activeNeedles++;
        int index = (needles[i].light + needleLength * i);
        
        int hue = leader % 0x100;
        Color color = Color::HSB(leader % 0x100, 0xFF, 0xFF);
        // rainbow
        pixels[index].r = color.red;
        pixels[index].g = color.green;
        pixels[index].b = color.blue;

        //set(index % 64, index / 64, Palette_ROYGBIV.sample(millis()/4.0/1000));
        
        // palette
        //set(index % 64, index / 64, palette.getColor((needle.counter) 0x100));
        
        // cool accidental twinkly effect due to `i` scaling down as `needles` is cleared out
        //set(index % 64, index / 64, palette.getColor((leader+10*i) % 0x100));
        
        
        // oscillate
        //float hue = centerColor + 30*sin(millis()*PI/1000);
        //println("center: " + centerColor + ", hue: " + hue);
        //set(index % 64, index / 64, color(hue, 100, 100));
        needles[i].tick();
        }
      }
    // centerColor += 0.05;
    // centerColor = centerColor % 100;
    leader++;
    if (isStopping() && activeNeedles == 0) {
      stopCompleted();
    }
  }
  const char *description() {
    return "Needles";
  }
};

/* -------------------- */

class Bits : public Pattern {
    enum BitColor {
      monotone, fromPalette, mix, white, pink
    };
    typedef struct _BitsPreset {
      unsigned int maxBits, bitLifespan, updateInterval, fadedown;
      BitColor color;
    } BitsPreset;

    BitsPreset presets[8] = {
      { .maxBits = 15, .bitLifespan = 3000, .updateInterval = 35, .fadedown = 5, .color = white}, // dots enhancer
      { .maxBits = 15, .bitLifespan = 3000, .updateInterval = 45, .fadedown = 5, .color = fromPalette}, // dots enhancer
      // little too frenetic, use as trigger patterns?
      //      { .maxBits = 10, .bitLifespan = 3000, .updateInterval = 0, .fadedown = 20, .color = monotone }, // party streamers
      //      { .maxBits = 10, .bitLifespan = 3000, .updateInterval = 0, .fadedown = 20, .color = mix }, // multi-color party streamers
      { .maxBits = 30, .bitLifespan = 3000, .updateInterval = 1, .fadedown = 15, .color = pink}, // pink triangle
      { .maxBits = 50, .bitLifespan = 3000, .updateInterval = 16, .fadedown = 5, .color = monotone }, // chill streamers
      { .maxBits = 50, .bitLifespan = 3000, .updateInterval = 16, .fadedown = 5, .color = fromPalette}, // palette chill streamers
      { .maxBits = 80, .bitLifespan = 3000, .updateInterval = 16, .fadedown = 30, .color = monotone }, // moving dots
      { .maxBits = 140, .bitLifespan = 3000, .updateInterval = 350, .fadedown = 5, .color = monotone }, // OG bits pattern
      { .maxBits = 12, .bitLifespan = 3000, .updateInterval = 4, .fadedown = 50, .color = monotone }, // chase
    };

    class Bit {
        int8_t direction;
        unsigned long birthdate;
      public:
        unsigned int pos;
        bool alive;
        unsigned long lastTick;
        Color color;
        Bit(Color color) : color(Color::Black) {
          reset(color);
        }
        void reset(Color color) {
          birthdate = millis();
          alive = true;
          pos = random() % NUM_LEDS;
          direction = random() % 2 == 0 ? 1 : -1;
          this->color = color;
        }
        unsigned int age() {
          return millis() - birthdate;
        }
        float ageBrightness() {
          // FIXME: assumes 3000ms lifespan
          float theAge = age();
          if (theAge < 500) {
            return theAge / 500.;
          } else if (theAge > 2500) {
            return (3000 - theAge) / 500.;
          }
          return 1.0;
        }
        void tick() {
          pos = mod_wrap(pos + direction, NUM_LEDS);
          lastTick = millis();
        }
    };

    Bit *bits;
    unsigned int numBits;
    unsigned int lastBitCreation;
    BitsPreset preset;
    char constPreset;

    Color color;
    Palette *palette;
  public:
    Bits(int constPreset = -1) : color(Color::Black) {
      this->constPreset = constPreset;
    }
  private:

    Color getBitColor() {
      switch (preset.color) {
        case monotone:
          return color; break;
        case fromPalette:
          return palette->getRandom(); break;
        case mix:
          return Color::HSB(random8(), random8(200, 255), 0xFF); break;
        case white:
          return Color::White;
        case pink:
        default:
          return Color::DeepPink;
      }
    }

    void setup() {
      uint8_t pick;
      if (constPreset != -1 && constPreset < ARRAY_SIZE(presets)) {
        pick = constPreset;
        logf("Using const Bits preset %u", pick);
      } else {
        pick = random8(ARRAY_SIZE(presets));
        logf("Picked Bits preset %u", pick);
      }
      preset = presets[pick];

      palette = &(paletteManager.palettes[random() % ARRAY_SIZE(gGradientPalettes)]);
      // for monotone
      color = Color::HSB(random8(), random8(8) == 0 ? 0 : random8(200, 255), 255);

      bits = (Bit *)calloc(preset.maxBits, sizeof(Bit));
      numBits = 0;
    }

    void update(pixel pixels[]) {
      unsigned long mils = millis();
      bool hasAliveBit = false;
      for (unsigned int i = 0; i < numBits; ++i) {
        Bit *bit = &bits[i];
        if (bit->age() > preset.bitLifespan) {
          bit->alive = false;
        }
        if (bit->alive) {
          Color c = Color::Black.blendWith(bit->color, bit->ageBrightness());
          pixels[bit->pos].r = c.red;
          pixels[bit->pos].g = c.green;
          pixels[bit->pos].b = c.blue;
          if (mils - bit->lastTick > preset.updateInterval) {
            bit->tick();
          }
          hasAliveBit = true;
        } else if (!isStopping()) {
          bit->reset(getBitColor());
          hasAliveBit = true;
        }
      }

      if (isRunning() && numBits < preset.maxBits && mils - lastBitCreation > preset.bitLifespan / preset.maxBits) {
        bits[numBits++] = Bit(getBitColor());
        lastBitCreation = mils;
      }
      if (!isStopping()) {
        fadeDownBy(1./preset.fadedown);
      } else if (!hasAliveBit) {
        stopCompleted();
      }
    }

    void stopCompleted() {
      Pattern::stopCompleted();
      if (bits) {
        free(bits);
        bits = NULL;
        numBits = 0;
      }
    }

    const char *description() {
      return "Bits pattern";
    }
};

/* ------------------- */

class RaverPlaid : public Pattern {
  void start() {
    Pattern::start();
  }

  float remap(float x, float oldmin, float oldmax, float newmin, float newmax) {
    float zero_to_one = (x-oldmin) / (oldmax-oldmin);
    return zero_to_one*(newmax-newmin) + newmin;
  }

  float util_cos(float x, float offset, float period, float minn, float maxx) {
      float value = cos((x/period - offset) * M_PI * 2) / 2 + 0.5;
      return value*(maxx-minn) + minn;
  }

  float clamp(float x, float minn, float maxx) {
    return fmax(minn, fmin(maxx, x));
  }

  void update(pixel pixels[]) {
    // Demo code from Open Pixel Control
    // http://github.com/zestyping/openpixelcontrol
    int n_pixels = NUM_LEDS;

    // how many sine wave cycles are squeezed into our n_pixels
    // 24 happens to create nice diagonal stripes on the wall layout
    float freq_r = 24;
    float freq_g = 24;
    float freq_b = 24;

    // how many seconds the color sine waves take to shift through a complete cycle
    float speed_r = 7;
    float speed_g = -13;
    float speed_b = 19;
    float t = runTime() / 1000. * 5;
    // pixels = [];
    for (int ii = 0; ii < n_pixels; ++ii) {
        float pct = (ii / (float)n_pixels);
        // diagonal black stripes
        float pct_jittered = fmod_wrap(pct * 77, 37);
        float blackstripes = util_cos(pct_jittered, t*0.05, 1, -1.5, 1.5);
        float blackstripes_offset = util_cos(t, 0.9, 60, -0.5, 3);
        blackstripes = clamp(blackstripes + blackstripes_offset, 0, 1);
        // 3 sine waves for r, g, b which are out of sync with each other
        char r = blackstripes * remap(cos((t/speed_r + pct*freq_r)*M_PI*2), -1, 1, 0, 255);
        char g = blackstripes * remap(cos((t/speed_g + pct*freq_g)*M_PI*2), -1, 1, 0, 255);
        char b = blackstripes * remap(cos((t/speed_b + pct*freq_b)*M_PI*2), -1, 1, 0, 255);
        pixels[ii].r = r;
        pixels[ii].g = g;
        pixels[ii].b = b;
    }
    if (isStopping()) {
      stopCompleted();
    }
    // usleep(1. / fps * 1000000);
  }
  const char *description() {
    return "Raver Plaid";
  }
};

#endif


