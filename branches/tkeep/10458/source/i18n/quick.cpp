#include <stdio.h>
#include "unicode/utypes.h"

extern void format34(double d, UChar *buffer, const UChar digits[]);

struct FSeq {
  uint64_t mag;
  uint64_t base;
  uint64_t magnum;
  uint64_t magden;
  int32_t scale;
  void set(uint64_t x, uint64_t y, uint64_t n, uint64_t d);
  int32_t getScale() const;
  UBool isDone() const { return mag == 0; }
  int32_t next();
};

void FSeq::set(uint64_t x, uint64_t y, uint64_t n, uint64_t d) {
  mag = x;
  base = y;
  magnum = n;
  magden = d;
  int64_t m = mag / base;
  scale = 0;
  while (m >= 10000) {
    scale += 4;
    m /= 10000;
    base *= 10000;
  }
  while (m >= 10) {
    scale += 1;
    m /= 10;
    base *= 10;
  }
}

int32_t FSeq::getScale() const { return scale; }

int32_t FSeq::next() {
  int32_t result = mag / base;
  mag %= base;
  mag *= 10;
  magnum *= 10;
  mag += magnum / magden;
  magnum %= magden;
  return result;
}

void format23(double d, UBool *neg, FSeq *frac) {
  uint64_t x = *(uint64_t *) &d;
  *neg = (x >> 63);
  int32_t exp = (int32_t) ((x >> 52) & 0x7ff) - 1023;
  uint64_t mag = (1L << 52) | (x & ((1L << 52) - 1));
  uint64_t base = 1L << (52 - exp);
  // round to nearest 1000th
  mag += base / 2000;
  frac->set(mag, base, base % 2000, 2000);
}

void format34(double d, UChar *buffer, const UChar digits[]) {
  FSeq frac;
  UBool neg;
  format23(d, &neg, &frac);
  int32_t idx = 0;
  if (neg) {
    buffer[idx++] = 0x2D;
  }
  int32_t decimalpos = idx + frac.getScale() + 1;
  buffer[decimalpos] = 0x2E;
  while (idx < decimalpos + 4) {
    if (idx == decimalpos) {
      ++idx;
      continue;
    }
    int32_t nd = frac.next();
    if (frac.isDone() && nd % 2 == 1 && idx + 1 == decimalpos + 4) {
      nd--;
    }
    buffer[idx++] = digits[nd];
  }
  buffer[idx++] = '\0';
}
