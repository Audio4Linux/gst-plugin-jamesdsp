// (c) Copyright 2016, Sean Connelly (@voidqk), http://syntheti.cc
// Modified by James34602 (James34602) for more efficient processing
// Changes:
// 1. Remove sample in/out structure
// 2. Change file extension to cpp for easy integrate with c++ program
// MIT License
// Project Home: https://github.com/voidqk/sndfilter
// Using in: JamesDSPManager

#include "reverb.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

// utility functions
static inline double db2lin(double db){ // dB to linear
	return pow(10.0, 0.05 * db);
}

static inline int clampi(int v, int min, int max){
	return v < min ? min : (v > max ? max : v);
}

static inline double clampf(double v, double min, double max){
	return v < min ? min : (v > max ? max : v);
}

static int isprime(int v)
{
    if (v < 0)
        return isprime(-v);
    if (v < 2)
        return 0;
    if (v <= 3)
        return 1;
    if ((v % 2) == 0 || (v % 3) == 0)
        return 0;
    int max = (int)sqrt((double)v);
    for (int i = 5; i <= max; i += 6)
    {
        if ((v % i) == 0 || (v % (i + 2)) == 0)
            return 0;
    }
    return 1;
}

static inline int nextprime(int v)
{
    while (!isprime(v))
        v++;
    return v;
}

// generate a random double [0, 1) using a simple (but good quality) RNG
static inline double randdouble()
{
    static uint32_t seed = 123; // doesn't matter
    static uint32_t i = 456; // doesn't matter
    uint32_t m = 0x5bd1e995;
    uint32_t k = i++ * m;
    seed = (k ^ (k >> 24) ^ (seed * m)) * m;
    uint32_t R = (seed ^ (seed >> 13)) & 0x007FFFFF; // get 23 random bits
    union
    {
        uint32_t i;
        double f;
    } u = { u.i = 0x3F800000 | R };
    return u.f - 1.0;
}

//
//
// component implementation
//
//

// components are in the basic format of `<component>_make` to initialize a structure and
// `<component>_step` to perform a single step with the component

//
// delay
//
static inline void delay_make(sf_rv_delay_st *delay, int size)
{
    delay->pos = 0;
    delay->size = clampi(size, 1, SF_REVERB_DS);
    memset(delay->buf, 0, sizeof(double) * delay->size);
}

static inline double delay_step(sf_rv_delay_st *delay, double v)
{
    double out = delay->buf[delay->pos];
    delay->buf[delay->pos] = v;
    delay->pos = (delay->pos + 1) % delay->size;
    return out;
}

// delay_get(d, 1) returns the last written value
// delay_get(d, 2) returns the second-last written value
// ..etc
static inline double delay_get(sf_rv_delay_st *delay, int offset)
{
    if (offset > delay->size)
        return delay->buf[delay->pos];
    else if (offset <= 0)
        offset = 1;
    int pos = delay->pos - offset;
    if (pos < 0)
        pos += delay->size;
    return delay->buf[pos];
}

static inline double delay_getlast(sf_rv_delay_st *delay)
{
    return delay->buf[delay->pos];
}

//
// iir1
//
static inline void iir1_makeLPF(sf_rv_iir1_st *iir1, int rate, double freq)
{
    // 1st order IIR lowpass filter (Butterworth)
    freq = clampf(freq, 0, rate / 2);
    double omega2 = 3.14159265358979323846264338327950288 * freq / (double)rate;
    double tano2 = tan(omega2);
    iir1->b1 = iir1->b2 = tano2 / (1.0 + tano2);
    iir1->a2 = (1.0 - tano2) / (1.0 + tano2);
    iir1->y1 = 0;
}

static inline void iir1_makeHPF(sf_rv_iir1_st *iir1, int rate, double freq)
{
    // 1st order IIR highpass filter (Butterworth)
    freq = clampf(freq, 0, rate / 2);
    double omega2 = 3.14159265358979323846264338327950288 * freq / (double)rate;
    double tano2 = tanf(omega2);
    iir1->b1 = 1.0 / (1.0 + tano2);
    iir1->b2 = -iir1->b1;
    iir1->a2 = (1.0 - tano2) / (1.0 + tano2);
    iir1->y1 = 0;
}

static inline double iir1_step(sf_rv_iir1_st *iir1, double v)
{
    double out = v * iir1->b1 + iir1->y1;
    iir1->y1 = out * iir1->a2 + v * iir1->b2;
    return out;
}

//
// biquad
//
static inline void biquad_makeLPF(sf_rv_biquad_st *biquad, int rate, double freq, double bw)
{
    freq = clampf(freq, 0, rate / 2);
    double omega = 2.0 * 3.14159265358979323846264338327950288 * freq / (double)rate;
    double cs = cos(omega);
    double sn = sin(omega);
    double alpha = sn * sinh(0.693147180559945309417232121458176568 * 0.5 * bw * omega / sn);
    double a0inv = 1.0 / (1.0 + alpha);
    biquad->b0 = a0inv * (1.0 - cs) * 0.5;
    biquad->b1 = 2.0 * biquad->b0;
    biquad->b2 = biquad->b0;
    biquad->a1 = a0inv * -2.0 * cs;
    biquad->a2 = a0inv * (1.0 - alpha);
    biquad->xn1 = 0;
    biquad->xn2 = 0;
    biquad->yn1 = 0;
    biquad->yn2 = 0;
}

