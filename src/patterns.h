#ifndef PATTERN_H
#define PATTERN_H

#include <unistd.h>
#include <math.h>
#include <list>
#include <vector>
#include <random>
#include <algorithm>
#include <unordered_set>

#include "ortho.h"
#include "util.h"
#include "palettes.h"

class Pattern {
private:  
  long startTime = -1;
  long stopTime = -1;
  long lastUpdateTime = -1;
public:
  DrawingContext ctx;
  int expectedRunDuration = 40;
  Pattern() { }
  Pattern(int expectedDuration) : expectedRunDuration(expectedDuration) { }
  virtual ~Pattern() { }

  void start() {
    logf("Starting %s", description());
    startTime = millis();
    stopTime = -1;
    setup();
  }

  void loop() {
    update();
    lastUpdateTime = millis();
  }

  virtual bool wantsToIdleStop() {
    return true;
  }

  virtual bool wantsToRun() {
    // for idle patterns that require microphone input and may opt not to run if there is no sound
    return true;
  }

  virtual void setup() { }

  void stop() {
    logf("Stopping %s", description());
    startTime = -1;
  }

  virtual void update() { }
  
  virtual const char *description() = 0;

public:
  bool isRunning() {
    return startTime != -1;
  }

  unsigned long runTime() {
    return startTime == -1 ? 0 : millis() - startTime;
  }

  unsigned long frameTime() {
    return (lastUpdateTime == -1 ? 0 : millis() - lastUpdateTime);
  }

  virtual void colorModeChanged() { }
};


/* --------------------------- */

// FIXME: add auto-palette-rotation
class Needles : public Pattern {
  static const int needleLength = STICK_LENGTH;

  class Needle {
  public:
    int stickIndex;
    float val;
    int direction;
    float speed = 1.0;
    bool active;
    CRGB color;
    int rangeMin;
    int rangeMax;
    Needle(int index, int min, int max) {
      stickIndex = index;
      rangeMin = min;
      rangeMax = max;

      reset();
    }

    void reset() {
      direction = (random() % 2) ? 1 : -1;
      val = direction > 0 ? rangeMin : rangeMax;
      active = true;
    }
        
    void tick() {
      if (!active) return;
      val += speed * direction;
      if (val < rangeMin || val > rangeMax) {
        active = false;
      }
    }
  };

  int leader;
  std::list<Needle *> activeNeedles;
  std::unordered_set<Needle *> inactiveNeedles;
  long lastStartMillis = 0;
  int mode;
  int colorMode;
  Palette *palette;
public:
  Needles() {
    mode = random8(2);
    colorMode = random8(4);
    printf("  mode = %i, colorMode %i\n", mode, colorMode);
    palette = paletteManager.randomPalette();
    lastStartMillis = millis();

    for (int i = 0; i < NUM_LEDS / needleLength; ++i) {
      Needle *needle = new Needle(
        i,
        (mode == 0 ? 0            : -needleLength/2), 
        (mode == 0 ? needleLength : 3 * needleLength/2) - 1);

      inactiveNeedles.insert(needle);
    }
  }
  ~Needles() {
    activeNeedles.clear();
    inactiveNeedles.clear();
  }

  Needle *getActiveNeedle() {
    std::vector<Needle *> sampled;
    size_t nelems = 1;
    std::sample(
        inactiveNeedles.begin(),
        inactiveNeedles.end(),
        std::back_inserter(sampled),
        1,
        std::mt19937{std::random_device{}()}
    );
    Needle *needle = NULL;
    if (sampled.size() > 0) {
      needle = sampled[0];
      needle->reset();
      activeNeedles.push_back(needle);
      inactiveNeedles.erase(needle);
    }
    return needle;
  }

