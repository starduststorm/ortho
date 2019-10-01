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

class Color {
private:
  void _init(char red, char green, char blue) {
    this->red = red;
    this->green = green;
    this->blue = blue;
  }
  Color(char red, char green, char blue) {
    _init(red, green, blue);
  }
public:
  char red, green, blue;
  static Color RGB(char red, char green, char blue) {
    return Color(red, green, blue);
  }
  static Color HSB(char hue, char sat, char bright) {
    float sat_f = (float)sat / 0xFF;
    float bright_f = (float)sat / 0xFF;
    int c = bright_f * sat_f;
    float x = c * (1 - fabs(fmod_wrap(hue / (0xFF/6.), 2) - 1));
    int m = bright_f - c;
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

#endif