static inline void biquad_makeLPFQ(sf_rv_biquad_st *biquad, int rate, double freq, double bw)
{
    freq = clampf(freq, 0, rate / 2);
    double omega = 2.0 * 3.14159265358979323846264338327950288 * freq / (double)rate;
    double cs = cos(omega);
    double alpha = sin(omega) * 2.0 * bw; // different alpha calculation than makeLPF above
    double a0inv = 1.0 / (1.0 + alpha);
    biquad->b0 = a0inv * (1.0 - cs) * 0.5;
    biquad->b1 = 2.0 * biquad->b0;
    biquad->b2 = biquad->b0;
    biquad->a1 = a0inv * -2.0 * cs;
    biquad->a2 = a0inv * (1.0 - alpha);
    biquad->xn1 = 0;
    biquad->xn2 = 0;
    biquad->yn1 = 0;
    biquad->yn2 = 0;
}

static inline void biquad_makeAPF(sf_rv_biquad_st *biquad, int rate, double freq, double bw)
{
    freq = clampf(freq, 0, rate / 2);
    double omega = 2.0 * 3.14159265358979323846264338327950288 * freq / (double)rate;
    double sn = sin(omega);
    double alpha = sn * sinh(0.693147180559945309417232121458176568 * 0.5 * bw * omega / sn);
    double a0inv = 1.0 / (1.0 + alpha);
    biquad->b0 = a0inv * (1.0 - alpha);
    biquad->b1 = a0inv * -2.0 * cos(omega);
    biquad->b2 = a0inv * (1.0 + alpha);
    biquad->a1 = biquad->b1;
    biquad->a2 = biquad->b0;
    biquad->xn1 = 0;
    biquad->xn2 = 0;
    biquad->yn1 = 0;
    biquad->yn2 = 0;
}

static inline double biquad_step(sf_rv_biquad_st *biquad, double v)
{
    double out = v * biquad->b0 + biquad->xn1 * biquad->b1 + biquad->xn2 * biquad->b2 -
                biquad->yn1 * biquad->a1 - biquad->yn2 * biquad->a2;
    biquad->xn2 = biquad->xn1;
    biquad->xn1 = v;
    biquad->yn2 = biquad->yn1;
    biquad->yn1 = out;
    return out;
}

//
// earlyref
//
static inline void earlyref_make(sf_rv_earlyref_st *earlyref, int rate, double factor, double width)
{
    static const double delaytblLc[18] = { 0.0043, 0.0215, 0.0225, 0.0268, 0.0270, 0.0298, 0.0458, 0.0485, 0.0572, 0.0587, 0.0595, 0.0612, 0.0707, 0.0708, 0.0726, 0.0741, 0.0753, 0.0797 };
    static const double delaytblRc[18] = { 0.0053, 0.0225, 0.0235, 0.0278, 0.0290, 0.0288, 0.0468, 0.0475, 0.0582, 0.0577, 0.0575, 0.0622, 0.0697, 0.0718, 0.0736, 0.0751, 0.0763, 0.0817 };
    earlyref->wet1 = width * 0.5 + 0.5;
    earlyref->wet2 = (1.0 - width) * 0.5;
    int lrdelay = 0.0002 * (double)rate;
    delay_make(&earlyref->delayRL, lrdelay);
    delay_make(&earlyref->delayLR, lrdelay);
    biquad_makeAPF(&earlyref->allpassXL, rate, 740.0, 4.0);
    earlyref->allpassXR = earlyref->allpassXL;
    biquad_makeAPF(&earlyref->allpassL, rate, 150.0, 4.0);
    earlyref->allpassR = earlyref->allpassL;
    factor *= rate;
    for (int i = 0; i < 18; i++)
    {
        earlyref->delaytblL[i] = delaytblLc[i] * factor;
        earlyref->delaytblR[i] = delaytblRc[i] * factor;
    }
    delay_make(&earlyref->delayPWL, earlyref->delaytblL[17] + 10);
    delay_make(&earlyref->delayPWR, earlyref->delaytblR[17] + 10);
    iir1_makeLPF(&earlyref->lpfL, rate, 20000.0);
    earlyref->lpfR = earlyref->lpfL;
    iir1_makeHPF(&earlyref->hpfL, rate, 4.0);
    earlyref->hpfR = earlyref->hpfL;
}

static inline void earlyref_step(sf_rv_earlyref_st *earlyref, double inputL, double inputR, double *outL, double *outR)
{
    static const double gaintblL[18] = { 0.841, 0.504, 0.491, 0.379, 0.380, 0.346, 0.289, 0.272, 0.192, 0.193, 0.217, 0.181, 0.180, 0.181, 0.176, 0.142, 0.167, 0.134 };
    static const double gaintblR[18] = { 0.842, 0.506, 0.489, 0.382, 0.300, 0.346, 0.290, 0.271, 0.193, 0.192, 0.217, 0.195, 0.192, 0.166, 0.186, 0.131, 0.168, 0.133 };
    double wetL = 0, wetR = 0;
    delay_step(&earlyref->delayPWL, inputL);
    delay_step(&earlyref->delayPWR, inputR);
    for (int i = 0; i < 18; i++)
    {
        wetL += gaintblL[i] * delay_get(&earlyref->delayPWL, earlyref->delaytblL[i]);
        wetR += gaintblR[i] * delay_get(&earlyref->delayPWR, earlyref->delaytblR[i]);
    }
    double L = delay_step(&earlyref->delayRL, inputR + wetR);
    L = biquad_step(&earlyref->allpassXL, L);
    L = biquad_step(&earlyref->allpassL, earlyref->wet1 * wetL + earlyref->wet2 * L);
    L = iir1_step(&earlyref->hpfL, L);
    L = iir1_step(&earlyref->lpfL, L);
    double R = delay_step(&earlyref->delayLR, inputL + wetL);
    R = biquad_step(&earlyref->allpassXR, R);
    R = biquad_step(&earlyref->allpassR, earlyref->wet1 * wetR + earlyref->wet2 * R);
    R = iir1_step(&earlyref->hpfR, R);
    R = iir1_step(&earlyref->lpfR, R);
    *outL = L;
    *outR = R;
}

