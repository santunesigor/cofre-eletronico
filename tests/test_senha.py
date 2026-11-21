"""Regressão do sketch: python3 tests/test_senha.py (requer g++)."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STUBS = r'''
#pragma once
#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef uint8_t byte;
const int LOW=0, HIGH=1, INPUT=0, OUTPUT=1, INPUT_PULLUP=2, A2=16;
unsigned long clockMs=0;
unsigned int keys=0;
int activeRow=-1;
unsigned long millis() { return clockMs; }
void delayMicroseconds(unsigned int) {}
void digitalWrite(byte, int) {}
void pinMode(byte pin, int mode) {
  if (pin>=2 && pin<=5) {
    if (mode==OUTPUT) activeRow=pin-2;
    else if (activeRow==pin-2) activeRow=-1;
  }
}
int digitalRead(byte pin) {
  return activeRow>=0 && (keys & (1U<<(activeRow*3+pin-6))) ? LOW : HIGH;
}
void tone(byte, unsigned int, unsigned long) {}
void noTone(byte) {}
struct LiquidCrystal_I2C {
  char lines[2][17] = {}; int x=0, y=0;
  LiquidCrystal_I2C(byte, int, int) {}
  void clear() { memset(lines,0,sizeof(lines)); x=y=0; }
  void setCursor(int col,int row) { x=col; y=row; }
  void print(char c) { assert(x<16 && y<2); lines[y][x++]=c; }
  void print(const char *s) { while (*s) print(*s++); }
  void init() {} void backlight() {}
};
struct Servo {
  int pulse=-1; bool enabled=false;
  void attach(byte) { enabled=true; }
  void detach() { enabled=false; }
  void writeMicroseconds(int value) { pulse=value; }
};
extern Servo trava;
int pulses[32], pulseCount=0;
unsigned long durations[32];
void delay(unsigned long ms) {
  if (trava.enabled) {
    assert(pulseCount<32);
    pulses[pulseCount]=trava.pulse;
    durations[pulseCount++]=ms;
  }
}
struct EEPROMClass {
  char data[6]; int writes=0;
  EEPROMClass() { memset(data,255,sizeof(data)); }
  byte read(int address) { assert(address>=0 && address<6); return data[address]; }
  void update(int address, byte value) {
    assert(address>=0 && address<6);
    if (read(address)!=value) { data[address]=value; ++writes; }
  }
  template<class T> void get(int address, T &value) {
    assert(address>=0 && address+sizeof(value)<=sizeof(data));
    memcpy(&value,data+address,sizeof(value));
  }
  template<class T> void put(int address, const T &value) {
    assert(address>=0 && address+sizeof(value)<=sizeof(data));
    memcpy(data+address,&value,sizeof(value)); ++writes;
  }
} EEPROM;
'''
TESTS = r'''
void enter(const char *text) { while (*text) tratarTecla(*text++); }
char sample(unsigned int mask, unsigned long elapsed) {
  keys=mask; clockMs+=elapsed; return lerPressionamento();
}
void release() { sample(0,0); assert(sample(0,40)==0); }
void holdToChange() {
  const unsigned int combo=(1U<<9)|(1U<<11);
  assert(sample(combo,0)==0);
  assert(sample(combo,40)==0);
  assert(sample(combo,4999)==0);
  assert(modo==NORMAL);
  assert(sample(combo,1)==TECLA_TROCAR_SENHA);
  tratarTecla(TECLA_TROCAR_SENHA);
  assert(modo==VALIDAR_ATUAL && !portaAberta);
  assert(sample(combo,10000)==0);
  // Releasing Limpar first must not submit the current password.
  assert(sample(1U<<11,0)==0);
  assert(sample(1U<<11,40)==0);
  release();
}
void assertStopped() {
  assert(!trava.enabled && trava.pulse==SERVO_PARADO_US);
}
void assertMovement(int start, int direction) {
  assert(pulseCount==start+2);
  assert(pulses[start]==direction && durations[start]==TEMPO_MOVIMENTO_MS);
  assert(pulses[start+1]==SERVO_PARADO_US && durations[start+1]==100);
  assertStopped();
}
int main() {
  setup();
  assert(strcmp(senha,"1234")==0 && !portaAberta);
  assertStopped();
  assert(pulseCount==1 && pulses[0]==SERVO_PARADO_US);
  for (int key=0;key<12;key++) {
    keys=1U<<key; assert(lerTeclado()==TECLAS[key/3][key%3]);
    assert(activeRow==-1);
  }
  keys=3; assert(lerTeclado()==0);
  // Normal debounce and one event per press.
  assert(sample(1,0)==0); assert(sample(1,39)==0);
  assert(sample(1,1)=='1'); assert(sample(1,1000)==0); release();
  // An interrupted hold must start its five seconds again.
  const unsigned int combo=(1U<<9)|(1U<<11);
  sample(combo,0); sample(combo,40); assert(sample(combo,4900)==0);
  release();
  holdToChange();
  assert(strcmp(lcd.lines[0],"Senha atual:")==0);
  enter("9999#"); assert(modo==VALIDAR_ATUAL && !portaAberta);
  assert(strcmp(senha,"1234")==0 && EEPROM.writes==0);
  enter("123#"); assert(modo==VALIDAR_ATUAL && quantidade==3);
  enter("4#"); assert(modo==DEFINIR_NOVA && !portaAberta && quantidade==0);
  assert(strcmp(lcd.lines[0],"Nova senha:")==0);
  enter("56#"); assert(modo==DEFINIR_NOVA && quantidade==2 && EEPROM.writes==0);
  enter("*000078"); assert(strcmp(digitado,"0000")==0 && quantidade==4);
  enter("#");
  assert(modo==NORMAL && !portaAberta);
  assertStopped();
  assert(pulseCount==1);
  assert(strcmp(senha,"0000")==0 && EEPROM.writes==1);
  // Simulate RAM returning to its initial value on restart.
  strcpy(senha,"1234"); setup(); assert(strcmp(senha,"0000")==0);
  enter("1234#"); assert(!portaAberta);
  int start=pulseCount;
  enter("0000#"); assert(portaAberta);
  assertMovement(start,SERVO_ABRIR_US);
  assert(EEPROM.read(sizeof(senha))==1);
  // Restart with reset RAM while the physical safe is still open.
  portaAberta=false; strcpy(senha,"1234");
  start=pulseCount;
  setup();
  assert(portaAberta && strcmp(senha,"0000")==0);
  assert(pulseCount==start+1 && pulses[start]==SERVO_PARADO_US);
  assertStopped();
  assert(strcmp(lcd.lines[0],"Para fechar:")==0);
  assert(strcmp(lcd.lines[1],"Aperte Confirmar")==0);
  enter("9*"); assert(portaAberta && quantidade==0);
  tratarTecla(TECLA_TROCAR_SENHA); assert(modo==NORMAL && portaAberta);
  start=pulseCount;
  enter("#"); assert(!portaAberta);
  assertMovement(start,SERVO_FECHAR_US);
  assert(EEPROM.read(sizeof(senha))==0);
  assert(strcmp(senha,"0000")==0);
  portaAberta=true;
  start=pulseCount;
  setup();
  assert(!portaAberta && strcmp(senha,"0000")==0);
  assert(pulseCount==start+1 && pulses[start]==SERVO_PARADO_US);
  assert(strcmp(lcd.lines[0],"Digite a senha")==0);
  start=pulseCount; enter("#"); assert(pulseCount==start && !portaAberta);
  assert(SERVO_ABRIR_US>SERVO_PARADO_US && SERVO_FECHAR_US<SERVO_PARADO_US);
  // Invalid persisted bytes must never become a password.
  memcpy(EEPROM.data,"12x4",5); strcpy(senha,"1234"); setup();
  assert(strcmp(senha,"1234")==0);
  memcpy(EEPROM.data,"12345",5); setup(); assert(strcmp(senha,"1234")==0);
}
'''

with tempfile.TemporaryDirectory(prefix="cofre-test-") as tmp:
    folder = Path(tmp)
    (folder / "Wire.h").write_text(STUBS)
    for name in ["LiquidCrystal_I2C.h", "Servo.h", "EEPROM.h"]:
        (folder / name).write_text('#include "Wire.h"\n')
    test = folder / "test.cpp"
    test.write_text(f'#include "{ROOT / "sketch.ino"}"\n' + TESTS)
    binary = folder / "test"
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++11", "-Wall", "-Wextra",
                    "-Werror", "-I", tmp, str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("OK: teclado, debounce, retenção por 5 s, autenticação, limite de 4 dígitos, EEPROM, recuperação aberto/fechado após reiniciar e servo temporizado com parada.")
