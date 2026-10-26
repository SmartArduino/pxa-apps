#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
class Arduboy2Base { public: uint16_t frameCount = 0; bool everyXFrames(uint8_t); };
bool Arduboy2Base::everyXFrames(uint8_t frames)
{
  return frameCount % frames == 0;
}
Arduboy2Base arduboy;
bool safeEveryXFrames(long sampled) {
  const uint8_t frames = static_cast<uint8_t>(sampled);
  return frames != 0 && arduboy.everyXFrames(frames);
}

static unsigned calls = 0;
long observedRandom(long value) { ++calls; return value; }
int main(int argc, char **argv) {
  if(argc != 3) return 2;
  const long sample = std::strtol(argv[2],nullptr,10);
  if(std::strcmp(argv[1],"original")==0) {
    arduboy.frameCount=60;
    const uint8_t narrowed=static_cast<uint8_t>(sample);
    std::printf("ORIGINAL sample=%ld narrowed=%u\n",sample,narrowed);
    std::fflush(stdout);
    volatile bool result=arduboy.everyXFrames(narrowed);
    return result ? 0 : 0;
  }
  uint64_t compared=0, skipped=0;
  for(uint32_t frame=0;frame<=65535;frame++) {
    arduboy.frameCount=static_cast<uint16_t>(frame);
    for(long value=0;value<480;value++) {
      const uint8_t narrowed=static_cast<uint8_t>(value);
      const unsigned before=calls;
      const bool patched=safeEveryXFrames(observedRandom(value));
      if(calls!=before+1) return 3;
      if(narrowed==0) {
        if(patched) return 4;
        skipped++;
      } else {
        const bool original=arduboy.everyXFrames(narrowed);
        if(patched!=original) return 5;
        compared++;
      }
    }
  }
  std::printf("PATCHED nonzero_comparisons=%llu zero_skips=%llu calls=%u\n",(unsigned long long)compared,(unsigned long long)skipped,calls);
  return 0;
}
