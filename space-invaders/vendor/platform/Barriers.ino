//When called, a 4-note melody is played as the 4 barriers are spawned
port::Task BuildBarriers() {
  if (!TitleScreenMove) {
  for (MusicPlace = 0; MusicPlace < 4; MusicPlace) {
    Render(MusicPlace, 0, 12, 0);
    BarrierHP[MusicPlace] = 8;
    Render(MusicPlace, 0, 12, 1);
    Display;
    MELODY(F("^*)C"), 9);
    co_await port::Delay{(uint32_t)(256), __LINE__};
  }
  }

  co_return;
}

//Used to detect is Xpos is within one of the barriers, and if so, which one it is (0-3).
//If Xpos is not a position within a barrier, this function returns 5.
byte WithinBarrier(byte Xpos) {
  byte BarrierNo = 5;   //5 means NOT within barrier.
  for (byte h = 0; h < 4; h++) {
    if (BarrierHP[h]) {
      if ((Xpos <= ((h * 32 + 16) + BarrierHP[h])) && (Xpos >= ((h * 32 + 16) - BarrierHP[h]))) {
        BarrierNo = h;
      }
    }
  }
  return BarrierNo;
}

//Reduce the barrier's HP by one point. Uses WithinBarrier to detect which one the barrier to damage.
void DestroyBarrier(byte Xposi) {
  byte InBarrier = WithinBarrier(Xposi);
  if (InBarrier >= 4) return; // no barrier: never index the sentinel 5
  Render(InBarrier, 0, 12, 0);
    
  BarrierHP[InBarrier]--;
  
  SOUND(1, BarrierHP[InBarrier] * 28 + 40, 2);
  
  if (BarrierHP[InBarrier] <= 2) {
    BarrierHP[InBarrier] = 0;
    Blast(InBarrier * 32 + 12, 49);
  }
    
}

//Render the barriers.
void DrawBarriers() {
  for (byte h = 0; h < 4; h++) {
    if (BarrierHP[h]) {
      Render(h, 0, 12, 1);
    }
  }
}

