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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef __linux__
#include <malloc.h>
#endif
#include <math.h>
#include <map>
#include <string>
#include <algorithm>
#include <sys/types.h>

#include "ao_ispc.h"
using namespace ispc;

#include "../timing.h"

#define NSUBSAMPLES        2

extern void ao_serial(int w, int h, int nsubsamples, float image[]);
extern "C" void ao_impala(int w, int h, int nsubsamples, float image[]);

static unsigned int test_iterations[] = {3, 7, 1};
static unsigned int width, height;
static unsigned char *img;
static float *fimg;
static double* times;

static unsigned char
clamp(float f)
{
    int i = (int)(f * 255.5);

    if (i < 0) i = 0;
    if (i > 255) i = 255;

    return (unsigned char)i;
}


static void
savePPM(const char *fname, int w, int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++)  {
            img[3 * (y * w + x) + 0] = clamp(fimg[3 *(y * w + x) + 0]);
            img[3 * (y * w + x) + 1] = clamp(fimg[3 *(y * w + x) + 1]);
            img[3 * (y * w + x) + 2] = clamp(fimg[3 *(y * w + x) + 2]);
        }
    }

    FILE *fp = fopen(fname, "wb");
    if (!fp) {
        perror(fname);
        exit(1);
    }

    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", w, h);
    fprintf(fp, "255\n");
    fwrite(img, w * h * 3, 1, fp);
    fclose(fp);
    printf("Wrote image file %s\n", fname);
}

static double
median(double* times, size_t n) {
    if (n == 0) return 0.0f;
    std::sort(times, times + n);
    return times[n/2];
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf ("%s\n", argv[0]);
        printf ("Usage: ao [width] [height] [ispc iterations] [AnyDSL iterations] [serial iterations]\n");
        getchar();
        exit(-1);
    }
    else {
        if (argc == 6) {
            for (int i = 0; i < 3; i++) {
                test_iterations[i] = atoi(argv[3 + i]);
            }
        }
        width = atoi (argv[1]);
        height = atoi (argv[2]);
    }
    unsigned int maxIter = std::max(test_iterations[0], std::max(test_iterations[1], test_iterations[2]));

    // Allocate space for output images
    img = new unsigned char[width * height * 3];
    fimg = new float[width * height * 3];
    times = new double[maxIter];

#define BENCH(iter, fn, res, name) \
    for (unsigned int i = 0; i < iter; i++) { \
        memset((void *)fimg, 0, sizeof(float) * width * height * 3); \
        assert(NSUBSAMPLES == 2); \
        reset_and_start_timer(); \
        fn(width, height, NSUBSAMPLES, fimg); \
        double t = get_elapsed_mcycles(); \
        printf("@time of " name " run:\t\t\t[%.3f] million cycles\n", t); \
        times[i] = t; \
    } \
    double res = median(times, iter); \
    printf("[aobench " name "]:\t\t\t[%.3f] million cycles (%d x %d image)\n", res, width, height); \
    savePPM("ao-" name ".ppm", width, height); \

    BENCH(test_iterations[0], ao_ispc,   timeISPC, "ispc")
    BENCH(test_iterations[1], ao_impala, timeImpala, "impala")
    BENCH(test_iterations[2], ao_serial, timeSerial, "serial")
    printf("\t\t\t\t(%.2fx speedup from ISPC, %.2fx speedup from AnyDSL)\n", timeSerial / timeISPC, timeSerial / timeImpala);

    return 0;
}
