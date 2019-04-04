/*
  Copyright (c) 2010-2011, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.


   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#pragma warning (disable: 4244)
#pragma warning (disable: 4305)
#endif

#include <stdio.h>
#include <algorithm>
#include "../timing.h"
#include "mandelbrot_ispc.h"
#include <string.h>
#include <cstdlib>
using namespace ispc;

extern void mandelbrot_serial(float x0, float y0, float x1, float y1,
                              int width, int height, int maxIterations,
                              int output[]);

extern "C" void mandelbrot_impala(float x0, float y0, float x1, float y1,
                              int width, int height, int maxIterations,
                              int output[]);

/* Write a PPM image file with the image of the Mandelbrot set */
static void
writePPM(int *buf, int width, int height, const char *fn) {
    FILE *fp = fopen(fn, "wb");
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", width, height);
    fprintf(fp, "255\n");
    for (int i = 0; i < width*height; ++i) {
        // Map the iteration count to colors by just alternating between
        // two greys.
        char c = (buf[i] & 0x1) ? (char)240 : 20;
        for (int j = 0; j < 3; ++j)
            fputc(c, fp);
    }
    fclose(fp);
    printf("Wrote image file %s\n", fn);
}

static double
median(double* times, size_t n) {
    if (n == 0) return 0.0f;
    std::sort(times, times + n);
    return times[n/2];
}

int main(int argc, char *argv[]) {
    static unsigned int test_iterations[] = {3, 3, 3};
    unsigned int width = 768;
    unsigned int height = 512;
    float x0 = -2;
    float x1 = 1;
    float y0 = -1;
    float y1 = 1;

    for (int i = 1, iter = 0; i < argc; ++i) {
        if (strncmp(argv[i], "--scale=", 8) == 0) {
            float scale = atof(argv[i] + 8);
            width *= scale;
            height *= scale;
        } else {
            test_iterations[iter++] = atoi(argv[i]);
        }
    }

    int maxIterations = 256;
    int *buf = new int[width*height];

    unsigned int maxTestIters = std::max(test_iterations[0], std::max(test_iterations[1], test_iterations[2]));
    double times[maxTestIters];

#define BENCH(iter, fn, res, name) \
    double res = 0.0; \
    { \
        for (unsigned int i = 0; i < iter; ++i) { \
            reset_and_start_timer(); \
            fn(x0, y0, x1, y1, width, height, maxIterations, buf); \
            double dt = get_elapsed_mcycles(); \
            printf("@time of " name " run:\t\t\t[%.3f] million cycles\n", dt); \
            times[i] = dt; \
        } \
        res = median(times, iter); \
        printf("[mandelbrot " name "]:\t\t[%.3f] million cycles\n", res); \
        writePPM(buf, width, height, "mandelbrot-" name ".ppm"); \
        for (unsigned int i = 0; i < width * height; ++i) \
            buf[i] = 0; \
    }

    BENCH(test_iterations[0], mandelbrot_ispc,   timeISPC,   "ispc")
    BENCH(test_iterations[1], mandelbrot_impala, timeImpala, "impala")
    BENCH(test_iterations[2], mandelbrot_serial, timeSerial, "serial")
    printf("\t\t\t\t(%.2fx speedup from ISPC, %.2fx speedup from AnyDSL)\n", timeSerial / timeISPC, timeSerial / timeImpala);

    return 0;
}