//
// oversample
//
static inline void oversample_make(sf_rv_oversample_st *oversample, int factor)
{
    oversample->factor = clampi(factor, 1, SF_REVERB_OF);
    biquad_makeLPFQ(&oversample->lpfU, 2 * oversample->factor, 1.0, 0.5773502691896258); // 1/sqrt(3)
    oversample->lpfD = oversample->lpfU;
}

// output length must be oversample->factor
static inline void oversample_stepup(sf_rv_oversample_st *oversample, double input, double *output)
{
    if (oversample->factor == 1)
    {
        output[0] = input;
        return;
    }
    output[0] = biquad_step(&oversample->lpfU, input * oversample->factor);
    for (int i = 1; i < oversample->factor; i++)
        output[i] = biquad_step(&oversample->lpfU, 0);
}

// input length must be oversample->factor
static inline double oversample_stepdown(sf_rv_oversample_st *oversample, double *input)
{
    if (oversample->factor == 1)
        return input[0];
    for (int i = 0; i < oversample->factor; i++)
        biquad_step(&oversample->lpfD, input[i]);
    return input[0];
}

//
// dccut
//
static inline void dccut_make(sf_rv_dccut_st *dccut, int rate, double freq)
{
    freq = clampf(freq, 0, rate / 2);
    double ang = 2.0 * 3.14159265358979323846264338327950288 * freq / (double)rate;
    double sn = sin(ang);
    double sqrt3 = 1.7320508075688772;
    dccut->gain = (sqrt3 - 2.0 * sn) / (sn + sqrt3 * cos(ang));
    dccut->y1 = 0;
    dccut->y2 = 0;
}

static inline double dccut_step(sf_rv_dccut_st *dccut, double v)
{
    double out = v - dccut->y1 + dccut->gain * dccut->y2;
    dccut->y1 = v;
    dccut->y2 = out;
    return out;
}

//
// noise
//
static inline void noise_make(sf_rv_noise_st *noise)
{
    noise->pos = SF_REVERB_NS;
}

static inline double noise_step(sf_rv_noise_st *noise)
{
    if (noise->pos >= SF_REVERB_NS)
    {
        // need to generate more noise
        noise->pos = 0;
        int len = SF_REVERB_NS;
        int tot = 1;
        double r = 0.8;
        double rmul = 0.7071067811865475; // 1/sqrt(2)
        noise->buf[0] = 0;
        while (len > 1)
        {
            double left = 0;
            for (int i = tot - 1; i >= 0; i--)
            {
                double right = left;
                left = noise->buf[i * len];
                double midpoint = (left + right) * 0.5;
                double newv = midpoint + r * (2.0 * randdouble() - 1.0); // displace by random amt
                noise->buf[i * len + (len / 2)] = clampf(newv, -1.0, 1.0);
            }
            len /= 2;
            tot *= 2;
            r *= rmul;
        }
    }
    return noise->buf[noise->pos++];
}

//
// lfo
//
static inline void lfo_make(sf_rv_lfo_st *lfo, int rate, double freq)
{
    lfo->count = 0;
    lfo->re = 1.0;
    lfo->im = 0.0;
    double theta = 2.0 * 3.14159265358979323846264338327950288 * freq / (double)rate;
    lfo->sn = sin(theta);
    lfo->co = cos(theta);
}

static inline double lfo_step(sf_rv_lfo_st *lfo)
{
    double v = lfo->im;
    double re = lfo->re * lfo->co - lfo->im * lfo->sn;
    double im = lfo->re * lfo->sn + lfo->im * lfo->co;
    if (lfo->count++ > 100000)
    {
        // if we've gathered a lot of samples, then it's probably a good idea to make sure our LFO
        // hasn't accumulated a bunch of errors
        lfo->count = 0;
        double leninv = 1.0 / sqrt(re * re + im * im);
        re *= leninv;
        im *= leninv;
    }
    lfo->re = re;
    lfo->im = im;
    return v;
}

//
// allpass
//
static inline void allpass_make(sf_rv_allpass_st *allpass, int size, double feedback, double decay)
{
    allpass->pos = 0;
    allpass->size = clampi(size, 1, SF_REVERB_APS);
    allpass->feedback = feedback;
    allpass->decay = decay;
    memset(allpass->buf, 0, sizeof(double) * allpass->size);
}

static inline double allpass_step(sf_rv_allpass_st *allpass, double v)
{
    v += allpass->feedback * allpass->buf[allpass->pos];
    double out = allpass->decay * allpass->buf[allpass->pos] - allpass->feedback * v;
    allpass->buf[allpass->pos] = v;
    allpass->pos = (allpass->pos + 1) % allpass->size;
    return out;
}

//
// allpass2
//
static inline void allpass2_make(sf_rv_allpass2_st *allpass2, int size1, int size2, double feedback1,
                                 double feedback2, double decay1, double decay2)
{
    allpass2->pos1 = 0;
    allpass2->pos2 = 0;
    allpass2->size1 = clampi(size1, 1, SF_REVERB_AP2S1);
    allpass2->size2 = clampi(size2, 1, SF_REVERB_AP2S2);
    allpass2->feedback1 = feedback1;
    allpass2->feedback2 = feedback2;
    allpass2->decay1 = decay1;
    allpass2->decay2 = decay2;
    memset(allpass2->buf1, 0, sizeof(double) * allpass2->size1);
    memset(allpass2->buf2, 0, sizeof(double) * allpass2->size2);
}

