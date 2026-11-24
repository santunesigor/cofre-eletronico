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

// Historico circular: 120 registros de 8 bytes, enderecos 32..991.
// Nao sobrepoe PIN, estado da porta, chave ou contador de recuperacao.
const int ADDR_HIST = 32;
const byte MAX_HIST = 120, TAM_REG = 8, MARCA_REG = 0xA7;
enum Evento { EV_INICIO = 1, EV_ABERTURA, EV_FECHAMENTO, EV_PIN_ERRADO,
              EV_BLOQUEIO, EV_REC_ERRADA, EV_REC_OK, EV_SENHA_ALTERADA };
byte totalHist = 0, ultimoSlotHist = 0, posHist = 0;
uint32_t ultimaSeqHist = 0, ultimaAcaoHist = 0;
bool vendoHist = false, aberturaAutenticada = false;

// CRC-16/CCITT do numero sequencial e do tipo; marcador gravado por ultimo.
uint16_t crcRegistro(uint32_t seq, byte evento) {
  uint16_t crc = 0xffff;
  for (byte i = 0; i < 5; ++i) {
    byte valor = i < 4 ? byte(seq >> (8 * i)) : evento;
    crc ^= uint16_t(valor) << 8;
    for (byte bit = 0; bit < 8; ++bit)
      crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
  }
  return crc;
}

bool lerRegistro(byte slot, uint32_t &seq, byte &evento) {
  if (slot >= MAX_HIST) return false;
  int addr = ADDR_HIST + int(slot) * TAM_REG;
  if (EEPROM.read(addr + 7) != MARCA_REG) return false;
  seq = 0;
  for (byte i = 0; i < 4; ++i) seq |= uint32_t(EEPROM.read(addr + i)) << (8 * i);
  evento = EEPROM.read(addr + 4);
  uint16_t crc = uint16_t(EEPROM.read(addr + 5)) | (uint16_t(EEPROM.read(addr + 6)) << 8);
  return evento >= EV_INICIO && evento <= EV_SENHA_ALTERADA && crc == crcRegistro(seq, evento);
}

void carregarHistorico() {
  totalHist = 0;
  ultimaSeqHist = 0;
  ultimoSlotHist = 0;
  vendoHist = aberturaAutenticada = false;
  for (byte slot = 0; slot < MAX_HIST; ++slot) {
    uint32_t seq;
    byte evento;
    if (!lerRegistro(slot, seq, evento)) continue;
    if (!totalHist || (seq != ultimaSeqHist && uint32_t(seq - ultimaSeqHist) < 0x80000000UL)) {
      ultimaSeqHist = seq;
      ultimoSlotHist = slot;
    }
    ++totalHist;
  }
}

void registrarEvento(byte evento) {
  byte slot = totalHist ? (ultimoSlotHist + 1) % MAX_HIST : 0;
  uint32_t anterior;
  byte tipoAnterior;
  bool substitui = lerRegistro(slot, anterior, tipoAnterior);
  uint32_t seq = ultimaSeqHist + 1;
  int addr = ADDR_HIST + int(slot) * TAM_REG;
  uint16_t crc = crcRegistro(seq, evento);
  EEPROM.update(addr + 7, 0);
  for (byte i = 0; i < 4; ++i) EEPROM.update(addr + i, byte(seq >> (8 * i)));
  EEPROM.update(addr + 4, evento);
  EEPROM.update(addr + 5, byte(crc));
  EEPROM.update(addr + 6, byte(crc >> 8));
  EEPROM.update(addr + 7, MARCA_REG);
  ultimaSeqHist = seq;
  ultimoSlotHist = slot;
  if (!substitui && totalHist < MAX_HIST) ++totalHist;
}

// posicao 0 = mais recente; pula registros invalidos ou incompletos.
bool consultarHistorico(byte pos, uint32_t &seq, byte &evento) {
  if (pos >= totalHist) return false;
  for (byte i = 0; i < MAX_HIST; ++i) {
    byte slot = (int(ultimoSlotHist) + MAX_HIST - i) % MAX_HIST;
    if (lerRegistro(slot, seq, evento)) {
      if (pos == 0) return true;
      --pos;
    }
  }
  return false;
}

