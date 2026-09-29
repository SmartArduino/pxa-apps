#include "images.h"
static const char *const paths[]={
    "assets/sun.pxr",
    "assets/moon.pxr",
    "assets/cloud.pxr",
    "assets/partly-cloudy.pxr",
    "assets/rain.pxr",
    "assets/snow.pxr",
    "assets/storm.pxr",
    "assets/fog.pxr",
    "assets/location.pxr",
    "assets/clock.pxr",
    "assets/info.pxr",
    "assets/refresh.pxr",
};
static uint64_t handles[WEATHER_IMAGE_COUNT];
pxa_image_set_t weather_images=PXA_IMAGE_SET_INIT(paths,handles,UINT64_C(0x57494d4700000000));