static inline double allpass2_step(sf_rv_allpass2_st *allpass2, double v)
{
    v += allpass2->feedback2 * allpass2->buf2[allpass2->pos2];
    double out = allpass2->decay2 * allpass2->buf2[allpass2->pos2] - v * allpass2->feedback2;
    v += allpass2->feedback1 * allpass2->buf1[allpass2->pos1];
    allpass2->buf2[allpass2->pos2] = allpass2->decay1 * allpass2->buf1[allpass2->pos1] -
                                     v * allpass2->feedback1;
    allpass2->buf1[allpass2->pos1] = v;
    allpass2->pos1 = (allpass2->pos1 + 1) % allpass2->size1;
    allpass2->pos2 = (allpass2->pos2 + 1) % allpass2->size2;
    return out;
}

static inline double allpass2_get1(sf_rv_allpass2_st *allpass2, int offset)
{
    if (offset > allpass2->size1)
        return allpass2->buf1[allpass2->pos1];
    else if (offset <= 0)
        offset = 1;
    int rp = allpass2->pos1 - offset;
    if (rp < 0)
        rp += allpass2->size1;
    return allpass2->buf1[rp];
}

static inline double allpass2_get2(sf_rv_allpass2_st *allpass2, int offset)
{
    if (offset > allpass2->size2)
        return allpass2->buf2[allpass2->pos2];
    else if (offset <= 0)
        offset = 1;
    int rp = allpass2->pos2 - offset;
    if (rp < 0)
        rp += allpass2->size2;
    return allpass2->buf2[rp];
}

//
// allpass3
//
static inline void allpass3_make(sf_rv_allpass3_st *allpass3, int size1, int msize1, int size2,
                                 int size3, double feedback1, double feedback2, double feedback3, double decay1, double decay2,
                                 double decay3)
{
    size1 = clampi(size1, 1, SF_REVERB_AP3S1);
    msize1 = clampi(msize1, 1, SF_REVERB_AP3M1);
    if (msize1 > size1)
        msize1 = size1;
    int newsize = size1 + msize1;
    allpass3->rpos1 = (msize1 * 2) % newsize;
    allpass3->wpos1 = 0;
    allpass3->pos2 = 0;
    allpass3->pos3 = 0;
    allpass3->size1 = newsize;
    allpass3->msize1 = msize1;
    allpass3->size2 = clampi(size2, 1, SF_REVERB_AP3S2);
    allpass3->size3 = clampi(size3, 1, SF_REVERB_AP3S3);
    allpass3->feedback1 = feedback1;
    allpass3->feedback2 = feedback2;
    allpass3->feedback3 = feedback3;
    allpass3->decay1 = decay1;
    allpass3->decay2 = decay2;
    allpass3->decay3 = decay3;
    memset(allpass3->buf1, 0, sizeof(double) * allpass3->size1);
    memset(allpass3->buf2, 0, sizeof(double) * allpass3->size2);
    memset(allpass3->buf3, 0, sizeof(double) * allpass3->size3);
}

static inline double allpass3_step(sf_rv_allpass3_st *allpass3, double v, double mod)
{
    mod = (mod + 1.0) * (double)allpass3->msize1;
    double floormod = floorf(mod);
    double mfrac = mod - floormod;
    int rpos1 = allpass3->rpos1 - (int)floormod;
    if (rpos1 < 0)
        rpos1 += allpass3->size1;
    int rpos2 = rpos1 - 1;
    if (rpos2 < 0)
        rpos2 += allpass3->size1;
    v += allpass3->feedback3 * allpass3->buf3[allpass3->pos3];
    double out = allpass3->decay3 * allpass3->buf3[allpass3->pos3] - allpass3->feedback3 * v;
    v += allpass3->feedback2 * allpass3->buf2[allpass3->pos2];
    allpass3->buf3[allpass3->pos3] = allpass3->decay2 * allpass3->buf2[allpass3->pos2] -
                                     allpass3->feedback2 * v;
    double tmp = allpass3->buf1[rpos2] * mfrac + allpass3->buf1[rpos1] * (1.0 - mfrac);
    v += allpass3->feedback1 * tmp;
    allpass3->buf2[allpass3->pos2] = allpass3->decay1 * tmp - allpass3->feedback1 * v;
    allpass3->buf1[allpass3->wpos1] = v;
    allpass3->wpos1 = (allpass3->wpos1 + 1) % allpass3->size1;
    allpass3->rpos1 = (allpass3->rpos1 + 1) % allpass3->size1;
    allpass3->pos2 = (allpass3->pos2 + 1) % allpass3->size2;
    allpass3->pos3 = (allpass3->pos3 + 1) % allpass3->size3;
    return out;
}

static inline double allpass3_get1(sf_rv_allpass3_st *allpass3, int offset)
{
    if (offset > allpass3->size1)
        return allpass3->buf1[allpass3->rpos1];
    else if (offset <= 0)
        offset = 1;
    int rp = allpass3->rpos1 - offset;
    if (rp < 0)
        rp += allpass3->size1;
    return allpass3->buf1[rp];
}

static inline double allpass3_get2(sf_rv_allpass3_st *allpass3, int offset)
{
    if (offset > allpass3->size2)
        return allpass3->buf2[allpass3->pos2];
    else if (offset <= 0)
        offset = 1;
    int rp = allpass3->pos2 - offset;
    if (rp < 0)
        rp += allpass3->size2;
    return allpass3->buf2[rp];
}

