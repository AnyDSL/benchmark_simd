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

#define NOMINMAX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <algorithm>
using std::max;

#include "options_defs.h"
#include "../timing.h"

#include "options_ispc.h"
using namespace ispc;

extern     void black_scholes_serial(float Sa[], float Xa[], float Ta[], float ra[], float va[], float result[], int count);
extern "C" void black_scholes_impala(float Sa[], float Xa[], float Ta[], float ra[], float va[], float result[], int count);
extern     void binomial_put_serial (float Sa[], float Xa[], float Ta[], float ra[], float va[], float result[], int count);
extern "C" void binomial_put_impala (float Sa[], float Xa[], float Ta[], float ra[], float va[], float result[], int count);

static void usage() {
    printf("usage: options [--count=<num options>]\n");
}

static double
median(double* times, size_t n) {
    if (n == 0) return 0.0f;
    std::sort(times, times + n);
    return times[n/2];
}

int main(int argc, char *argv[]) {
    int nOptions = 128*1024;

    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--count=", 8) == 0) {
            nOptions = atoi(argv[i] + 8);
            if (nOptions <= 0) {
                usage();
                exit(1);
            }
        }
    }

    int maxIters = 7;

    float *S = new float[nOptions];
    float *X = new float[nOptions];
    float *T = new float[nOptions];
    float *r = new float[nOptions];
    float *v = new float[nOptions];
    float *result = new float[nOptions];
    double* times = new double[maxIters];

    for (int i = 0; i < nOptions; ++i) {
        S[i] = 100;  // stock price
        X[i] = 98;   // option strike price
        T[i] = 2;    // time (years)
        r[i] = .02;  // risk-free interest rate
        v[i] = 5;    // volatility
    }

    double sum;
    double timeISPC, timeImpala, timeSerial;

#define BENCH(iters, fn, res, name) \
    { \
        for (int i = 0; i < iters; ++i) { \
            reset_and_start_timer(); \
            fn(S, X, T, r, v, result, nOptions); \
            double dt = get_elapsed_mcycles(); \
            sum = 0.; \
            for (int i = 0; i < nOptions; ++i) \
                sum += result[i]; \
            times[i] = dt; \
        } \
        res = median(times, iters); \
        printf("[" name "]:\t[%.3f] million cycles (avg %f)\n", res, sum / nOptions); \
    }

    BENCH(7, binomial_put_ispc,   timeISPC,   "binomial ispc")
    BENCH(7, binomial_put_impala, timeImpala, "binomial impala")
    BENCH(7, binomial_put_serial, timeSerial, "binomial serial")
    printf("\t\t\t\t(%.2fx speedup from ISPC, %.2fx speedup from AnyDSL)\n", timeSerial / timeISPC, timeSerial / timeImpala);

    BENCH(7, black_scholes_ispc,   timeISPC,   "black-scholes ispc")
    BENCH(7, black_scholes_impala, timeImpala, "black-scholes impala")
    BENCH(7, black_scholes_serial, timeSerial, "black-scholes serial")
    printf("\t\t\t\t(%.2fx speedup from ISPC, %.2fx speedup from AnyDSL)\n", timeSerial / timeISPC, timeSerial / timeImpala);

    return 0;
}
