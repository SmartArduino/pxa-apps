
#include "pxt.h"
#ifdef PXT_MAIN
PXT_MAIN
#else
int main() {
    uBit.init();
    pxt::start();
    release_fiber();
    return 0;   // your program will never reach this line.
}
#endif