  void update() {
    long mils = millis();

    if (mode == 0) {
      fadeDownBy(0.02, ctx);
    }

    const float stickStartInterval = 10;
    
    long elapsed = mils - lastStartMillis;
    while (elapsed > stickStartInterval) {
      elapsed -= stickStartInterval;
      lastStartMillis = mils;
      
      Needle *needle = getActiveNeedle();
      if (!needle) continue;

      if (colorMode == 0) { // rainbow
        int hue = leader % 0x100;
        needle->color = CRGB::HSB(leader % 0x100, 0xFF, 0xFF);
      } else {  // palette
        needle->color = palette->getRandom();
      }
      needle->speed = (mode == 0 ? 1 : 0.2);
    }
    
    bool hasActiveNeedles = false;
    for (auto it = activeNeedles.begin(); it != activeNeedles.end();) {
      Needle *needle = *it;
      if (needle->active) {
        hasActiveNeedles = true;

        int index = (needle->val + needleLength * needle->stickIndex);
        
        CRGB color = needle->color;
        
        if (mode == 0) {
          ctx.leds[index] = color;
        } else {
          for (int j = 0; j < needleLength; ++j) {
            float bright = 0;
            if (abs(needle->val - j) < needleLength) {
              bright = cos((needle->val - j) / needleLength * M_PI);
            }
            if (bright < 0) {
              bright = 0;
            }

            CRGB c = CRGB::Black;
            c.r = color.r * bright;
            c.g = color.g * bright;
            c.b = color.b * bright;
            ctx.leds[needle->stickIndex * needleLength + j] = c;
          }
        }
        needle->tick();

        ++it;
      } else {
        inactiveNeedles.insert(*it);
        it = activeNeedles.erase(it);
      }
    }

    leader++;
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
      { .maxBits = 35, .bitLifespan = 3000, .updateInterval = 35, .fadedown = 2, .color = white}, // dots enhancer
      { .maxBits = 35, .bitLifespan = 3000, .updateInterval = 45, .fadedown = 5, .color = fromPalette}, // dots enhancer
      // little too frenetic, use as trigger patterns?
      //      { .maxBits = 10, .bitLifespan = 3000, .updateInterval = 0, .fadedown = 20, .color = monotone }, // party streamers
      //      { .maxBits = 10, .bitLifespan = 3000, .updateInterval = 0, .fadedown = 20, .color = mix }, // multi-color party streamers
      { .maxBits = 30, .bitLifespan = 3000, .updateInterval = 1, .fadedown = 15, .color = pink}, // pink triangle
      { .maxBits = 50, .bitLifespan = 3000, .updateInterval = 16, .fadedown = 5, .color = monotone }, // chill streamers
      { .maxBits = 50, .bitLifespan = 3000, .updateInterval = 16, .fadedown = 5, .color = fromPalette}, // palette chill streamers
      { .maxBits = 80, .bitLifespan = 3000, .updateInterval = 16, .fadedown = 20, .color = monotone }, // moving dots
      // { .maxBits = 140, .bitLifespan = 3000, .updateInterval = 350, .fadedown = 5, .color = monotone }, // OG bits pattern  TOO SLOW
      { .maxBits = 12, .bitLifespan = 3000, .updateInterval = 4, .fadedown = 40, .color = monotone }, // chase
    };

    class Bit {
        int8_t direction;
        unsigned long birthdate;
      public:
        unsigned int pos;
        bool alive;
        unsigned long lastTick;
        CRGB color;
        Bit(CRGB color) : color(CRGB::Black) {
          reset(color);
        }
        void reset(CRGB color) {
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

    CRGB color;
    Palette *palette;
  public:
    Bits(int constPreset = -1) : color(CRGB::Black) {
      this->constPreset = constPreset;

      uint8_t pick;
      if (constPreset != -1 && constPreset < ARRAY_SIZE(presets)) {
        pick = constPreset;
        logf("Using const Bits preset %u", pick);
      } else {
        pick = random8(ARRAY_SIZE(presets));
        logf("Picked Bits preset %u", pick);
      }
      preset = presets[pick];

      palette = paletteManager.randomPalette();
      // for monotone
      color = CRGB::HSB(random8(), random8(8) == 0 ? 0 : random8(200, 255), 255);

      bits = (Bit *)calloc(preset.maxBits, sizeof(Bit));
      numBits = 0;
    }

    ~Bits() {
      free(bits);
    }
  private:
    CRGB getBitColor() {
      switch (preset.color) {
        case monotone:
          return color; break;
        case fromPalette:
          return palette->getRandom(); break;
        case mix:
          return CRGB::HSB(random8(), random8(200, 255), 0xFF); break;
        case white:
          return CRGB::White;
        case pink:
        default:
          return CRGB::DeepPink;
      }
    }

    void update() {
      unsigned long mils = millis();
      for (unsigned int i = 0; i < numBits; ++i) {
        Bit *bit = &bits[i];
        if (bit->age() > preset.bitLifespan) {
          bit->alive = false;
        }
        if (bit->alive) {
          CRGB c = CRGB::Black.blendWith(bit->color, bit->ageBrightness());
          ctx.leds[bit->pos].r = c.red;
          ctx.leds[bit->pos].g = c.green;
          ctx.leds[bit->pos].b = c.blue;
          if (mils - bit->lastTick > preset.updateInterval) {
            bit->tick();
          }
        } else {
          bit->reset(getBitColor());
        }
      }

      if (isRunning() && numBits < preset.maxBits && mils - lastBitCreation > preset.bitLifespan / preset.maxBits) {
        bits[numBits++] = Bit(getBitColor());
        lastBitCreation = mils;
      }
      fadeDownBy(1./preset.fadedown, ctx);
    }

    const char *description() {
      return "Bits pattern";
    }
};

/* ------------------- */

class Undulation : public Pattern {
  class Highlight {
  public:
    static const long lifespan = 2000;
    int stick;
    float amount;
    CRGB color;
    long startMillis;
    bool dead = false;
    Highlight() {
      stick = random8(NUM_LEDS / STICK_LENGTH);
      startMillis = millis();
      tick();
    }
    void tick() {
      long duration = millis() - startMillis;
      if (duration > lifespan) {
        dead = true;
        amount = 0;
      } else {
        amount = util_cos(duration, 0.50, lifespan, 0, 1.0);
      }
    }
  };

