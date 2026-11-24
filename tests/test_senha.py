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
#define F_CPU 16000000UL
#define _BV(x) (1U<<(x))
#define ISR(vector) void vector()
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(type) for (bool atomicOnce=true; atomicOnce; atomicOnce=false)
const int PC2=2, WGM21=1, OCF2A=1, OCIE2A=1, CS21=1;
uint8_t TCCR2A=0,TCCR2B=0,TCNT2=0,OCR2A=0,TIFR2=0,TIMSK2=0,PINC=0,PORTC=0;
unsigned int keys=0;
int activeRow=-1;
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
struct EEPROMClass {
  char data[64]; int writes=0;
  EEPROMClass() { memset(data,255,sizeof(data)); }
  byte read(int address) { assert(address>=0 && address<64); return data[address]; }
  void update(int address, byte value) {
    assert(address>=0 && address<64);
    if (read(address)!=value) { data[address]=value; ++writes; }
  }
  template<class T> void get(int address, T &value) {
    assert(address>=0 && address+sizeof(value)<=sizeof(data));
    memcpy(&value,data+address,sizeof(value));
  }
  template<class T> void put(int address, const T &value) {
    assert(address>=0 && address+sizeof(value)<=sizeof(data));
    const byte *p=reinterpret_cast<const byte*>(&value);
    for (size_t i=0; i<sizeof(value); ++i) update(address+i,p[i]);
  }
} EEPROM;
'''
TESTS = r'''
void ticks(uint32_t count) {
  while (count--) { TIMER2_COMPA_vect(); loop(); }
}
void runMs(uint32_t ms) { ticks(ms * TICKS_POR_MS); }
void enter(const char *text) { while (*text) tratarTecla(*text++); }
void release() { keys=0; runMs(50); }
void press(unsigned int mask) { keys=mask; runMs(50); release(); }
void stopped() { assert(trava.enabled && trava.pulse==SERVO_PARADO_US); }
void restart() {
  strcpy(senha,"1234"); modo=NORMAL; quantidade=0; digitado[0]=0;
  keys=0; setup(); runMs(100); assert(fase==LIVRE || fase==ESPERA_REC); stopped();
}
void wrongPassword() {
  assert(fase==ERRO_SONORO && buzzerLigado && !portaAberta);
  PINC=0; ticks(1); assert(PINC==_BV(PC2));
  ticks(1500*TICKS_POR_MS-2);
  assert(buzzerLigado && fase==ERRO_SONORO);
  ticks(1); assert(!buzzerLigado && fase==ERRO_SILENCIOSO);
  assert(!(PORTC & _BV(PC2)));
  runMs(199); assert(fase==ERRO_SILENCIOSO);
  runMs(1); assert(fase==LIVRE && quantidade==0);
}
int main() {
  restart();
  assert(OCR2A==249 && TCCR2A==_BV(WGM21) && TCCR2B==_BV(CS21));
  assert(TIMSK2==_BV(OCIE2A));
  assert(strcmp(senha,"1234")==0 && !portaAberta);
  assert(strcmp(lcd.lines[0],"Chave 16 digitos")==0);
  const char *key="0123456789012345";
  enter(key); enter("#"); assert(confirmarChave && !chaveConfigurada);
  enter("9999999999999999#"); assert(!confirmarChave && !chaveConfigurada);
  enter(key); enter("#"); enter(key); enter("#");
  assert(chaveConfigurada && EEPROM.read(ADDR_REC_MAGIC)==REC_MAGIC);
  assert(memcmp(chaveRec,key,16)==0);
  restart(); assert(chaveConfigurada);
  assert(strcmp(lcd.lines[0],"Digite a senha")==0);
  // Vetor oficial SipHash para mensagem de oito bytes 00..07.
  char vectorKey[16]; for (int i=0;i<16;i++) vectorKey[i]=char(i);
  assert(sipHash8(vectorKey,0x0706050403020100ULL)==0x93f5f5799a932462ULL);
  for (int key=0; key<12; ++key) {
    keys=1U<<key;
    char read=0; bool done=false;
    for (int i=0; i<16 && !done; ++i) {
      TIMER2_COMPA_vect(); done=lerTeclado(read);
    }
    done=false;
    for (int i=0; i<16 && !done; ++i) {
      TIMER2_COMPA_vect(); done=lerTeclado(read);
    }
    assert(done && read==TECLAS[key/3][key%3]);
  }
  release();
  press(3); assert(quantidade==0);
  keys=1; runMs(40); assert(quantidade==0);
  runMs(2); assert(quantidade==1 && digitado[0]=='1');
  runMs(1000); assert(quantidade==1); release();
  press(1U<<9); assert(quantidade==0);
  const unsigned int combo=(1U<<9)|(1U<<11);
  keys=combo; runMs(4900); release(); assert(modo==NORMAL);
  keys=combo; runMs(5040); assert(modo==NORMAL);
  runMs(2); assert(modo==VALIDAR_ATUAL && !portaAberta);
  runMs(6000); assert(modo==VALIDAR_ATUAL);
  keys=1U<<11; runMs(50); assert(fase==LIVRE && quantidade==0);
  release();
  assert(strcmp(lcd.lines[0],"Senha atual:")==0);
  enter("9999#"); wrongPassword();
  assert(modo==VALIDAR_ATUAL && erros==1);
  enter("123#"); assert(fase==AVISO_PRESERVAR && quantidade==3);
  runMs(900); assert(fase==LIVRE && quantidade==3);
  enter("4#"); assert(modo==DEFINIR_NOVA && quantidade==0);
  assert(strcmp(lcd.lines[0],"Nova senha:")==0);
  enter("56#"); runMs(900);
  assert(modo==DEFINIR_NOVA && quantidade==2 && erros==0);
  enter("*000078#"); assert(fase==AVISO_LIMPAR);
  assert(strcmp(senha,"0000")==0 && erros==0);
  runMs(1200); assert(fase==LIVRE && modo==NORMAL && quantidade==0);
  stopped(); restart(); assert(strcmp(senha,"0000")==0);
  enter("1234#"); wrongPassword(); assert(erros==1);
  restart(); assert(erros==1);
  enter("1234#"); wrongPassword(); assert(erros==2);
  enter("1234#"); wrongPassword(); assert(bloqueado());
  assert(strcmp(lcd.lines[0],"ID: 000000010000")==0);
  uint32_t session=sessaoRec;
  uint32_t first=codigoRec();
  enter("0000#"); assert(!portaAberta && bloqueado());
  limparRec();
  runMs(60000); assert(janelaRec==1 && qtdRec==0);
  assert(codigoRec()!=first); // this fixed test key has different outputs
  restart(); assert(bloqueado() && fase==ESPERA_REC);
  runMs(30000); assert(sessaoRec!=session && fase==LIVRE);
  char recovery[7];
  snprintf(recovery,sizeof(recovery),"%06lu",(unsigned long)((codigoRec()+1)%1000000));
  enter(recovery); enter("#"); assert(fase==ESPERA_REC && bloqueado());
  runMs(30000); assert(fase==LIVRE && bloqueado());
  snprintf(recovery,sizeof(recovery),"%06lu",(unsigned long)codigoRec());
  enter(recovery); enter("#"); assert(!bloqueado() && !portaAberta && erros==0);
  assert(strcmp(lcd.lines[0],"Digite a senha")==0);
  enter("0000#");
  assert(fase==ABRINDO && !portaAberta && trava.pulse==SERVO_ABRIR_US);
  assert(EEPROM.read(sizeof(senha))==1);
  runMs(499); assert(trava.pulse==SERVO_ABRIR_US && !buzzerLigado);
  runMs(1); stopped(); assert(fase==NEUTRO_ABERTURA);
  runMs(99); assert(!buzzerLigado);
  runMs(1); assert(portaAberta && fase==BEEP_1 && buzzerLigado);
  tratarTecla('#'); assert(fase==BEEP_1);
  runMs(69); assert(buzzerLigado); runMs(1);
  assert(fase==INTERVALO_BEEPS && !buzzerLigado);
  runMs(49); assert(!buzzerLigado); runMs(1);
  assert(fase==BEEP_2 && buzzerLigado);
  runMs(69); assert(buzzerLigado); runMs(1);
  assert(fase==LIVRE && !buzzerLigado);
  stopped(); restart(); assert(portaAberta && !buzzerLigado);
  assert(strcmp(lcd.lines[0],"Para fechar:")==0);
  assert(strcmp(lcd.lines[1],"Aperte Confirmar")==0);
  enter("9*T"); assert(portaAberta && quantidade==0 && modo==NORMAL);
  enter("#"); assert(fase==FECHANDO && trava.pulse==SERVO_FECHAR_US);
  runMs(499); assert(trava.pulse==SERVO_FECHAR_US);
  runMs(1); stopped(); assert(fase==NEUTRO_FECHAMENTO);
  runMs(99); assert(EEPROM.read(sizeof(senha))==1);
  runMs(1); assert(!portaAberta && EEPROM.read(sizeof(senha))==0);
  assert(!buzzerLigado && fase==AVISO_LIMPAR);
  runMs(800); assert(fase==LIVRE);
  restart(); assert(!portaAberta && strcmp(senha,"0000")==0);
  enter("#"); runMs(900); stopped();
  ticksTimer=UINT32_MAX-100;
  enter("0000#"); runMs(499); assert(fase==ABRINDO);
  runMs(1); stopped(); assert(fase==NEUTRO_ABERTURA);
  restart(); assert(portaAberta);
  memcpy(EEPROM.data,"12x4",5); restart(); assert(strcmp(senha,"1234")==0);
  memcpy(EEPROM.data,"12345",5); restart(); assert(strcmp(senha,"1234")==0);
}
'''

with tempfile.TemporaryDirectory(prefix="cofre-test-") as tmp:
    folder = Path(tmp)
    (folder / "Wire.h").write_text(STUBS)
    for directory, header in [("avr", "interrupt.h"), ("util", "atomic.h")]:
        (folder / directory).mkdir()
        (folder / directory / header).write_text('#include "Wire.h"\n')
    import re
    assert not re.search(r"\b(?:delay|delayMicroseconds|millis|micros|tone|noTone)\s*\(", (ROOT / "sketch.ino").read_text())
    for name in ["LiquidCrystal_I2C.h", "Servo.h", "EEPROM.h"]:
        (folder / name).write_text('#include "Wire.h"\n')
    test = folder / "test.cpp"
    test.write_text(f'#include "{ROOT / "sketch.ino"}"\n' + TESTS)
    binary = folder / "test"
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++11", "-Wall", "-Wextra",
                    "-Werror", "-I", tmp, str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("OK: Timer2, teclado, senha, EEPROM, servo, bipes, SipHash e recuperação.")