static inline double allpass3_get3(sf_rv_allpass3_st *allpass3, int offset)
{
    if (offset > allpass3->size3)
        return allpass3->buf3[allpass3->pos3];
    else if (offset <= 0)
        offset = 1;
    int rp = allpass3->pos3 - offset;
    if (rp < 0)
        rp += allpass3->size3;
    return allpass3->buf3[rp];
}

//
// allpassm
//
static inline void allpassm_make(sf_rv_allpassm_st *allpassm, int size, int msize, double feedback,
                                 double decay)
{
    size = clampi(size, 1, SF_REVERB_APMS);
    msize = clampi(msize, 1, SF_REVERB_APMM);
    if (msize > size)
        msize = size;
    int newsize = size + msize;
    allpassm->rpos = (msize * 2) % newsize;
    allpassm->wpos = 0;
    allpassm->size = newsize;
    allpassm->msize = msize;
    allpassm->feedback = feedback;
    allpassm->decay = decay;
    allpassm->z1 = 0;
    memset(allpassm->buf, 0, sizeof(double) * allpassm->size);
}

static inline double allpassm_step(sf_rv_allpassm_st *allpassm, double v, double mod, double fbmod)
{
    double mfeedback = allpassm->feedback + fbmod;
    mod = (mod + 1.0) * (double)allpassm->msize;
    double floormod = floorf(mod);
    double mfrac = 1.0 - mod + floormod;
    int rpos1 = allpassm->rpos - (int)floormod;
    if (rpos1 < 0)
        rpos1 += allpassm->size;
    int rpos2 = rpos1 - 1;
    if (rpos2 < 0)
        rpos2 += allpassm->size;
    allpassm->z1 = allpassm->buf[rpos2] + mfrac * (allpassm->buf[rpos1] - allpassm->z1);
    allpassm->rpos = (allpassm->rpos + 1) % allpassm->size;
    allpassm->buf[allpassm->wpos] = v + allpassm->z1 * mfeedback;
    v = allpassm->decay * allpassm->z1 - allpassm->buf[allpassm->wpos] * mfeedback;
    allpassm->wpos = (allpassm->wpos + 1) % allpassm->size;
    return v;
}

//
// comb
//
static inline void comb_make(sf_rv_comb_st *comb, int size)
{
    comb->pos = 0;
    comb->size = clampi(size, 1, SF_REVERB_CS);
    memset(comb->buf, 0, sizeof(double) * comb->size);
}

static inline double comb_step(sf_rv_comb_st *comb, double v, double feedback)
{
    v = comb->buf[comb->pos] * feedback + v;
    comb->buf[comb->pos] = v;
    comb->pos = (comb->pos + 1) % comb->size;
    return v;
}

//
//
// reverb implementation
//
//

// now that all the components are done (thank god), we can start on the actual reverb effect

void sf_presetreverb(sf_reverb_state_st *rv, int rate, sf_reverb_preset preset)
{
    // sorry for the bad formatting, I've tried to cram this in as best as I could
    struct
    {
        int osf;
        double p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16;
    } ps[] =
    {

        //OSF ERtoLt ERWet Dry ERFac ERWdth Wdth Wet Wander BassB Spin InpLP BasLP DmpLP OutLP RT60  Delay
        { 1, 0.4, -9.0,-7, 1.6, 0.7, 1.0, -0, 0.25, 0.15, 0.7,17000, 500, 7000,10000, 3.2,0.02 },
        { 1, 0.3, -9.0, -7, 1.0, 0.7, 1.0, -8, 0.3, 0.25, 0.7,18000, 600, 9000,17000, 2.1,0.01 },
        { 1, 0.3, -9.0, -7, 1.0, 0.7, 1.0, -8, 0.25, 0.2, 0.5,18000, 600, 7000, 9000, 2.3,0.01 },
        { 1, 0.3, -9.0, -7, 1.2, 0.7, 1.0, -8, 0.25, 0.2, 0.7,18000, 500, 8000,16000, 2.8,0.01 },
        { 1, 0.3, -9.0, -7, 1.2, 0.7, 1.0, -8, 0.2, 0.15, 0.5,18000, 500, 6000, 8000, 2.9,0.01 },
        { 1, 0.2, -9.0, -7, 1.4, 0.7, 1.0, -8, 0.15, 0.2, 1.0,18000, 400, 9000,14000, 3.8,0.018 },
        { 1, 0.2, -9.0, -7, 1.5, 0.7, 1.0, -8, 0.2, 0.2, 0.5,18000, 400, 5000, 7000, 4.2,0.018 },
        { 1, 0.7, -8.0, -7, 0.7,-0.4, 0.8, -8, 0.2, 0.3, 1.6,18000,1000,18000,18000, 0.5,0.005 },
        { 1, 0.7, -8.0, -7, 0.8, 0.6, 0.9, -8, 0.3, 0.3, 0.4,18000, 300,10000,18000, 0.5,0.005 },
        { 1, 0.5, -8.0, -7, 1.2,-0.4, 0.8, -8, 0.2, 0.1, 1.6,18000,1000,18000,18000, 0.8,0.008 },
        { 1, 0.5, -8.0, -7, 1.2, 0.6, 0.9, -8, 0.3, 0.1, 0.4,18000, 300,10000,18000, 1.2,0.016 },
        { 1, 0.2, -8.0, -7, 2.2,-0.4, 0.9, -8, 0.2, 0.1, 1.6,18000,1000,16000,18000, 1.8,0.01 },
        { 1, 0.2, -8.0, -7, 2.2, 0.6, 0.9, -8, 0.3, 0.1, 0.4,18000, 500, 9000,18000, 1.9,0.02 },
        { 1, 0.5, -7.0, -6, 1.2,-0.4, 0.8,-70, 0.2, 0.1, 1.6,18000,1000,18000,18000, 0.8,0.008 },
        { 1, 0.5, -7.0, -6, 1.2, 0.6, 0.9,-70, 0.3, 0.1, 0.4,18000, 300,10000,18000, 1.2,0.016 },
        { 2, 0.0,-30.0,-12, 1.0, 1.0, 1.0, -8, 0.2, 0.1, 1.6,18000,1000,16000,18000, 1.8,0.0 },
        { 2, 0.0,-30.0,-12, 1.0, 1.0, 1.0, -8, 0.3, 0.2, 0.4,18000, 500, 9000,18000, 1.9,0.0 },
        { 2, 0.1,-16.0,-14, 1.0, 0.1, 1.0, -5, 0.35, 0.05, 1.0,18000, 100,10000,18000,12.0,0.0 },
        { 2, 0.1,-16.0,-14, 1.0, 0.1, 1.0, -5, 0.4, 0.05, 1.0,18000, 100, 9000,18000,30.0,0.0 }
    };
#define CASE(prs, i)                                                                        \
		case prs: sf_advancereverb(rv, rate, ps[i].osf, ps[i].p1, ps[i].p2, ps[i].p3, ps[i].p4, \
			ps[i].p5, ps[i].p6, ps[i].p7, ps[i].p8, ps[i].p9, ps[i].p10, ps[i].p11, ps[i].p12,  \
			ps[i].p13, ps[i].p14, ps[i].p15, ps[i].p16); return;
    switch (preset)
    {
        CASE(SF_REVERB_PRESET_DEFAULT, 0)
        CASE(SF_REVERB_PRESET_SMALLHALL1, 1)
        CASE(SF_REVERB_PRESET_SMALLHALL2, 2)
        CASE(SF_REVERB_PRESET_MEDIUMHALL1, 3)
        CASE(SF_REVERB_PRESET_MEDIUMHALL2, 4)
        CASE(SF_REVERB_PRESET_LARGEHALL1, 5)
        CASE(SF_REVERB_PRESET_LARGEHALL2, 6)
        CASE(SF_REVERB_PRESET_SMALLROOM1, 7)
        CASE(SF_REVERB_PRESET_SMALLROOM2, 8)
        CASE(SF_REVERB_PRESET_MEDIUMROOM1, 9)
        CASE(SF_REVERB_PRESET_MEDIUMROOM2, 10)
        CASE(SF_REVERB_PRESET_LARGEROOM1, 11)
        CASE(SF_REVERB_PRESET_LARGEROOM2, 12)
        CASE(SF_REVERB_PRESET_MEDIUMER1, 13)
        CASE(SF_REVERB_PRESET_MEDIUMER2, 14)
        CASE(SF_REVERB_PRESET_PLATEHIGH, 15)
        CASE(SF_REVERB_PRESET_PLATELOW, 16)
        CASE(SF_REVERB_PRESET_LONGREVERB1, 17)
        CASE(SF_REVERB_PRESET_LONGREVERB2, 18)
    }
#undef CASE
}

