#include <EEPROM.h>

// Sketch temporário. O gravador coloca o firmware do cofre de volta ao final.
void setup() {
  Serial.begin(115200);
  // 0xFF é o valor apagado. update poupa gravações nos bytes já limpos.
  for (int address = 0; address < EEPROM.length(); ++address) {
    EEPROM.update(address, 0xFF);
  }
}

void loop() {
  // O computador pede uma leitura depois de abrir a serial.
  if (Serial.read() != '?') return;
  int cleared = 0;
  // Leia cada byte para que a confirmação reflita o estado real da EEPROM.
  for (int address = 0; address < EEPROM.length(); ++address) {
    if (EEPROM.read(address) == 0xFF) ++cleared;
  }
  Serial.print(F("EEPROM bytes FF: ")); // F() mantém o texto fixo na flash.
  Serial.print(cleared);
  Serial.print('/');
  Serial.println(EEPROM.length());
}
