void SaveProgress() {
  EEPROM.update(16, Lives);
  EEPROM.update(17, PlayerX);
  EEPROM.put(18, Score);
  EEPROM.update(22, Invulnerable);
  for (byte i = 0; i < 4; i++) {
    EEPROM.update(23 + i, BarrierHP[i]);
  }

  EEPROM.update(27, Level);
  EEPROM.update(28, GameType);
  EEPROM.update(32, Freeze);
  EEPROM.update(33, MegaShot);
  if (GameType == 1) {
    EEPROM.update(29, NewType[0]);
    EEPROM.update(30, NewType[1]);
    EEPROM.update(31, NewType[2]);
  }
  if (GameType == 2) {
    EEPROM.update(29, AppearingEnemyType);
  }
    
}

void EraseSaveProgress() {
  for (byte i = 16; i < 33; i++) {
    EEPROM.update(i, 0);
  }
}