void sf_advancereverb(sf_reverb_state_st *rv, int rate,
                      int oversamplefactor, double ertolate, double erefwet, double dry, double ereffactor,
                      double erefwidth, double width, double wet, double wander, double bassb, double spin, double inputlpf,
                      double basslpf, double damplpf, double outputlpf, double rt60, double delay)
{
    rv->ertolate = ertolate;
    rv->erefwet = db2lin(erefwet);
    rv->dry = db2lin(dry);
    wet = db2lin(wet);
    rv->wet1 = wet * (width * 0.5 + 0.5);
    rv->wet2 = wet * ((1.0 - width) * 0.5);
    rv->wander = wander;
    rv->bassb = bassb;
    earlyref_make(&rv->earlyref, rate, ereffactor, erefwidth);
    oversample_make(&rv->oversampleL, oversamplefactor);
    rv->oversampleR = rv->oversampleL;
    int osrate = rate * rv->oversampleL.factor;
    dccut_make(&rv->dccutL, osrate, 5.0);
    rv->dccutR = rv->dccutL;
    noise_make(&rv->noise);
    lfo_make(&rv->lfo1, osrate, spin);
    iir1_makeLPF(&rv->lfo1_lpf, osrate, 20.0);
    lfo_make(&rv->lfo2, osrate, sqrt(100.0 - (10.0 - spin) * (10.0 - spin)) * 0.5);
    iir1_makeLPF(&rv->lfo2_lpf, osrate, 12.0);
    static const int diffLc[10] = { 617, 535, 434, 347, 218, 162, 144, 122, 109, 74 };
    static const int diffRc[10] = { 603, 547, 416, 364, 236, 162, 140, 131, 111, 79 };
    int totfactor = osrate / 34125;
    int msize = nextprime(10 * osrate / 34125);
    for (int i = 0; i < 10; i++)
    {
        allpassm_make(&rv->diffL[i], nextprime(diffLc[i] * totfactor), msize, -0.78, 1);
        allpassm_make(&rv->diffR[i], nextprime(diffRc[i] * totfactor), msize, -0.78, 1);
    }
    static const int crossLc[4] = { 430, 341, 264, 174 };
    static const int crossRc[4] = { 447, 324, 247, 191 };
    for (int i = 0; i < 4; i++)
    {
        allpass_make(&rv->crossL[i], nextprime(crossLc[i] * totfactor), 0.78, 1);
        allpass_make(&rv->crossR[i], nextprime(crossRc[i] * totfactor), 0.78, 1);
    }
    iir1_makeLPF(&rv->clpfL, osrate, inputlpf);
    rv->clpfR = rv->clpfL;
    delay_make(&rv->cdelayL, nextprime(1572 * totfactor));
    delay_make(&rv->cdelayR, nextprime(16 * totfactor));
    delay_make(&rv->dampdL, nextprime(2 * totfactor));
    delay_make(&rv->dampdR, nextprime(totfactor));
    delay_make(&rv->cbassd1L, nextprime(1055 * totfactor));
    delay_make(&rv->cbassd1R, nextprime(1460 * totfactor));
    delay_make(&rv->cbassd2L, nextprime(344 * totfactor));
    delay_make(&rv->cbassd2R, nextprime(500 * totfactor));
    biquad_makeAPF(&rv->bassapL, osrate, 150.0, 4.0);
    rv->bassapR = rv->bassapL;
    biquad_makeLPF(&rv->basslpL, osrate, basslpf, 2.0);
    rv->basslpR = rv->basslpL;
    iir1_makeLPF(&rv->damplpL, osrate, damplpf);
    rv->damplpR = rv->damplpL;
    double decay0 = pow(10.0, log10(0.237) / rt60);
    double decay1 = pow(10.0, log10(0.938) / rt60);
    double decay2 = pow(10.0, log10(0.844) / rt60);
    double decay3 = pow(10.0, log10(0.906) / rt60);
    rv->loopdecay = decay0;
    msize = nextprime(32 * totfactor);
    allpassm_make(&rv->dampap1L, nextprime(239 * totfactor), msize, 0.375, decay2);
    allpassm_make(&rv->dampap1R, nextprime(205 * totfactor), msize, 0.375, decay2);
    allpassm_make(&rv->dampap2L, nextprime(392 * totfactor), msize, 0.312, decay3);
    allpassm_make(&rv->dampap2R, nextprime(329 * totfactor), msize, 0.312, decay3);
    allpass2_make(&rv->cbassap1L, nextprime(1944 * totfactor), nextprime(612 * totfactor),
                  0.250, 0.406, decay1, decay2);
    allpass2_make(&rv->cbassap1R, nextprime(2032 * totfactor), nextprime(368 * totfactor),
                  0.250, 0.406, decay1, decay2);
    allpass3_make(&rv->cbassap2L,
                  nextprime(1212 * totfactor),
                  nextprime(121 * totfactor),
                  nextprime(816 * totfactor),
                  nextprime(1264 * totfactor),
                  0.250, 0.250, 0.406, decay1, decay1, decay2);
    allpass3_make(&rv->cbassap2R,
                  nextprime(1452 * totfactor),
                  nextprime(5 * totfactor),
                  nextprime(688 * totfactor),
                  nextprime(1340 * totfactor),
                  0.250, 0.250, 0.406, decay1, decay1, decay2);
    static const int outco[32] =
    {
        1,  40, 192, 276, 321, 110, 468, 1572, 121, 480, 103, 26, 780, 1200, 310, 780,
        625, 468, 312,  24,  36, 790, 189,   8,  10, 359,  30, 10, 109, 1310, 800,  10
    };
    for (int i = 0; i < 32; i++)
        rv->outco[i] = outco[i] * totfactor;
    comb_make(&rv->combL, nextprime(22 * osrate / 1000));
    rv->combR = rv->combL;
    biquad_makeLPF(&rv->lastlpfL, osrate, outputlpf, 1.0);
    rv->lastlpfR = rv->lastlpfL;
    int delaysamp = osrate * delay;
    if (delaysamp >= 0)
    {
        delay_make(&rv->inpdelayL, 0);
        delay_make(&rv->inpdelayR, 0);
        delay_make(&rv->lastdelayL, delaysamp);
        delay_make(&rv->lastdelayR, delaysamp);
    }
    else
    {
        delay_make(&rv->inpdelayL, -delaysamp);
        delay_make(&rv->inpdelayR, -delaysamp);
        delay_make(&rv->lastdelayL, 0);
        delay_make(&rv->lastdelayR, 0);
    }
}
void sf_reverb_process(sf_reverb_state_st *rv, double inputL, double inputR, double *outputL, double *outputR)
{
    // extra hardcoded constants
    const double modnoise1 = 0.09;
    const double modnoise2 = 0.06;
    const double crossfeed = 0.4;
    // oversample buffer
    double osL[SF_REVERB_OF], osR[SF_REVERB_OF];
    double outLRef, outRRef;
    // early reflection
    earlyref_step(&rv->earlyref, inputL, inputR, &outLRef, &outRRef);
    double erL = outLRef * rv->ertolate + inputL;
    double erR = outRRef * rv->ertolate + inputR;
    // oversample the single input into multiple outputs
    oversample_stepup(&rv->oversampleL, erL, osL);
    oversample_stepup(&rv->oversampleR, erR, osR);
    // for each oversampled sample...
    for (int i2 = 0; i2 < rv->oversampleL.factor; i2++)
    {
        // dc cut
        double outL = dccut_step(&rv->dccutL, osL[i2]);
        double outR = dccut_step(&rv->dccutR, osR[i2]);
        // noise
        double mnoise = noise_step(&rv->noise);
        double lfo = (lfo_step(&rv->lfo1) + modnoise1 * mnoise) * rv->wander;
        lfo = iir1_step(&rv->lfo1_lpf, lfo);
        mnoise *= modnoise2;
        // diffusion
        for (int i = 0, s = -1; i < 10; i++, s = -s)
        {
            outL = allpassm_step(&rv->diffL[i], outL, lfo * s, mnoise);
            outR = allpassm_step(&rv->diffR[i], outR, lfo, mnoise * s);
        }
        // cross fade
        double crossL = outL, crossR = outR;
        for (int i = 0; i < 4; i++)
        {
            crossL = allpass_step(&rv->crossL[i], crossL);
            crossR = allpass_step(&rv->crossR[i], crossR);
        }
        outL = iir1_step(&rv->clpfL, outL + crossfeed * crossR);
        outR = iir1_step(&rv->clpfR, outR + crossfeed * crossL);
        // bass boost
        crossL = delay_getlast(&rv->cdelayL);
        crossR = delay_getlast(&rv->cdelayR);
        outL += rv->loopdecay *
                (crossR + rv->bassb * biquad_step(&rv->basslpL, biquad_step(&rv->bassapL, crossR)));
        outR += rv->loopdecay *
                (crossL + rv->bassb * biquad_step(&rv->basslpR, biquad_step(&rv->bassapR, crossL)));
        // dampening
        outL = allpassm_step(&rv->dampap2L,
                             delay_step(&rv->dampdL,
                                        allpassm_step(&rv->dampap1L,
                                                iir1_step(&rv->damplpL, outL), lfo, mnoise)),
                             -lfo, -mnoise);
        outR = allpassm_step(&rv->dampap2R,
                             delay_step(&rv->dampdR,
                                        allpassm_step(&rv->dampap1R,
                                                iir1_step(&rv->damplpR, outR), -lfo, -mnoise)),
                             lfo, mnoise);
        // update cross fade bass boost delay
        delay_step(&rv->cdelayL,
                   allpass3_step(&rv->cbassap2L,
                                 delay_step(&rv->cbassd2L,
                                            allpass2_step(&rv->cbassap1L,
                                                    delay_step(&rv->cbassd1L, outL))),
                                 lfo));
        delay_step(&rv->cdelayR,
                   allpass3_step(&rv->cbassap2R,
                                 delay_step(&rv->cbassd2R,
                                            allpass2_step(&rv->cbassap1R,
                                                    delay_step(&rv->cbassd1R, outR))),
                                 -lfo));
        //
        double D1 =
            delay_get(&rv->cbassd1L, rv->outco[0]);
        double D2 =
            delay_get(&rv->cbassd2L, rv->outco[1]) -
            delay_get(&rv->cbassd2R, rv->outco[2]) +
            delay_get(&rv->cbassd2L, rv->outco[3]) -
            delay_get(&rv->cdelayR, rv->outco[4]) -
            delay_get(&rv->cbassd1R, rv->outco[5]) -
            delay_get(&rv->cbassd2R, rv->outco[6]);
        double D3 =
            delay_get(&rv->cdelayL, rv->outco[7]) +
            allpass2_get1(&rv->cbassap1L, rv->outco[8]) +
            allpass2_get2(&rv->cbassap1L, rv->outco[9]) -
            allpass2_get2(&rv->cbassap1R, rv->outco[10]) +
            allpass3_get1(&rv->cbassap2L, rv->outco[11]) +
            allpass3_get2(&rv->cbassap2L, rv->outco[12]) +
            allpass3_get3(&rv->cbassap2L, rv->outco[13]) -
            allpass3_get2(&rv->cbassap2R, rv->outco[14]);
        double D4 =
            delay_get(&rv->cdelayL, rv->outco[15]);
        double B1 =
            delay_get(&rv->cbassd1R, rv->outco[16]);
        double B2 =
            delay_get(&rv->cbassd2R, rv->outco[17]) -
            delay_get(&rv->cbassd2L, rv->outco[18]) +
            delay_get(&rv->cbassd2R, rv->outco[19]) -
            delay_get(&rv->cdelayL, rv->outco[20]) -
            delay_get(&rv->cbassd1L, rv->outco[21]) -
            delay_get(&rv->cbassd2L, rv->outco[22]);
        double B3 =
            delay_get(&rv->cdelayR, rv->outco[23]) +
            allpass2_get1(&rv->cbassap1R, rv->outco[24]) +
            allpass2_get2(&rv->cbassap1R, rv->outco[25]) -
            allpass2_get2(&rv->cbassap1L, rv->outco[26]) +
            allpass3_get1(&rv->cbassap2R, rv->outco[27]) +
            allpass3_get2(&rv->cbassap2R, rv->outco[28]) +
            allpass3_get3(&rv->cbassap2R, rv->outco[29]) -
            allpass3_get2(&rv->cbassap2L, rv->outco[30]);
        double B4 =
            delay_get(&rv->cdelayR, rv->outco[31]);
        double D = D1 * 0.469 + D2 * 0.219 + D3 * 0.064 + D4 * 0.045;
        double B = B1 * 0.469 + B2 * 0.219 + B3 * 0.064 + B4 * 0.045;
        lfo = iir1_step(&rv->lfo2_lpf, lfo_step(&rv->lfo2) * rv->wander);
        outL = comb_step(&rv->combL, D, lfo);
        outR = comb_step(&rv->combR, B, -lfo);
        outL = delay_step(&rv->lastdelayL, biquad_step(&rv->lastlpfL, outL));
        outR = delay_step(&rv->lastdelayR, biquad_step(&rv->lastlpfR, outR));
        osL[i2] = outL * rv->wet1 + outR * rv->wet2 +
                  delay_step(&rv->inpdelayL, osL[i2]) * rv->dry;
        osR[i2] = outR * rv->wet1 + outL * rv->wet2 +
                  delay_step(&rv->inpdelayR, osR[i2]) * rv->dry;
    }
    double outL = oversample_stepdown(&rv->oversampleL, osL);
    double outR = oversample_stepdown(&rv->oversampleR, osR);
    outL += outLRef * rv->erefwet + inputL * rv->dry;
    outR += outRRef * rv->erefwet + inputR * rv->dry;
    *outputL = outL;
    *outputR = outR;
}
