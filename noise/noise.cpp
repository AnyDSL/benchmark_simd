/*
  Copyright (c) 2010-2014, Intel Corporation
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

#include <cstdlib>
#include <stdio.h>
#include <algorithm>
#include "../timing.h"
#include "noise_ispc.h"
#include <string.h>
using namespace ispc;

extern void noise_serial(float x0, float y0, float x1, float y1, int width, int height, float output[]);
extern "C" void noise_impala(float x0, float y0, float x1, float y1, int width, int height, float output[]);

/* Write a PPM image file with the image */
static void
writePPM(float *buf, int width, int height, const char *fn) {
    FILE *fp = fopen(fn, "wb");
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", width, height);
    fprintf(fp, "255\n");
    for (int i = 0; i < width*height; ++i) {
        float v = buf[i] * 255.f;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        for (int j = 0; j < 3; ++j)
            fputc((char)v, fp);
    }
    fclose(fp);
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
    unsigned int height = 768;
    float x0 = -10;
    float x1 = 10;
    float y0 = -10;
    float y1 = 10;

    if (argc > 1) {
        if (strncmp(argv[1], "--scale=", 8) == 0) {
            float scale = atof(argv[1] + 8);
            width *= scale;
            height *= scale;
        }
    }
    if ((argc >= 3) && (argc <= 5)) {
        for (int i = 0; i < 3; i++) {
            test_iterations[i] = atoi(argv[argc - 3 + i]);
        }
    }
    float *buf = new float[width*height];

    unsigned int maxTestIters = std::max(test_iterations[0], std::max(test_iterations[1], test_iterations[2]));
    double times[maxTestIters];

#define BENCH(iter, fn, res, name) \
    double res; \
    for (unsigned int i = 0; i < iter; ++i) { \
        reset_and_start_timer(); \
        fn(x0, y0, x1, y1, width, height, buf); \
        double dt = get_elapsed_mcycles(); \
        printf("@time of " name " run:\t\t\t[%.3f] million cycles\n", dt); \
        times[i] = dt; \
    } \
    res = median(times, iter); \
    printf("[noise " name "]:\t\t\t[%.3f] million cycles\n", res); \
    writePPM(buf, width, height, "noise-" name ".ppm"); \
    for (unsigned int i = 0; i < width * height; ++i) \
        buf[i] = 0;

    BENCH(test_iterations[0], noise_ispc,   timeISPC,   "ispc")
    BENCH(test_iterations[1], noise_impala, timeImpala, "impala")
    BENCH(test_iterations[2], noise_serial, timeSerial, "serial")
    printf("\t\t\t\t(%.2fx speedup from ISPC, %.2fx speedup from AnyDSL)\n", timeSerial / timeISPC, timeSerial / timeImpala);
    return 0;
}
