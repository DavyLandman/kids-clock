#include <Arduino.h>

// Anti-aliased functions

// Support
#define ipart(X) ((int16_t)(X))
#define iround(X) ((uint16_t)(((float)(X))+0.5))
#define fpart(X) (((float)(X))-(float)ipart(X))
#define rfpart(X) (1.0-fpart(X))

// Calculate distance of px,py to closest part of line ax,ay .... bx,by
// ESP32 has FPU so floats are quite quick
float lineDistance(float px, float py, float ax, float ay, float bx, float by) {
  float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
  float h = fmaxf(fminf((pax * bax + pay * bay) / (bax * bax + bay * bay), 1.0f), 0.0f);
  float dx = pax - bax * h, dy = pay - bay * h;
  return sqrtf(dx * dx + dy * dy);
}

// Alphablend specified color with image background pixel and plot at x,y
void plotPixel(int16_t x, int16_t y, float alpha, uint16_t color) {
  // Adjust alpha, 1.3 gain is good for my display
  alpha = alpha * ALPHA_GAIN * 255; // 1.3 alpha gain is good for my display gamma curve
  // Clip alpha
  if (alpha > 255) alpha = 255;
  //Blend color with background and plot
  face.drawPixel(x, y, face.alphaBlend((uint8_t)alpha, color, face.readPixel(x, y)));
}

void drawRoundRectangleAntialiased(int16_t x, int16_t y, int16_t width, int16_t height, int16_t rx, int16_t ry, uint16_t color);

// AA circle outline
void drawCircleAA( int16_t x, int16_t y, int16_t radius, uint16_t color )  {
  drawRoundRectangleAntialiased(x - radius, y - radius, radius << 1, radius << 1, radius, radius, color);
}

// AA filled circle
void fillCircleAA( int16_t x, int16_t y, int16_t radius, uint16_t color )  {
  drawRoundRectangleAntialiased(x - radius, y - radius, radius << 1, radius << 1, radius, radius, color);
  face.fillCircle(x, y, radius, color);
}

// AA circle and rounded rectangle support
void drawRoundRectangleAntialiased(int16_t x, int16_t y, int16_t width, int16_t height, int16_t rx, int16_t ry, uint16_t color) {

  int16_t i;
  int32_t a2, b2, ds, dt, dxt, t, s, d;
  int16_t xp, yp, xs, ys, dyt, od, xx, yy, xc2, yc2;
  float cp;
  float sab;
  float weight, iweight;

  if ((rx < 0) || (ry < 0)) {
    return;
  }

  // Temporary - should really draw a box if corner radius is zero...
  if (rx == 0) {
    face.drawFastVLine(x, y - ry, y + ry, color);
    return;
  }

  if (ry == 0) {
    face.drawFastHLine(x - rx, y, x + rx, color);
    return;
  }

  a2 = rx * rx;
  b2 = ry * ry;

  ds = a2 << 1;
  dt = b2 << 1;

  xc2 = x << 1;
  yc2 = y << 1;

  sab = sqrt((float)(a2 + b2));
  od = (int)iround(sab * 0.01) + 1;
  dxt = (int)iround((float)a2 / sab) + od;

  t = 0;
  s = -2 * a2 * ry;
  d = 0;

  xp = x + rx;
  yp = y;

  // Sides - may need to reduce color brightness to visually match anti-aliased corners
  face.drawFastHLine(x + rx, y + height, 2 * rx - width + 1, color);
  face.drawFastHLine(x + rx, y, 2 * rx - width + 1, fg);
  face.drawFastVLine(x + width, y + ry, 2 * ry - height + 1, color);
  face.drawFastVLine(x, y + ry, 2 * ry - height + 1, color);

  for (i = 1; i <= dxt; i++) {
    xp--;
    d += t - b2;

    if (d >= 0) {
      ys = yp - 1;
    } else if ((d - s - a2) > 0) {
      if (((d << 1) - s - a2) >= 0) {
        ys = yp + 1;
      } else {
        ys = yp;
        yp++;
        d -= s + a2;
        s += ds;
      }
    } else {
      yp++;
      ys = yp + 1;
      d -= s + a2;
      s += ds;
    }

    t -= dt;

    if (s != 0) {
      cp = (float) abs(d) / (float) abs(s);
      if (cp > 1.0) {
        cp = 1.0f;
      }
    } else {
      cp = 1.0f;
    }

    weight = cp;
    iweight = 1 - weight;


    /* Upper half */
    xx = xc2 - xp;
    plotPixel(xp, yp, iweight, color);
    plotPixel(xx + width, yp, iweight, color);

    plotPixel(xp, ys, weight, color);
    plotPixel(xx + width, ys, weight, color);

    /* Lower half */
    yy = yc2 - yp;
    plotPixel(xp, yy + height, iweight, color);
    plotPixel(xx + width, yy + height, iweight, color);

    yy = yc2 - ys;
    plotPixel(xp, yy + height, weight, color);
    plotPixel(xx + width, yy + height, weight, color);
  }

  /* Replaces original approximation code dyt = abs(yp - yc); */
  dyt = (int)iround((float)b2 / sab ) + od;

  for (i = 1; i <= dyt; i++) {
    yp++;
    d -= s + a2;

    if (d <= 0) {
      xs = xp + 1;
    } else if ((d + t - b2) < 0) {
      if (((d << 1) + t - b2) <= 0) {
        xs = xp - 1;
      } else {
        xs = xp;
        xp--;
        d += t - b2;
        t -= dt;
      }
    } else {
      xp--;
      xs = xp - 1;
      d += t - b2;
      t -= dt;
    }

    s += ds;

    if (t != 0) {
      cp = (float) abs(d) / (float) abs(t);
      if (cp > 1.0) {
        cp = 1.0f;
      }
    } else {
      cp = 1.0f;
    }

    weight = cp;
    iweight = 1 - weight;

    /* Left half */
    xx = xc2 - xp;
    yy = yc2 - yp;
    plotPixel(xp, yp, iweight, color);
    plotPixel(xx + width, yp, iweight, color);

    plotPixel(xp, yy + height, iweight, color);
    plotPixel(xx + width, yy + height, iweight, color);

    /* Right half */
    xx = xc2 - xs;
    plotPixel(xs, yp, weight, color);
    plotPixel(xx + width, yp, weight, color);

    plotPixel(xs, yy + height, weight, color);
    plotPixel(xx + width, yy + height, weight, color);
  }
}

// Anti aliased line ax,ay to bx,by, (2 * r) wide with rounded ends
// Note: not optimised to minimise sampling zone
// Using floats for line coordinates allows for sub-pixel positioning
void drawWideLineAA(float ax, float ay, float bx, float by, float r, uint16_t color) {
  int16_t x0 = (int16_t)floorf(fminf(ax, bx) - r);
  int16_t x1 = (int16_t) ceilf(fmaxf(ax, bx) + r);
  int16_t y0 = (int16_t)floorf(fminf(ay, by) - r);
  int16_t y1 = (int16_t) ceilf(fmaxf(ay, by) + r);

  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      plotPixel(x, y, fmaxf(fminf(0.5f - (lineDistance(x, y, ax, ay, bx, by) - r), 1.0f), 0.0f), color);
    }
  }
}
