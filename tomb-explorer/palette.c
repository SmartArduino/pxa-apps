#include "palette.h"

typedef struct {
    uint8_t r, g, b;
} tomb_ramp_t;

/* Brightest colour of each ramp; steps darken towards black. */
static const tomb_ramp_t kRampBright[16] = {
    {200u, 200u, 205u}, /* gray */
    {225u, 200u, 150u}, /* sand */
    {205u, 150u, 80u},  /* ochre */
    {140u, 95u, 60u},   /* brown */
    {95u, 140u, 70u},   /* moss */
    {60u, 150u, 140u},  /* teal */
    {70u, 95u, 190u},   /* blue */
    {240u, 200u, 70u},  /* gold */
    {180u, 55u, 45u},   /* red */
    {230u, 175u, 135u}, /* skin */
    {70u, 45u, 30u},    /* hair */
    {50u, 110u, 150u},  /* cloth */
    {110u, 70u, 40u},   /* leather */
    {235u, 225u, 200u}, /* bone */
    {40u, 90u, 130u},   /* water */
    {255u, 255u, 255u}, /* white */
};

uint16_t tomb_rgb565(uint32_t red, uint32_t green, uint32_t blue) {
    if (red > 255u) red = 255u;
    if (green > 255u) green = 255u;
    if (blue > 255u) blue = 255u;
    return (uint16_t)(((red & 0xF8u) << 8u) | ((green & 0xFCu) << 3u) |
                      (blue >> 3u));
}

void tomb_build_palette(uint16_t entries[TOMB_LIGHT_LEVELS * 256u]) {
    uint32_t light;
    uint32_t ramp;
    uint32_t step;
    for (light = 0u; light < TOMB_LIGHT_LEVELS; ++light) {
        uint16_t *row = entries + light * 256u;
        const uint32_t light_scale =
            24u + light * (256u - 24u) / (TOMB_LIGHT_LEVELS - 1u);
        for (ramp = 0u; ramp < 16u; ++ramp) {
            const tomb_ramp_t bright = kRampBright[ramp];
            for (step = 0u; step < TOMB_RAMP_STEPS; ++step) {
                const uint32_t step_scale =
                    40u + step * (256u - 40u) / (TOMB_RAMP_STEPS - 1u);
                const uint32_t scale = step_scale * light_scale / 256u;
                uint32_t blue = bright.b * scale / 256u;
                if (light < 6u) {
                    blue += (6u - light) * 3u;
                    if (blue > 255u) blue = 255u;
                }
                row[ramp * TOMB_RAMP_STEPS + step] = tomb_rgb565(
                    bright.r * scale / 256u, bright.g * scale / 256u, blue);
            }
        }
        row[0] = 0u;
    }
}
