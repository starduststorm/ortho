#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

#ifndef __WIRING_PI_H__
long millis() {
  static long start_sec = 0;
  struct timeval tv;
  if (start_sec == 0) {
    gettimeofday(&tv, (struct timezone *)0);
    start_sec = tv.tv_sec;
  }
  gettimeofday(&tv, (struct timezone *)0);
  return (unsigned long)(1000 * (tv.tv_sec - start_sec) + tv.tv_usec / 1000.);
}
#endif

#define fadeDownBy(amt)     for (int i = 0; i < NUM_LEDS; ++i) { \
      pixels[i].r *= (1 - amt); \
      pixels[i].g *= (1 - amt); \
      pixels[i].b *= (1 - amt); \
    }

void logf(const char *format, ...)
{
  va_list argptr;
  va_start(argptr, format);
  char *buf = (char *)calloc(strlen(format) + 200, sizeof(char));
  vsnprintf(buf, 200, format, argptr);
  va_end(argptr);
#if SERIAL_LOGGING
  Serial.println(buf);
#endif
  printf("%s\n", buf);
  free(buf);
}

#define MOD_DISTANCE(a, b, m) (abs(m / 2. - fmod((3 * m) / 2 + a - b, m)))

inline int mod_wrap(int x, int m) {
  int result = x % m;
  return result < 0 ? result + m : result;
}

inline float fmod_wrap(float x, int m) {
  float result = fmod(x, m);
  return result < 0 ? result + m : result;
}

class FrameCounter {
  private:
    long lastPrint = 0;
    long frames = 0;
  public:
    long printInterval = 2000;
    void tick() {
      unsigned long mil = millis();
      long elapsed = mil - lastPrint;
      if (elapsed > printInterval) {
        if (lastPrint != 0) {
          printf("Framerate: %f\n", frames / (float)elapsed * 1000);
        }
        frames = 0;
        lastPrint = mil;
      }
      ++frames;
    }
};

uint8_t random8(uint8_t lowerBound, int upperBound) {
  return random() % (upperBound - lowerBound) + lowerBound;
}

uint8_t random8(int upperBound) {
  return random8(0, upperBound);
}

uint8_t random8() {
  return random() % 0x100;
}

class Color {
private:
  void _init(uint8_t red, uint8_t green, uint8_t blue) {
    this->red = red;
    this->green = green;
    this->blue = blue;
  }
  Color(uint8_t red, uint8_t green, uint8_t blue) : _description(NULL) {
    _init(red, green, blue);
  }
  char *_description = NULL; // cache
public:
  uint8_t red, green, blue;

  Color(const Color &oldc) {
    red = oldc.red;
    green = oldc.green;
    blue = oldc.blue;
  }

  ~Color() {
    free(_description);
  }

  uint8_t blend8(uint8_t a, uint8_t b, float amount) {
    return (1-amount) * a + amount * b;
  }

  Color blendWith(Color c2, float amount) {
    amount = fmax(0.0, fmin(1.0, amount));
    uint8_t r = blend8(red, c2.red, amount);
    uint8_t g = blend8(green, c2.green, amount);
    uint8_t b = blend8(blue, c2.blue, amount);
    return Color::RGB(r, g, b);
  }

  char *description() {
    if (_description) {
      free(_description);
    }
    _description = (char *)malloc(20 * sizeof(char));
    snprintf(_description, 20, "(%i, %i, %i)\n", red, green, blue);
    return _description;
  }

  //

  static Color White;
  static Color Black;
  static Color DeepPink;

  static Color RGB(uint8_t red, uint8_t green, uint8_t blue) {
    return Color(red, green, blue);
  }
  static Color HSB(uint8_t hue, uint8_t sat, uint8_t bright) {
    float sat_f = (float)sat / 0xFF;
    float bright_f = (float)sat / 0xFF;
    float c = bright_f * sat_f;
    float x = c * (1 - fabs(fmod_wrap(hue / (0xFF/6.), 2) - 1));
    float m = bright_f - c;
    float rp, gp, bp;
    if (hue < 1 * 0xFF/6.) {
      rp = c; gp = x; bp = 0;
    } else if (hue < 2 * 0xFF/6.) {
      rp = x; gp = c; bp = 0;
    } else if (hue < 3 * 0xFF/6.) {
      rp = 0; gp = c; bp = x;
    } else if (hue < 4 * 0xFF/6.) {
      rp = 0; gp = x; bp = c;
    } else if (hue < 5 * 0xFF/6.) {
      rp = x; gp = 0; bp = c;
    } else {
      rp = c; gp = 0; bp = x;
    }
    return Color((rp + m) * 0xFF, (gp + m) * 0xFF, (bp + m) * 0xFF);
  }
};

Color Color::White = Color::RGB(0xFF, 0xFF, 0xFF);
Color Color::Black = Color::RGB(0, 0, 0);
Color Color::DeepPink = Color::RGB(0xFF, 0x14, 0x93);

#endif
