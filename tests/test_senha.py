"""Regressão do sketch: python3 tests/test_senha.py (requer g++)."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SKETCH = ROOT / "CofreEletronico" / "CofreEletronico.ino"
# Substitutos das APIs do Uno para executar o sketch no computador.
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
const int WDIE=6, WDIF=7;
uint8_t WDTCSR=0;
void wdt_disable() { WDTCSR=0; }
unsigned int keys=0;
int activeRow=-1;
// O simulador responde LOW só na coluna da linha selecionada.
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
  // O assert em print detecta texto que ultrapassa a matriz física de 16 × 2.
  char lines[2][17] = {}; int x=0, y=0;
  LiquidCrystal_I2C(byte, int, int) {}
  void clear() { memset(lines,0,sizeof(lines)); x=y=0; }
  void setCursor(int col,int row) { x=col; y=row; }
  void print(char c) { assert(x<16 && y<2); lines[y][x++]=c; }
  void print(const char *s) { while (*s) print(*s++); }
  void init() {} void backlight() {}
};
struct Servo {
  // Guarde o último pulso para conferir direção e parada sem motor físico.
  int pulse=-1; bool enabled=false;
  void attach(byte) { enabled=true; }
  void detach() { enabled=false; }
  void writeMicroseconds(int value) { pulse=value; }
};
struct EEPROMClass {
  // A mesma instância sobrevive às reinicializações simuladas.
  char data[1024]; int writes=0;
  EEPROMClass() { memset(data,255,sizeof(data)); }
  byte read(int address) { assert(address>=0 && address<1024); return data[address]; }
  void update(int address, byte value) {
    assert(address>=0 && address<1024);
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
// Avance o Timer2 a cada chamada. O teste não espera tempo de relógio real.
void ticks(uint32_t count) {
  while (count--) { TIMER2_COMPA_vect(); loop(); }
}
void runMs(uint32_t ms) { ticks(ms * TICKS_POR_MS); }
// enter envia teclas já reconhecidas; press passa pela varredura e pelo debounce.
void enter(const char *text) { while (*text) tratarTecla(*text++); }
void release() { keys=0; runMs(50); }
void press(unsigned int mask) { keys=mask; runMs(50); release(); }
void stopped() { assert(trava.enabled && trava.pulse==SERVO_PARADO_US); }
void amostrarWatchdog(byte bit) {
  TCNT2=bit;
  WDT_vect();
  loop();
}
void simularDigito(byte valor) {
  for (int i=3; i>=0; --i) {
    byte bit=(valor>>i)&1;
    amostrarWatchdog(bit);
    amostrarWatchdog(1-bit);
  }
}
void restart() {
  // Recrie a partida e preserve a EEPROM simulada.
  strcpy(senha,"1234"); modo=NORMAL; quantidade=0; digitado[0]=0;
  keys=0; setup(); runMs(100);
  assert(fase==LIVRE || fase==ESPERA_REC || fase==GERANDO_CODIGO); stopped();
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
  // Primeiro uso: código visível, três pressões, cadastro e confirmação do PIN.
  restart();
  assert(OCR2A==249 && TCCR2A==_BV(WGM21) && TCCR2B==_BV(CS21));
  assert(TIMSK2==_BV(OCIE2A));
  assert(strcmp(senha,"1234")==0 && !portaAberta);
  assert(totalHist==1 && ultimaSeqHist==1);
  assert(fase==GERANDO_CODIGO && WDTCSR&_BV(WDIE));
  assert(strcmp(lcd.lines[0],"Gerando codigo")==0 && EEPROM.read(ADDR_REC_MAGIC)==255);
  // Pares iguais e valores acima de 9 não produzem dígitos.
  amostrarWatchdog(0); amostrarWatchdog(0);
  simularDigito(15);
  assert(digitosGerados==0 && EEPROM.read(ADDR_REC_MAGIC)==255);
  const char *key="0123456789012345";
  for (const char *d=key; *d; ++d) simularDigito(*d-'0');
  assert(fase==LIVRE && WDTCSR==0);
  assert(strcmp(lcd.lines[0],key)==0);
  assert(strcmp(lcd.lines[1],"Confirmar: 3x")==0);
  assert(EEPROM.read(ADDR_REC_MAGIC)==REC_PENDENTE);
  enter("1234*"); assert(confirmacoesChave==3 && !portaAberta);
  keys=1U<<11; runMs(1000); assert(confirmacoesChave==2); release();
  press(1U<<11); assert(confirmacoesChave==1);
  restart(); assert(confirmacoesChave==3 && strcmp(lcd.lines[0],key)==0);
  for (int i=0; i<3; ++i) press(1U<<11);
  assert(EEPROM.read(ADDR_REC_MAGIC)==PIN_PENDENTE);
  assert(strcmp(lcd.lines[0],"Defina seu PIN")==0);
  enter("43#"); runMs(900); assert(quantidade==2 && modo==DEFINIR_INICIAL);
  restart();
  assert(confirmacoesChave==0 && strcmp(lcd.lines[0],"Defina seu PIN")==0);
  enter("4321#");
  assert(confirmandoPin && strcmp(lcd.lines[0],"Confirme seu PIN")==0);
  assert(EEPROM.read(ADDR_REC_MAGIC)==PIN_PENDENTE && EEPROM.read(ADDR_PIN)==255);
  enter("4320#");
  assert(strcmp(lcd.lines[0],"PINs diferentes")==0 && EEPROM.read(ADDR_PIN)==255);
  runMs(1200);
  assert(strcmp(lcd.lines[0],"Defina seu PIN")==0);
  enter("4321#4321#");
  assert(!portaAberta && strcmp(lcd.lines[0],"PIN cadastrado!")==0);
  assert(chaveConfigurada && EEPROM.read(ADDR_REC_MAGIC)==REC_MAGIC);
  assert(memcmp(codigoRec,key,16)==0);
  restart(); assert(chaveConfigurada);
  assert(strcmp(lcd.lines[0],"Digite a senha")==0 && strcmp(senha,"4321")==0);
  // Confira a correspondência entre as 12 teclas físicas e a tabela do firmware.
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
  // Segurar por menos de cinco segundos não entra no menu de troca do PIN.
  keys=combo; runMs(4900); release(); assert(modo==NORMAL);
  keys=combo; runMs(5040); assert(modo==NORMAL);
  runMs(2); assert(modo==VALIDAR_ATUAL && !portaAberta);
  runMs(6000); assert(modo==VALIDAR_ATUAL);
  keys=1U<<11; runMs(50); assert(fase==LIVRE && quantidade==0);
  release();
  assert(strcmp(lcd.lines[0],"Senha atual:")==0);
  enter("9999#"); wrongPassword();
  assert(modo==VALIDAR_ATUAL && erros==1);
  enter("432#"); assert(fase==AVISO_PRESERVAR && quantidade==3);
  runMs(900); assert(fase==LIVRE && quantidade==3);
  enter("1#"); assert(modo==DEFINIR_NOVA && quantidade==0);
  assert(strcmp(lcd.lines[0],"Novo PIN:")==0);
  enter("56#"); runMs(900);
  assert(modo==DEFINIR_NOVA && quantidade==2 && erros==0);
  enter("*000078#"); assert(confirmandoPin && strcmp(senha,"4321")==0);
  enter("0000#"); assert(fase==AVISO_LIMPAR);
  assert(strcmp(senha,"0000")==0 && erros==0);
  runMs(1200); assert(fase==LIVRE && modo==NORMAL && quantidade==0);
  stopped(); restart(); assert(strcmp(senha,"0000")==0);
  enter("1234#"); wrongPassword(); assert(erros==1);
  // O contador persiste após reiniciar e o terceiro erro abre a tela de recuperação.
  restart(); assert(erros==1);
  enter("1234#"); wrongPassword(); assert(erros==2);
  enter("1234#"); wrongPassword(); assert(bloqueado());
  assert(strcmp(lcd.lines[0],"Cod. recuperacao")==0);
  // Verifique cada comprimento, inclusive a linha cheia e a rejeição do 17º dígito.
  for (int n=1; n<=16; ++n) {
    tratarTecla(key[n-1]);
    assert(strlen(lcd.lines[1])==size_t(n) && lcd.lines[1][n-1]=='*');
  }
  enter("9"); assert(strcmp(lcd.lines[1],"****************")==0);
  enter("*"); assert(lcd.lines[1][0]==0);
  enter("0000#"); runMs(900);
  assert(bloqueado() && quantidade==4 && strcmp(lcd.lines[1],"****")==0);
  restart(); assert(bloqueado() && fase==ESPERA_REC);
  runMs(30000); assert(fase==LIVRE && quantidade==0);
  enter(key); enter("#0099#");
  assert(bloqueado() && confirmandoPin && !portaAberta);
  // Reiniciar durante a confirmação exige o código de novo.
  restart(); assert(bloqueado() && fase==ESPERA_REC);
  runMs(30000);
  enter("9999999999999999#"); assert(fase==ESPERA_REC && bloqueado());
  runMs(29999); assert(fase==ESPERA_REC);
  runMs(1); assert(fase==LIVRE && bloqueado());
  enter(key); enter("#"); assert(bloqueado() && !portaAberta);
  assert(strcmp(lcd.lines[0],"Novo PIN:")==0);
  enter("0073#0072#"); runMs(1200);
  assert(bloqueado() && strcmp(senha,"0000")==0 && strcmp(lcd.lines[0],"Novo PIN:")==0);
  enter("0073#0073#"); runMs(1200);
  assert(!bloqueado() && !portaAberta && erros==0);
  assert(strcmp(lcd.lines[0],"Digite a senha")==0 && strcmp(senha,"0073")==0);
  enter("0073#");
  // Avance até a borda de cada intervalo para pegar transições adiantadas ou atrasadas.
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
  uint32_t seqLog; byte eventoLog;
  assert(consultarHistorico(0,seqLog,eventoLog) && eventoLog==EV_ABERTURA);
  assert(consultarHistorico(1,seqLog,eventoLog) && eventoLog==EV_REC_OK);
  assert(consultarHistorico(2,seqLog,eventoLog) && eventoLog==EV_REC_ERRADA);
  enter("0"); assert(vendoHist && posHist==0 && portaAberta);
  assert(strcmp(lcd.lines[1],"Cofre aberto")==0);
  enter("2"); assert(posHist==1 && strcmp(lcd.lines[1],"Desbloqueado")==0);
  enter("8"); assert(posHist==0);
  enter("8"); assert(posHist==0);
  enter("*"); assert(!vendoHist && portaAberta);
  enter("0"); runMs(30000); assert(!vendoHist && portaAberta);
  enter("0#"); assert(!vendoHist && fase==LIVRE && portaAberta);
  stopped(); restart(); assert(portaAberta && !buzzerLigado);
  assert(strcmp(lcd.lines[0],"Para fechar:")==0);
  assert(strcmp(lcd.lines[1],"Aperte Confirmar")==0);
  enter("90*T"); assert(portaAberta && quantidade==0 && modo==NORMAL && !vendoHist);
  assert(consultarHistorico(0,seqLog,eventoLog) && eventoLog==EV_INICIO);
  assert(consultarHistorico(1,seqLog,eventoLog) && eventoLog==EV_ABERTURA);
  enter("#"); assert(fase==FECHANDO && trava.pulse==SERVO_FECHAR_US);
  runMs(499); assert(trava.pulse==SERVO_FECHAR_US);
  runMs(1); stopped(); assert(fase==NEUTRO_FECHAMENTO);
  runMs(99); assert(EEPROM.read(sizeof(senha))==1);
  runMs(1); assert(!portaAberta && EEPROM.read(sizeof(senha))==0);
  assert(!buzzerLigado && fase==AVISO_LIMPAR);
  assert(consultarHistorico(0,seqLog,eventoLog) && eventoLog==EV_FECHAMENTO);
  runMs(800); assert(fase==LIVRE);
  restart(); assert(!portaAberta && strcmp(senha,"0073")==0);
  enter("#"); runMs(900); stopped();
  ticksTimer=UINT32_MAX-100;
  enter("0073#"); runMs(499); assert(fase==ABRINDO);
  runMs(1); stopped(); assert(fase==NEUTRO_ABERTURA);
  restart(); assert(portaAberta);
  memcpy(EEPROM.data,"12x4",5); restart(); assert(strcmp(senha,"1234")==0);
  memcpy(EEPROM.data,"12345",5); restart(); assert(strcmp(senha,"1234")==0);
  char dadosSeguranca[32]; memcpy(dadosSeguranca,EEPROM.data,32);
  for (int i=0;i<MAX_HIST+5;++i) registrarEvento(EV_PIN_ERRADO);
  assert(totalHist==MAX_HIST);
  assert(consultarHistorico(0,seqLog,eventoLog) && seqLog==ultimaSeqHist);
  assert(consultarHistorico(MAX_HIST-1,seqLog,eventoLog) && seqLog==ultimaSeqHist-MAX_HIST+1);
  assert(!consultarHistorico(MAX_HIST,seqLog,eventoLog));
  assert(memcmp(dadosSeguranca,EEPROM.data,32)==0);
  uint32_t anteriorLog=ultimaSeqHist;
  carregarHistorico(); assert(totalHist==MAX_HIST && ultimaSeqHist==anteriorLog);
  // Uma escrita interrompida perde somente o slot em gravacao.
  byte proximo=(ultimoSlotHist+1)%MAX_HIST;
  EEPROM.update(ADDR_HIST+proximo*TAM_REG+7,0);
  carregarHistorico(); assert(totalHist==MAX_HIST-1 && ultimaSeqHist==anteriorLog);
  registrarEvento(EV_BLOQUEIO);
  assert(totalHist==MAX_HIST && ultimaSeqHist==anteriorLog+1);
  assert(consultarHistorico(0,seqLog,eventoLog) && eventoLog==EV_BLOQUEIO);
  EEPROM.update(ADDR_HIST+ultimoSlotHist*TAM_REG+5, EEPROM.read(ADDR_HIST+ultimoSlotHist*TAM_REG+5)^1);
  carregarHistorico(); assert(totalHist==MAX_HIST-1 && ultimaSeqHist==anteriorLog);

  // Um reinício normal preserva até códigos armazenados por uma versão anterior.
  const char chaveAnterior[16] = {'9','8','7','6','5','4','3','2','1','0','9','8','7','6','5','4'};
  EEPROM.put(ADDR_CHAVE, chaveAnterior);
  restart();
  assert(WDTCSR==0 && confirmacoesChave==0 && memcmp(codigoRec,chaveAnterior,16)==0);

  // EEPROM vazia: sem amostras ou com todos os dígitos iguais, não salve código.
  memset(EEPROM.data,255,sizeof(EEPROM.data));
  restart();
  runMs(TEMPO_GERACAO_MS);
  assert(WDTCSR==0 && EEPROM.read(ADDR_REC_MAGIC)==255);
  assert(strcmp(lcd.lines[0],"Falha na geracao")==0);
  enter("#");
  for (int n=0; n<16; ++n) simularDigito(5);
  assert(WDTCSR==0 && !chaveConfigurada && EEPROM.read(ADDR_REC_MAGIC)==255);
  enter("#");
  for (int n=0; n<16; ++n) simularDigito(n%2 ? 7 : 5);
  assert(WDTCSR==0 && EEPROM.read(ADDR_REC_MAGIC)==REC_PENDENTE);
  assert(strcmp(lcd.lines[0],"5757575757575757")==0);
}
'''

with tempfile.TemporaryDirectory(prefix="cofre-test-") as tmp:
    # Compile uma cópia isolada com as bibliotecas simuladas.
    folder = Path(tmp)
    (folder / "Wire.h").write_text(STUBS)
    for directory, header in [("avr", "interrupt.h"), ("avr", "wdt.h"), ("util", "atomic.h")]:
        (folder / directory).mkdir(exist_ok=True)
        (folder / directory / header).write_text('#include "Wire.h"\n')
    import re
    assert not re.search(r"\b(?:delay|delayMicroseconds|millis|micros|tone|noTone)\s*\(", SKETCH.read_text())
    for name in ["LiquidCrystal_I2C.h", "Servo.h", "EEPROM.h"]:
        (folder / name).write_text('#include "Wire.h"\n')
    test = folder / "test.cpp"
    (folder / SKETCH.name).write_bytes(SKETCH.read_bytes())
    test.write_text(f'#include "{SKETCH.name}"\n' + TESTS)
    binary = folder / "test"
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++11", "-Wall", "-Wextra",
                    "-Werror", "-I", tmp, str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("OK: geração do código, Timer2, teclado, PIN, EEPROM, servo, bipes, recuperação e histórico.")
