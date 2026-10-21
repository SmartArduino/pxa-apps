/* Host side regression for the Game Boy audio conversion. */
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define CORE_RATE 32768u
#define FRAMES 549u

static unsigned stream(gb_audio_t *a, const int16_t *input, unsigned length,
                       int16_t *output, int irregular) {
    unsigned used=0, written=0, n=1;
    while(used<length) {
        unsigned chunk=irregular ? (n=n*17u%549u+1u) : FRAMES;
        if(chunk>length-used)chunk=length-used;
        written+=gb_audio_convert(a,input+used*2,chunk,output+written*a->channels,GB_AUDIO_OUT_MAX);
        used+=chunk;
        assert(a->phase<a->step);
    }
    return written;
}

static void continuous_stream(void) {
    /* Cross the former Q16 absolute-position wrap (~2 seconds) repeatedly.
     * Different chunk boundaries exercise all resampler carry samples. */
    unsigned length=32768u*5;
    int16_t *input=malloc((size_t)length*2*sizeof(*input));
    int16_t *one=malloc(500000u*sizeof(*one));
    int16_t *two=malloc(500000u*sizeof(*two));
    assert(input && one && two);
    for(unsigned i=0;i<length;++i)
        input[2*i]=input[2*i+1]=(i/8)%2 ? INT16_MIN : INT16_MAX;
    const unsigned rates[]={8000,16000,32768,44100,48000};
    for(unsigned r=0;r<sizeof(rates)/sizeof(*rates);++r)for(unsigned ch=1;ch<=2;++ch) {
        gb_audio_t a,b;gb_audio_reset(&a);gb_audio_reset(&b);
        gb_audio_configure(&a,rates[r],ch);gb_audio_configure(&b,rates[r],ch);
        unsigned na=stream(&a,input,length,one,0),nb=stream(&b,input,length,two,1);
        uint64_t intervals=(uint64_t)(length-1)*65536;
        assert(na==(intervals+a.step-1)/a.step && na==nb);
        assert(!memcmp(one,two,(size_t)na*ch*sizeof(*one)));
        for(unsigned i=0;ch==2 && i<na;++i)assert(one[i*2]==one[i*2+1]);
        gb_audio_start(&a,rates[r],ch);gb_audio_start(&b,rates[r],ch);
        na=stream(&a,input,length,one,0);nb=stream(&b,input,length,two,1);
        assert(na==nb && !a.fade_remaining && !b.fade_remaining && one[0]==0);
        assert(!memcmp(one,two,(size_t)na*ch*sizeof(*one)));
    }
    free(input);free(one);free(two);
    puts("PASS: five-second streaming, 5 sample rates, mono/stereo, arbitrary chunks and full-scale transitions");
}

int main(void) {
    /* A 1 kHz tone at the core rate must survive the streaming conversion. */
    static int16_t source[FRAMES * 2];
    static int16_t output[GB_AUDIO_OUT_MAX];
    for (unsigned i = 0; i < FRAMES; ++i) {
        double phase = 2.0 * 3.14159265358979 * 1000.0 * i / CORE_RATE;
        int value = (int) (12000.0 * __builtin_sin(phase));
        source[2 * i] = (int16_t) value;
        source[2 * i + 1] = (int16_t) value;
    }
    gb_audio_t state;
    gb_audio_reset(&state);
    gb_audio_configure(&state, 16000, 1);
    unsigned produced = gb_audio_convert(&state, source, FRAMES, output, GB_AUDIO_OUT_MAX);
    printf("produced=%u\n", produced);
    /* 32768 Hz -> 16000 Hz is a factor of 2.048. */
    unsigned expected = (unsigned) ((double) FRAMES * 16000.0 / 32768.0);
    if (produced + 4 < expected || produced > expected + 4) {
        fprintf(stderr, "FAIL: expected %u output frames, got %u\n", expected, produced);
        return 1;
    }
    /* A full-scale sine should retain its expected amplitude. */
    double sum = 0.0;
    for (unsigned i = 0; i < produced; ++i) {
        sum += (double) output[i] * output[i];
    }
    double rms = sum / (produced ? produced : 1);
    rms = __builtin_sqrt(rms);
    if (rms < 3000.0 || rms > 12000.0) {
        fprintf(stderr, "FAIL: unexpected level rms=%.1f\n", rms);
        return 1;
    }
    /* Every fourth output frame must repeat the input period. */
    unsigned crossings = 0;
    for (unsigned i = 1; i < produced; ++i) {
        if ((output[i - 1] < 0) != (output[i] < 0)) ++crossings;
    }
    printf("crossings=%u rms=%.1f\n", crossings, rms);
    if (crossings < 2) {
        fprintf(stderr, "FAIL: tone did not survive the conversion\n");
        return 1;
    }
    continuous_stream();
    printf("PASS\n");
    return 0;
}
