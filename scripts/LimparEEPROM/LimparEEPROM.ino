#include <EEPROM.h>

void setup() {
  Serial.begin(115200);
  for (int address = 0; address < EEPROM.length(); ++address) {
    EEPROM.update(address, 0xFF);
  }
}

void loop() {
  if (Serial.read() != '?') return;
  int cleared = 0;
  for (int address = 0; address < EEPROM.length(); ++address) {
    if (EEPROM.read(address) == 0xFF) ++cleared;
  }
  Serial.print(F("EEPROM bytes FF: "));
  Serial.print(cleared);
  Serial.print('/');
  Serial.println(EEPROM.length());
}