  std::vector<Highlight> highlights;
  long lastHighlight = 0;

  int submode;
  int baseHue;
public:
  Undulation() {
    highlights.clear();
    submode = random8(2); // FIXME: don't use submode 2 cause it's boring
    printf("  submode %i\n", submode);
    baseHue = random8();
  }

  void update() {

    float t = runTime() / 1000.;
    for (int stick = 0; stick < NUM_LEDS / STICK_LENGTH; ++stick) {
      float period = 4;// + util_cos(t, 0.01 * stick, 10, 0, 1);
      int sat = 0;//fmax(0, util_cos(t, 0.31 * stick, 30, -2*0xFF, 0xFF));
      int hue = 0;//util_cos(t, -0.21 * stick, 40, 0, 0xFF);
      for (int i = 0; i < STICK_LENGTH; ++i) {
        // TODO: offset could be improved here to show more difference in phase from stick to stick

        // TODO: add submode where a couple lights just go back and forth across each stick smoothly



        int bright = fmax(0, util_cos(t, 0.01 * stick + 0.1 * i, period, -30, 0x60));
        int index = STICK_LENGTH * stick + i;

        CRGB c;
        if (submode == 0) {
          c = CRGB::HSB(hue, sat, bright);
        } else {
          c = CRGB::HSB(baseHue, 0xFF, bright);
        }
        
        ctx.leds[index] = c;
      }
    }

    if (millis() - lastHighlight > 140) {
      Highlight h;
      if (submode == 0) {
        h.color = CRGB::HSB(random8(), 0xFF, 0xFF);
        highlights.push_back(h);
      } else if (submode == 1) {
        h.color = CRGB::HSB(0, 0, 0xFF);
        highlights.push_back(h);
      } else if (submode == 2) {
        // no highlights
      }
      
      lastHighlight = millis();
    }
    for (std::vector<Highlight>::iterator it = highlights.begin(); it < highlights.end(); ++it) {
      for (int i = 0; i < STICK_LENGTH; ++i) {
        int index = it->stick * STICK_LENGTH + i;
        ctx.leds[index].r = it->amount * it->color.red + (1-it->amount) * ctx.leds[index].r;
        ctx.leds[index].g = it->amount * it->color.green + (1-it->amount) * ctx.leds[index].g;
        ctx.leds[index].b = it->amount * it->color.blue + (1-it->amount) * ctx.leds[index].b;
      }

      it->tick();
      if (it->dead) {
        it = highlights.erase(it);
      }
    }
  }
  const char *description() {
    return "Undulation Pattern";
  }
};

/* ------------------- */

class RaverPlaid : public Pattern {
  // how many sine wave cycles are squeezed into our n_pixels
  // 24 happens to create nice diagonal stripes on the wall layout
  const int default_freq = 24;
  float freq_r = default_freq;
  float freq_g = default_freq;
  float freq_b = default_freq;
  int mode;
public:
  RaverPlaid() {
    if (random8(2) == 0) {
      printf("  Default frequencies\n");
      freq_r = default_freq;
      freq_g = default_freq;
      freq_b = default_freq;
    } else {
      freq_r = random8(18, 30);
      freq_g = random8(18, 30);
      freq_b = random8(18, 30);;
      printf("  frequencies %f, %f, %f\n", freq_r, freq_g, freq_b);
    }
    // 20% chance to cut out each channel
    mode = random8(5);
  }

  void update() {
    // Demo code from Open Pixel Control
    // http://github.com/zestyping/openpixelcontrol
    int n_pixels = NUM_LEDS;

    // how many seconds the color sine waves take to shift through a complete cycle
    float speed_r = 7;
    float speed_g = -13;
    float speed_b = 19;
    float t = runTime() / 1000. * 5;
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

        r = (mode == 2 ? 0 : r);
        g = (mode == 3 ? 0 : g);
        b = (mode == 4 ? 0 : b);

        const float brightness = 0.7;
        ctx.leds[ii].r = r * brightness;
        ctx.leds[ii].g = g * brightness;
        ctx.leds[ii].b = b * brightness;
    }
    // usleep(1. / fps * 1000000);
  }
  const char *description() {
    return "Raver Plaid";
  }
};

/* ------------------- */

class Breathe : public Pattern {
  class Stick {
  public:
    int stick;
    long startMillis;
    int direction = 0;
    void fadeUp() {
      startMillis = millis();
      direction = 1;
    }
    void fadeDown() {
      startMillis = millis();
      direction = -1;
    }
    float amount() {
      float progress = (millis() - startMillis) / 500.;
      return fmax(0, fmin(1.0, direction > 0 ? progress : 1 - progress));
    }
  };

