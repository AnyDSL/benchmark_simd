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
#include <string.h>
#include <math.h>
#include "../timing.h"
#include "stencil_ispc.h"

//#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
//#include <x86intrin.h>
//#endif

using namespace ispc;


extern void loop_stencil_serial(int t0, int t1, int x0, int x1,
                                int y0, int y1, int z0, int z1,
                                int Nx, int Ny, int Nz,
                                const float coef[4],
                                const float vsq[],
                                float Aeven[], float Aodd[]);

extern "C" void loop_stencil_impala(int t0, int t1, int x0, int x1,
                                    int y0, int y1, int z0, int z1,
                                    int Nx, int Ny, int Nz,
                                    const float coef[4],
                                    const float vsq[],
                                    float Aeven[], float Aodd[]);

void InitData(int Nx, int Ny, int Nz, float *A[2], float *vsq) {
    int offset = 0;
    for (int z = 0; z < Nz; ++z)
        for (int y = 0; y < Ny; ++y)
            for (int x = 0; x < Nx; ++x, ++offset) {
                A[0][offset] = (x < Nx / 2) ? x / float(Nx) : y / float(Ny);
                A[1][offset] = 0;
                vsq[offset] = x*y*z / float(Nx * Ny * Nz);
            }
}

static double
median(double* times, size_t n) {
    if (n == 0) return 0.0f;
    std::sort(times, times + n);
    return times[n/2];
}

int main(int argc, char *argv[]) {
    //#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
        //_mm_setcsr(_mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
    //#endif

    static unsigned int test_iterations[] = {3, 3, 3};//the last two numbers must be equal here
    int Nx = 256, Ny = 256, Nz = 256;
    int width = 4;

    if (argc > 1) {
        if (strncmp(argv[1], "--scale=", 8) == 0) {
            float scale = atof(argv[1] + 8);
            Nx *= scale;
            Ny *= scale;
            Nz *= scale;
        }
    }
    if ((argc == 4) || (argc == 5)) {
        for (int i = 0; i < 3; i++) {
            test_iterations[i] = atoi(argv[argc - 3 + i]);
        }
    }

    unsigned int maxTestIters = std::max(test_iterations[0], std::max(test_iterations[1], test_iterations[2]));
    double times[maxTestIters];

    float *Aserial[2], *Aispc[2], *Aimpala[2];
    Aserial[0] = new float [Nx * Ny * Nz];
    Aserial[1] = new float [Nx * Ny * Nz];
    Aispc[0] = new float [Nx * Ny * Nz];
    Aispc[1] = new float [Nx * Ny * Nz];
    Aimpala[0] = new float [Nx * Ny * Nz];
    Aimpala[1] = new float [Nx * Ny * Nz];
    float *vsq = new float [Nx * Ny * Nz];

    float coeff[4] = { 0.5, -.25, .125, -.0625 };

#define BENCH(iter, fn, res, name, bufs) \
    double res; \
    InitData(Nx, Ny, Nz, bufs, vsq); \
    for (unsigned int i = 0; i < iter; ++i) { \
        reset_and_start_timer(); \
        fn(0, 6, width, Nx - width, width, Ny - width, width, Nz - width, Nx, Ny, Nz, coeff, vsq, bufs[0], bufs[1]); \
        double dt = get_elapsed_mcycles(); \
        printf("@time of " name " run:\t\t\t[%.3f] million cycles\n", dt); \
        times[i] = dt; \
    } \
    res = median(times, iter); \
    printf("[stencil " name "]:\t\t[%.3f] million cycles\n", res);

    BENCH(test_iterations[0], loop_stencil_ispc,   timeISPC,   "ispc",   Aispc);
    BENCH(test_iterations[1], loop_stencil_impala, timeImpala, "impala", Aimpala);
    BENCH(test_iterations[2], loop_stencil_serial, timeSerial, "serial", Aserial);
    printf("\t\t\t\t(%.2fx speedup from ISPC, %.2fx speedup from AnyDSL)\n", timeSerial / timeISPC, timeSerial / timeImpala);

    // Check for agreement
#if 0
    int offset = 0;
    for (int z = 0; z < Nz; ++z)
        for (int y = 0; y < Ny; ++y)
            for (int x = 0; x < Nx; ++x, ++offset) {
                float error = fabsf((Aserial[1][offset] - Aispc[1][offset]) /
                                    Aserial[1][offset]);
                if (error > 1e-4)
                    printf("Error @ (%d,%d,%d): ispc = %f, serial = %f\n",
                           x, y, z, Aispc[1][offset], Aserial[1][offset]);
            }

    // Check for agreement
    int offset = 0;
    for (int z = 0; z < Nz; ++z)
        for (int y = 0; y < Ny; ++y)
            for (int x = 0; x < Nx; ++x, ++offset) {
                float error = fabsf((Aserial[1][offset] - Aimpala[1][offset]) /
                                    Aserial[1][offset]);
                if (error > 1e-4)
                    printf("Error @ (%d,%d,%d): impala = %f, serial = %f\n",
                           x, y, z, Aimpala[1][offset], Aserial[1][offset]);
            }

#endif
    return 0;
}
