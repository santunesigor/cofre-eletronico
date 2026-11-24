/*
 * COFRE ELETRONICO - Arduino Uno / Wokwi
 * Componentes: LCD 16x2 I2C, microservo, buzzer e 12 botoes (matriz 4x3)
 * Senha inicial: 1234 | Confirmar valida ou fecha | Limpar apaga
 * Nao precisa da biblioteca Keypad: varredura da matriz feita no codigo.
 * Simulacao no Wokwi com as bibliotecas Servo e LiquidCrystal I2C.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <EEPROM.h>
#include <string.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// LCD 16x2 I2C: SDA=A4, SCL=A5 no Arduino Uno.
// 0x27 e o endereco padrao no Wokwi; confirme no LCD fisico.
const byte ENDERECO_LCD = 0x27;
LiquidCrystal_I2C lcd(ENDERECO_LCD, 16, 2);
Servo trava;

const byte PIN_SERVO = 9;
const byte PIN_BUZZER = A2;
const unsigned int FREQUENCIA_BEEP_ABERTURA_HZ = 4000;
// Timer2: interrupcao a cada 125 us; alternar A2 gera 4 kHz.
const uint32_t TICKS_POR_MS = 8;
volatile uint32_t ticksTimer = 0;
volatile bool buzzerLigado = false;

ISR(TIMER2_COMPA_vect) {
  ++ticksTimer;
  if (buzzerLigado) PINC = _BV(PC2); // A2: toggle atomico do pino.
}

uint32_t lerTimer() {
  uint32_t ticks;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { ticks = ticksTimer; }
  return ticks;
}

void configurarTimer() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    TCCR2B = 0;
    TCCR2A = _BV(WGM21); // CTC, sem PWM nos pinos do Timer2.
    TCNT2 = 0;
    OCR2A = F_CPU / 8UL / (2UL * FREQUENCIA_BEEP_ABERTURA_HZ) - 1;
    TIFR2 = _BV(OCF2A);
    TIMSK2 = _BV(OCIE2A);
    ticksTimer = 0;
    buzzerLigado = false;
    TCCR2B = _BV(CS21); // prescaler 8
  }
}

void controlarBuzzer(bool ligado) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    buzzerLigado = ligado;
    PORTC &= ~_BV(PC2);
  }
}
const unsigned long DURACAO_BEEP_ABERTURA_MS = 70;
const unsigned long INTERVALO_BEEPS_ABERTURA_MS = 50;
// Codigos internos dos botoes rotulados no circuito.
const char TECLA_LIMPAR = '*';
const char TECLA_CONFIRMAR = '#';
const char TECLA_TROCAR_SENHA = 'T';
const byte PIN_LINHAS[4] = {2, 3, 4, 5};
const byte PIN_COLUNAS[3] = {6, 7, 8};
const char TECLAS[4][3] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {TECLA_LIMPAR, '0', TECLA_CONFIRMAR}
};

char senha[5] = "1234";
enum Modo { NORMAL, VALIDAR_ATUAL, DEFINIR_NOVA };
Modo modo = NORMAL;
const unsigned long DEBOUNCE_MS = 40;
const unsigned long TEMPO_TROCA_MS = 5000;
// Calibre o neutro e o tempo com a trava desacoplada antes de montar.
const int SERVO_PARADO_US = 1500;
const int SERVO_ABRIR_US = 1600;
const int SERVO_FECHAR_US = 1400;
const unsigned long TEMPO_MOVIMENTO_MS = 500;

char digitado[5] = "";
byte quantidade = 0;
bool portaAberta = false;

// EEPROM: preserva os bytes da senha e do estado usados na v1.0.0.
const byte ADDR_REC_MAGIC = 6, ADDR_ERROS = 7, ADDR_SESSAO = 8, ADDR_CHAVE = 12;
const byte REC_MAGIC = 0xA5, MAX_ERROS = 3;
const uint32_t JANELA_MS = 60000UL, ESPERA_REC_MS = 30000UL;
byte erros = 0;
bool chaveConfigurada = false, confirmarChave = false;
char chaveRec[16];
char entradaRec[17] = "";
byte qtdRec = 0;
uint32_t sessaoRec = 0, inicioJanela = 0;
uint16_t janelaRec = 0;

// SipHash-2-4, chave de 128 bits, mensagem de oito bytes little-endian.
// A chave de recuperacao e escolhida pelo proprietario, nunca pelo repositorio.
uint64_t rotl64(uint64_t v, byte n) { return (v << n) | (v >> (64 - n)); }
uint64_t ler64(const char *p) {
  uint64_t v = 0;
  for (byte i = 0; i < 8; ++i) v |= uint64_t(uint8_t(p[i])) << (8 * i);
  return v;
}
void sipRound(uint64_t &v0, uint64_t &v1, uint64_t &v2, uint64_t &v3) {
  v0 += v1; v1 = rotl64(v1, 13); v1 ^= v0; v0 = rotl64(v0, 32);
  v2 += v3; v3 = rotl64(v3, 16); v3 ^= v2;
  v0 += v3; v3 = rotl64(v3, 21); v3 ^= v0;
  v2 += v1; v1 = rotl64(v1, 17); v1 ^= v2; v2 = rotl64(v2, 32);
}
uint64_t sipHash8(const char *key, uint64_t msg) {
  uint64_t k0 = ler64(key), k1 = ler64(key + 8);
  uint64_t v0 = 0x736f6d6570736575ULL ^ k0, v1 = 0x646f72616e646f6dULL ^ k1;
  uint64_t v2 = 0x6c7967656e657261ULL ^ k0, v3 = 0x7465646279746573ULL ^ k1;
  v3 ^= msg;
  for (byte i = 0; i < 2; ++i) sipRound(v0, v1, v2, v3);
  v0 ^= msg;
  const uint64_t b = 8ULL << 56;
  v3 ^= b;
  for (byte i = 0; i < 2; ++i) sipRound(v0, v1, v2, v3);
  v0 ^= b; v2 ^= 0xff;
  for (byte i = 0; i < 4; ++i) sipRound(v0, v1, v2, v3);
  return v0 ^ v1 ^ v2 ^ v3;
}
uint32_t codigoRec() {
  return uint32_t(sipHash8(chaveRec, (uint64_t(sessaoRec) << 16) | janelaRec) % 1000000ULL);
}
void limparRec() { qtdRec = 0; entradaRec[0] = 0; }
void salvarErros(byte valor) {
  erros = valor;
  EEPROM.update(ADDR_ERROS, erros);
}
void novaSessao() {
  ++sessaoRec;
  EEPROM.put(ADDR_SESSAO, sessaoRec);
  janelaRec = 0;
  inicioJanela = lerTimer();
  limparRec();
}
bool bloqueado() { return chaveConfigurada && erros >= MAX_ERROS; }


char leituraAnterior = '\0';
char teclaEstavel = '\0';
uint32_t ultimaMudanca = 0;
uint32_t inicioTroca = 0;
bool aguardarSoltar = false;
bool trocaDisparada = false;
byte linhaAtiva = 0;
uint32_t inicioLinha = 0;
unsigned int pressionadas = 0;
char teclaLida = '\0';

enum Fase { LIVRE, INICIANDO, ABRINDO, NEUTRO_ABERTURA, BEEP_1,
            INTERVALO_BEEPS, BEEP_2, FECHANDO, NEUTRO_FECHAMENTO,
            AVISO_LIMPAR, AVISO_PRESERVAR, ERRO_SONORO, ERRO_SILENCIOSO, ESPERA_REC };
Fase fase = LIVRE;
uint32_t inicioFase = 0;
uint32_t duracaoFase = 0;

void iniciarFase(Fase nova, uint32_t duracaoMs);
void iniciarFase(Fase nova, uint32_t duracaoMs) {
  fase = nova;
  inicioFase = lerTimer();
  duracaoFase = duracaoMs * TICKS_POR_MS;
}

void mostrarEntrada() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (portaAberta) {
    lcd.print("Para fechar:");
    lcd.setCursor(0, 1);
    lcd.print("Aperte Confirmar");
    return;
  }
  if (!chaveConfigurada || bloqueado()) {
    if (!chaveConfigurada) {
      lcd.print(confirmarChave ? "Repita a chave" : "Chave 16 digitos");
      lcd.setCursor(0, 1);
    } else {
      char id[17];
      snprintf(id, sizeof(id), "ID: %08lX%04X", (unsigned long)sessaoRec, (unsigned int)janelaRec);
      lcd.print(id);
      lcd.setCursor(0, 1);
      lcd.print("Codigo: ");
    }
    for (byte i = 0; i < qtdRec; ++i) lcd.print('*');
    return;
  }
  if (modo == VALIDAR_ATUAL) lcd.print("Senha atual:");
  else if (modo == DEFINIR_NOVA) lcd.print("Nova senha:");
  else lcd.print("Digite a senha");
  lcd.setCursor(0, 1);
  lcd.print("PIN: ");
  for (byte i = 0; i < quantidade; i++) {
    lcd.print('*');
  }
}

void limparEntrada() {
  quantidade = 0;
  digitado[0] = '\0';
  mostrarEntrada();
}

// A cada minuto, a janela muda e a entrada anterior e descartada.
void atualizarDesafio() {
  if (!bloqueado() || portaAberta || fase != LIVRE) return;
  if (uint32_t(lerTimer() - inicioJanela) >= JANELA_MS * TICKS_POR_MS) {
    if (janelaRec == 0xffff) novaSessao();
    else {
      ++janelaRec;
      inicioJanela = lerTimer();
      limparRec();
    }
    mostrarEntrada();
  }
}

void esperarRecuperacao() {
  limparRec();
  lcd.clear();
  lcd.print("Bloqueado 30s");
  iniciarFase(ESPERA_REC, ESPERA_REC_MS);
}

void tratarRec(char tecla) {
  byte limite = chaveConfigurada ? 6 : 16;
  if (tecla == TECLA_LIMPAR) {
    limparRec();
  } else if (tecla == TECLA_CONFIRMAR && qtdRec == limite) {
    if (!chaveConfigurada) {
      if (!confirmarChave) {
        memcpy(chaveRec, entradaRec, sizeof(chaveRec));
        confirmarChave = true;
      } else if (memcmp(chaveRec, entradaRec, sizeof(chaveRec)) == 0) {
        EEPROM.put(ADDR_CHAVE, chaveRec);
        sessaoRec = 0;
        EEPROM.put(ADDR_SESSAO, sessaoRec);
        salvarErros(0);
        EEPROM.update(ADDR_REC_MAGIC, REC_MAGIC); // marcador por ultimo
        chaveConfigurada = true;
        confirmarChave = false;
      } else {
        confirmarChave = false;
      }
      limparRec();
    } else {
      uint32_t informado = 0;
      for (byte i = 0; i < 6; ++i) informado = informado * 10 + (entradaRec[i] - '0');
      if (informado == codigoRec()) {
        salvarErros(0);
        modo = NORMAL;
        limparRec();
        limparEntrada(); // exige o PIN original; nao abre o servo
        return;
      }
      esperarRecuperacao();
      return;
    }
  } else if (tecla >= '0' && tecla <= '9' && qtdRec < limite) {
    entradaRec[qtdRec++] = tecla;
    entradaRec[qtdRec] = 0;
  }
  mostrarEntrada();
}

// Varredura cooperativa: uma linha por vez, sem espera bloqueante.
void ativarLinha() {
  digitalWrite(PIN_LINHAS[linhaAtiva], LOW);
  pinMode(PIN_LINHAS[linhaAtiva], OUTPUT);
  inicioLinha = lerTimer();
}

// Retorna true somente ao completar a leitura das quatro linhas.
bool lerTeclado(char &tecla) {
  // Dois ticks garantem pelo menos 125 us de estabilizacao.
  if (uint32_t(lerTimer() - inicioLinha) < 2) return false;
  for (byte coluna = 0; coluna < 3; coluna++) {
    if (digitalRead(PIN_COLUNAS[coluna]) == LOW) {
      pressionadas |= 1U << (linhaAtiva * 3 + coluna);
      teclaLida = TECLAS[linhaAtiva][coluna];
    }
  }
  pinMode(PIN_LINHAS[linhaAtiva], INPUT);
  linhaAtiva = (linhaAtiva + 1) % 4;
  bool completa = linhaAtiva == 0;
  if (completa) {
    tecla = '\0';
    if (pressionadas == ((1U << 9) | (1U << 11))) tecla = TECLA_TROCAR_SENHA;
    else if (pressionadas && !(pressionadas & (pressionadas - 1))) tecla = teclaLida;
    pressionadas = 0;
    teclaLida = '\0';
  }
  ativarLinha();
  return completa;
}

// Debounce e deteccao da combinacao de 5 segundos pelo Timer2.
char lerPressionamento() {
  char leitura;
  if (!lerTeclado(leitura)) return '\0';
  uint32_t agora = lerTimer();
  if (leitura != leituraAnterior) {
    leituraAnterior = leitura;
    ultimaMudanca = agora;
  }
  if (uint32_t(agora - ultimaMudanca) >= DEBOUNCE_MS * TICKS_POR_MS && leitura != teclaEstavel) {
    teclaEstavel = leitura;
    if (teclaEstavel == '\0') {
      aguardarSoltar = false;
      trocaDisparada = false;
    } else if (teclaEstavel == TECLA_TROCAR_SENHA) {
      aguardarSoltar = true;
      inicioTroca = agora;
    } else if (!aguardarSoltar) {
      return teclaEstavel;
    }
  }
  if (leitura != TECLA_TROCAR_SENHA) inicioTroca = agora;
  if (teclaEstavel == TECLA_TROCAR_SENHA && leitura == TECLA_TROCAR_SENHA &&
      !trocaDisparada && uint32_t(agora - inicioTroca) >= TEMPO_TROCA_MS * TICKS_POR_MS) {
    trocaDisparada = true;
    return TECLA_TROCAR_SENHA;
  }
  return '\0';
}

void pararServo() {
  trava.writeMicroseconds(SERVO_PARADO_US);
}

void abrirPorta() {
  // Persiste aberto antes do movimento, como medida conservadora.
  EEPROM.update(sizeof(senha), 1);
  trava.writeMicroseconds(SERVO_ABRIR_US);
  iniciarFase(ABRINDO, TEMPO_MOVIMENTO_MS);
}

void fecharPorta() {
  trava.writeMicroseconds(SERVO_FECHAR_US);
  iniciarFase(FECHANDO, TEMPO_MOVIMENTO_MS);
}

// Maquina de estados: cada chamada executa no maximo uma transicao.
void atualizarFase() {
  if (fase == LIVRE || uint32_t(lerTimer() - inicioFase) < duracaoFase) return;
  switch (fase) {
    case INICIANDO:
      fase = LIVRE;
      if (bloqueado() && !portaAberta) esperarRecuperacao();
      else mostrarEntrada();
      break;
    case ABRINDO:
      pararServo();
      iniciarFase(NEUTRO_ABERTURA, 100);
      break;
    case NEUTRO_ABERTURA:
      portaAberta = true;
      controlarBuzzer(true);
      iniciarFase(BEEP_1, DURACAO_BEEP_ABERTURA_MS);
      break;
    case BEEP_1:
      controlarBuzzer(false);
      iniciarFase(INTERVALO_BEEPS, INTERVALO_BEEPS_ABERTURA_MS);
      break;
    case INTERVALO_BEEPS:
      controlarBuzzer(true);
      iniciarFase(BEEP_2, DURACAO_BEEP_ABERTURA_MS);
      break;
    case BEEP_2:
      controlarBuzzer(false);
      fase = LIVRE;
      mostrarEntrada();
      break;
    case FECHANDO:
      pararServo();
      iniciarFase(NEUTRO_FECHAMENTO, 100);
      break;
    case NEUTRO_FECHAMENTO:
      EEPROM.update(sizeof(senha), 0);
      portaAberta = false;
      lcd.clear();
      lcd.print("Cofre fechado");
      iniciarFase(AVISO_LIMPAR, 800);
      break;
    case ERRO_SONORO:
      controlarBuzzer(false);
      iniciarFase(ERRO_SILENCIOSO, 200);
      break;
    case ERRO_SILENCIOSO:
    case AVISO_LIMPAR:
      fase = LIVRE;
      limparEntrada();
      break;
    case AVISO_PRESERVAR:
      fase = LIVRE;
      mostrarEntrada();
      break;
    case ESPERA_REC:
      novaSessao();
      fase = LIVRE;
      mostrarEntrada();
      break;
    case LIVRE:
      break;
  }
}

void confirmarSenha() {
  if (quantidade != 4) {
    lcd.clear();
    lcd.print("Digite 4 numeros");
    iniciarFase(AVISO_PRESERVAR, 900);
    return;
  }
  if (modo == DEFINIR_NOVA) {
    strcpy(senha, digitado);
    EEPROM.put(0, senha);
    modo = NORMAL;
    lcd.clear();
    lcd.print("Senha alterada!");
    iniciarFase(AVISO_LIMPAR, 1200);
  } else if (strcmp(digitado, senha) == 0) {
    salvarErros(0);
    if (modo == VALIDAR_ATUAL) {
      modo = DEFINIR_NOVA;
      limparEntrada();
      return;
    }
    quantidade = 0;
    digitado[0] = '\0';
    abrirPorta();
  } else {
    if (erros < MAX_ERROS) salvarErros(erros + 1);
    if (bloqueado()) {
      modo = NORMAL;
      novaSessao();
    }
    lcd.clear();
    lcd.print("Senha incorreta");
    lcd.setCursor(0, 1);
    lcd.print("Tente de novo");
    controlarBuzzer(true);
    iniciarFase(ERRO_SONORO, 1500);
  }
}

void tratarTecla(char tecla) {
  if (fase != LIVRE) return;
  if (!portaAberta && (!chaveConfigurada || bloqueado())) {
    tratarRec(tecla);
    return;
  }
  if (tecla == TECLA_TROCAR_SENHA) {
    if (!portaAberta && modo == NORMAL) {
      modo = VALIDAR_ATUAL;
      limparEntrada();
    }
    return;
  }
  if (portaAberta) {
    if (tecla == TECLA_CONFIRMAR) fecharPorta();
    return;
  }

  if (tecla == TECLA_LIMPAR) {
    limparEntrada();
  } else if (tecla == TECLA_CONFIRMAR) {
    confirmarSenha();
  } else if (tecla >= '0' && tecla <= '9' && quantidade < 4) {
    digitado[quantidade++] = tecla;
    digitado[quantidade] = '\0';
    mostrarEntrada();
  }
}

void setup() {
  char salva[5];
  EEPROM.get(0, salva);
  bool valida = salva[4] == '\0';
  for (byte i = 0; i < 4; i++) {
    if (salva[i] < '0' || salva[i] > '9') valida = false;
  }
  if (valida) strcpy(senha, salva);
  portaAberta = EEPROM.read(sizeof(senha)) == 1;
  chaveConfigurada = EEPROM.read(ADDR_REC_MAGIC) == REC_MAGIC;
  confirmarChave = false;
  limparRec();
  erros = 0;
  sessaoRec = 0;
  janelaRec = 0;
  if (chaveConfigurada) {
    EEPROM.get(ADDR_CHAVE, chaveRec);
    EEPROM.get(ADDR_SESSAO, sessaoRec);
    erros = EEPROM.read(ADDR_ERROS);
    if (erros > MAX_ERROS) erros = MAX_ERROS;
  }
  for (byte i = 0; i < 4; i++) pinMode(PIN_LINHAS[i], INPUT);
  for (byte i = 0; i < 3; i++) pinMode(PIN_COLUNAS[i], INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  controlarBuzzer(false);

  trava.writeMicroseconds(SERVO_PARADO_US);
  trava.attach(PIN_SERVO);
  pararServo();
  lcd.init();
  lcd.backlight();
  configurarTimer();
  linhaAtiva = 0;
  pressionadas = 0;
  teclaLida = '\0';
  leituraAnterior = teclaEstavel = '\0';
  ultimaMudanca = inicioTroca = 0;
  aguardarSoltar = trocaDisparada = false;
  ativarLinha();
  iniciarFase(INICIANDO, 100);
}

void loop() {
  atualizarDesafio();
  char tecla = lerPressionamento();
  if (tecla != '\0' && fase == LIVRE) tratarTecla(tecla);
  atualizarFase();
}
