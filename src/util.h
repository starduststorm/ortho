#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>

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

#define fadeDownBy(amt, ctx)     for (int i = 0; i < NUM_LEDS; ++i) { \
      ctx.leds[i].r *= (1 - amt); \
      ctx.leds[i].g *= (1 - amt); \
      ctx.leds[i].b *= (1 - amt); \
    }

static void _logf(bool newline, const char *format, va_list argptr)
{
  if (strlen(format) == 0) {
    if (newline) {
      printf("\n");
    }
    return;
  }
  char *buf;
  vasprintf(&buf, format, argptr);
  if (newline) {
    printf("%s\n", buf ? buf : "LOGF MEMORY ERROR");
  } else {
    printf("%s", buf ? buf : "LOGF MEMORY ERROR");
  }
#if DEBUG
  fflush(stdout);
  fflush(stderr);
#endif
  free(buf);
}

void logf(const char *format, ...)
{
  va_list argptr;
  va_start(argptr, format);
  _logf(true, format, argptr);
  va_end(argptr);
}

void loglf(const char *format, ...)
{
  va_list argptr;
  va_start(argptr, format);
  _logf(false, format, argptr);
  va_end(argptr);
}

void assert_func(bool result, const char *pred, const char *reasonFormat, ...) {
  if (!result) {
    logf("ASSERTION FAILED: %s", pred);
    va_list argptr;
    va_start(argptr, reasonFormat);
    logf(reasonFormat, argptr);
    va_end(argptr);
#if DEBUG
    while (1) sleep(100);
#endif
  }
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
    long printInterval = 5000;
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

// FIXME: kind of lies because this isn't actually an 8-bit random, just named to match FastLED
uint8_t random8(uint8_t lowerBound, int upperBound) {
  return random() % (upperBound - lowerBound) + lowerBound;
}

uint8_t random8(int upperBound) {
  return random8(0, upperBound);
}

uint8_t random8() {
  // FIXME: replace with SRNG
  return random() % 0x100;
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

///

// slightly cribbed from FastLED

typedef uint8_t   fract8;   ///< ANSI: unsigned short _Fract


static void nscale8x3( uint8_t& r, uint8_t& g, uint8_t& b, fract8 scale)
{
    uint16_t scale_fixed = scale + 1;
    r = (((uint16_t)r) * scale_fixed) >> 8;
    g = (((uint16_t)g) * scale_fixed) >> 8;
    b = (((uint16_t)b) * scale_fixed) >> 8;
}

typedef struct CRGB {
  union {
    struct {
      union {
        uint8_t r;
        uint8_t red;
      };
      union {
        uint8_t g;
        uint8_t green;
      };
      union {
        uint8_t b;
        uint8_t blue;
      };
    };
    uint8_t raw[3];
  };
public:
  CRGB() { }
  CRGB(uint8_t r, uint8_t g, uint8_t b) : red(r), green(g), blue(b) { }

  inline uint8_t& operator[] (uint8_t x) __attribute__((always_inline)) {
    return raw[x];
  }

  uint8_t blend8(uint8_t a, uint8_t b, float amount) {
    return (1-amount) * a + amount * b;
  }

  /// scale down a RGB to N 256ths of it's current brightness, using
  /// 'plain math' dimming rules, which means that if the low light levels
  /// may dim all the way to 100% black.
  inline CRGB& nscale8 (uint8_t scaledown )
  {
      nscale8x3( r, g, b, scaledown);
      return *this;
  }

  // FIXME: make blending a drop in for FastLED
  CRGB blendWith(CRGB c2, float amount) {
    amount = fmax(0.0, fmin(1.0, amount));
    uint8_t r = blend8(red, c2.red, amount);
    uint8_t g = blend8(green, c2.green, amount);
    uint8_t b = blend8(blue, c2.blue, amount);
    return CRGB::RGB(r, g, b);
  }

  static CRGB White;
  static CRGB Black;
  static CRGB Red;
  static CRGB DeepPink;

  static CRGB RGB(uint8_t red, uint8_t green, uint8_t blue) {
    return CRGB(red, green, blue);
  }
  static CRGB HSB(uint8_t hue, uint8_t sat, uint8_t bright) {
    float sat_f = (float)sat / 0xFF;
    float bright_f = (float)bright / 0xFF;
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
    return CRGB((rp + m) * 0xFF, (gp + m) * 0xFF, (bp + m) * 0xFF);
  }
} CRGB;

CRGB CRGB::White = CRGB::RGB(0xFF, 0xFF, 0xFF);
CRGB CRGB::Black = CRGB::RGB(0, 0, 0);
CRGB CRGB::Red = CRGB::RGB(0xFF, 0, 0);
CRGB CRGB::DeepPink = CRGB::RGB(0xFF, 0x14, 0x93);

inline uint8_t scale8( uint8_t i, fract8 scale)
{
    return (((uint16_t)i) * (1+(uint16_t)(scale))) >> 8;
}

uint8_t dim8_raw( uint8_t x)
{
    return scale8( x, x);
}

float easeInOut(float t)
{
    float sqt = t * t;
    return sqt / (2.0f * (sqt - t) + 1.0f);
}

#endif

#undef assert
#define assert(expr, reasonFormat, ...) assert_func((expr), #expr, reasonFormat, ## __VA_ARGS__);