const char *nomeEvento(byte evento) {
  switch (evento) {
    case EV_INICIO: return "Reinicio";
    case EV_ABERTURA: return "Cofre aberto";
    case EV_FECHAMENTO: return "Cofre fechado";
    case EV_PIN_ERRADO: return "Senha incorreta";
    case EV_BLOQUEIO: return "Cofre bloqueado";
    case EV_REC_ERRADA: return "Contra. errada";
    case EV_REC_OK: return "Desbloqueado";
    case EV_SENHA_ALTERADA: return "Senha alterada";
    default: return "Desconhecido";
  }
}
void mostrarHistorico() {
  lcd.clear();
  uint32_t seq;
  byte evento;
  if (!consultarHistorico(posHist, seq, evento)) {
    lcd.print("Sem registros");
    return;
  }
  char titulo[17];
  snprintf(titulo, sizeof(titulo), "Reg. %03u/%03u", (unsigned int)(posHist + 1), (unsigned int)totalHist);
  lcd.print(titulo);
  lcd.setCursor(0, 1);
  lcd.print(nomeEvento(evento));
}


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
const uint32_t ROLAGEM_MS = 400UL, EXIBIR_ID_MS = 6000UL;
const char MSG_REC_1[] = "Digite a contrassenha";
const char MSG_REC_2[] = "Entre em contato com o fabricante";
const byte ULTIMO_PASSO_REC = sizeof(MSG_REC_2) - 1 - 16;
const uint32_t CICLO_MSG_MS = (ULTIMO_PASSO_REC + 1) * ROLAGEM_MS;
uint32_t inicioTelaRec = 0;
int8_t ultimoQuadroRec = -1;

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

// Escreve uma janela de 16 caracteres; frases menores param no final.
void mostrarLinhaRolante(byte linha, const char *msg, byte passo) {
  byte tamanho = strlen(msg);
  byte maximo = tamanho > 16 ? tamanho - 16 : 0;
  byte deslocamento = passo < maximo ? passo : maximo;
  lcd.setCursor(0, linha);
  for (byte i = 0; i < 16; ++i) {
    byte pos = deslocamento + i;
    lcd.print(pos < tamanho ? msg[pos] : ' ');
  }
}

// Alterna o aviso rolante com um ID estatico, legivel pelo scanner.
void atualizarTelaRec() {
  if (!bloqueado() || portaAberta || fase != LIVRE || qtdRec != 0) return;
  uint32_t decorrido = uint32_t(lerTimer() - inicioTelaRec) / TICKS_POR_MS;
  uint32_t pos = decorrido % (CICLO_MSG_MS + EXIBIR_ID_MS);
  int8_t quadro = pos < CICLO_MSG_MS ? int8_t(pos / ROLAGEM_MS) : -2;
  if (quadro == ultimoQuadroRec) return;
  ultimoQuadroRec = quadro;
  if (quadro == -2) {
    char id[17];
    snprintf(id, sizeof(id), "ID: %08lX%04X", (unsigned long)sessaoRec, (unsigned int)janelaRec);
    lcd.setCursor(0, 0);
    lcd.print(id);
    lcd.setCursor(0, 1);
    lcd.print("Escaneie o ID   ");
  } else {
    mostrarLinhaRolante(0, MSG_REC_1, byte(quadro));
    mostrarLinhaRolante(1, MSG_REC_2, byte(quadro));
  }
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
      if (qtdRec == 0) {
        inicioTelaRec = lerTimer();
        ultimoQuadroRec = -1;
        atualizarTelaRec();
        return;
      }
      lcd.print("Contrassenha:");
      lcd.setCursor(0, 1);
      lcd.print("PIN: ");
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
        registrarEvento(EV_REC_OK);
        modo = NORMAL;
        limparRec();
        limparEntrada(); // exige o PIN original; nao abre o servo
        return;
      }
      registrarEvento(EV_REC_ERRADA);
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
      aberturaAutenticada = true;
      registrarEvento(EV_ABERTURA);
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
      aberturaAutenticada = vendoHist = false;
      registrarEvento(EV_FECHAMENTO);
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
    registrarEvento(EV_SENHA_ALTERADA);
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
    registrarEvento(EV_PIN_ERRADO);
    if (bloqueado()) {
      registrarEvento(EV_BLOQUEIO);
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
    if (vendoHist) {
      if (tecla == TECLA_LIMPAR || tecla == TECLA_CONFIRMAR) {
        vendoHist = false;
        mostrarEntrada();
      } else if (tecla == '2' || tecla == '8') {
        if (tecla == '2' && posHist + 1 < totalHist) ++posHist;
        if (tecla == '8' && posHist > 0) --posHist;
        ultimaAcaoHist = lerTimer();
        mostrarHistorico();
      }
      return;
    }
    if (tecla == TECLA_CONFIRMAR) fecharPorta();
    else if (tecla == '0' && aberturaAutenticada) {
      vendoHist = true;
      posHist = 0;
      ultimaAcaoHist = lerTimer();
      mostrarHistorico();
    }
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
  carregarHistorico();
  registrarEvento(EV_INICIO);
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
  atualizarTelaRec();
  if (vendoHist && uint32_t(lerTimer() - ultimaAcaoHist) >= 30000UL * TICKS_POR_MS) {
    vendoHist = false;
    mostrarEntrada();
  }
}