  float lastValue;
  int hueOffset;
  int mode;

  std::vector<Stick> sticks;
  int generator = -1;
public:
  Breathe() : Pattern(21) {
    sticks.clear();
    lastValue = -1;
    mode = 1;//random8(2);
    printf("  mode %i\n", mode);

    if (random8(3) == 0) {
      hueOffset = -1;
    } else {
      hueOffset = random8();
    }
    printf("  hue offset %i\n", hueOffset);
  }
private:

  void linearBreathe() {
    float value = util_cos(runTime(), 0, 8000, 0, STRIP_LENGTH);
    if (lastValue == -1) {
      lastValue = value;
    }

    // we want to gently fade in the next light
    float integral;
    float frac = modff(value, &integral);
    int index = (int)value;
    int prelightOffset = (lastValue > value ? -1 : 1);
    int prelightBrightness = (lastValue > value ? 1 - frac : frac) * 0xFF;

    for (int s = 0; s < STRIP_COUNT; ++s) {
      int prelightIndex = fmax(0, fmin(STRIP_LENGTH - 1, index + prelightOffset)) + s * STRIP_LENGTH;
      int lightIndex    = fmax(0, fmin(STRIP_LENGTH - 1, index)) + s * STRIP_LENGTH;

      int saturation = (hueOffset == -1 ? 0 : 200);

      CRGB prelightColor = CRGB::HSB(hueOffset + index, saturation, prelightBrightness);
      CRGB lightColor = CRGB::HSB(hueOffset + index, saturation, 0xFF);

      ctx.leds[prelightIndex].r = prelightColor.red;
      ctx.leds[prelightIndex].g = prelightColor.green;
      ctx.leds[prelightIndex].b = prelightColor.blue;
      
      ctx.leds[lightIndex].r = lightColor.red;
      ctx.leds[lightIndex].g = lightColor.green;
      ctx.leds[lightIndex].b = lightColor.blue;
    }
    lastValue = value;
  }

  void popcornBreathe() {
    // use generators for group of size 96
    int numSticks = NUM_LEDS / STICK_LENGTH;
    const int generators[] = {37, 53, 67, 83, 101, 137, 163};

    float value = util_cos(runTime(), 0.5, 7000, -4, numSticks);
    if (lastValue == -1) {
      lastValue = value;
    }
    if (value < 0) {
      // pause in between
      generator = -1;
    } else if (generator == -1) {
      generator = generators[random8(ARRAY_SIZE(generators))];
      for (std::vector<Stick>::iterator it = sticks.begin(); it != sticks.end(); ++it) {
        it->direction = 0;
      }
    }
    
    if (lastValue > value) {
      for (int i = sticks.size() - 1; i >= 0 && i >= value; --i) {
        if (sticks[i].direction != -1) {
          sticks[i].fadeDown();
        }
      }
    } else {
      for (int i = 0; i < value; ++i) {
        while (i > (long)sticks.size() - 1) {
          Stick s;
          sticks.push_back(s);
        }
        if (sticks[i].direction != 1) {
          sticks[i].stick = ((i + 1) * generator) % numSticks;
          sticks[i].fadeUp();
        }
      }
    }

    float alphaLimiter = 0.4;
    int saturation = (hueOffset == -1 ? 0 : 200);
    
    for (std::vector<Stick>::iterator it = sticks.begin(); it != sticks.end(); ++it) {
      if (it->direction == 0) {
        continue;
      }
      int index = it->stick;
      CRGB lightColor = CRGB::HSB(hueOffset, saturation, it->amount() * 0xFF);
      for (int i = 0; i < STICK_LENGTH; ++i) {
        ctx.leds[index * STICK_LENGTH + i].r = alphaLimiter * lightColor.red;
        ctx.leds[index * STICK_LENGTH + i].g = alphaLimiter * lightColor.green;
        ctx.leds[index * STICK_LENGTH + i].b = alphaLimiter * lightColor.blue;
      }
    }
    lastValue = value;
  }

  void update() {
    if (mode == 0) {
      linearBreathe();
    } else {
      popcornBreathe();
    }
    fadeDownBy(0.02, ctx);
  }

  const char *description() {
    return "Breathe";
  }
};

#endif


