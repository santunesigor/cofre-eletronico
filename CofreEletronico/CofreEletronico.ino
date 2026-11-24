/*
 * COFRE ELETRONICO - Arduino Uno / Wokwi
 * Componentes: LCD 16x2 I2C, microservo, buzzer e 12 botoes (matriz 4x3)
 * Primeiro uso: anote o codigo de recuperacao, confirme 3 vezes e escolha o PIN.
 * Confirmar valida ou fecha; Limpar apaga a entrada. 1234 e o PIN legado.
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
#include <avr/wdt.h>
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
// volatile avisa ao compilador que a interrupção e o loop compartilham os valores.
volatile uint32_t ticksTimer = 0;
volatile bool buzzerLigado = false;

ISR(TIMER2_COMPA_vect) {
  // A interrupção só marca tempo e gera o sinal. LCD e EEPROM ficam no loop.
  ++ticksTimer;
  if (buzzerLigado) PINC = _BV(PC2); // A2: toggle atomico do pino.
}

uint32_t lerTimer() {
  uint32_t ticks;
  // O Uno tem CPU de 8 bits: uma leitura de 32 bits pode cruzar uma interrupção.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { ticks = ticksTimer; }
  return ticks;
}

void configurarTimer() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    TCCR2B = 0; // Pare o relógio enquanto configura o comparador.
    TCCR2A = _BV(WGM21); // CTC, sem PWM nos pinos do Timer2.
    TCNT2 = 0;
    // Divisor 8 e 8.000 comparações/s: o Uno conta de zero a 249.
    OCR2A = F_CPU / 8UL / (2UL * FREQUENCIA_BEEP_ABERTURA_HZ) - 1;
    TIFR2 = _BV(OCF2A); // Limpa uma indicação de comparação pendente.
    TIMSK2 = _BV(OCIE2A);
    ticksTimer = 0;
    buzzerLigado = false;
    TCCR2B = _BV(CS21); // prescaler 8
  }
}

void controlarBuzzer(bool ligado) {
  // Atualize a flag e leve A2 a nível baixo sem deixar a ISR alternar o pino entre as duas ações.
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
// Cada posição da matriz corresponde ao cruzamento dos pinos de linha e coluna.
const byte PIN_LINHAS[4] = {2, 3, 4, 5};
const byte PIN_COLUNAS[3] = {6, 7, 8};
const char TECLAS[4][3] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {TECLA_LIMPAR, '0', TECLA_CONFIRMAR}
};

// O PIN é texto para preservar zeros à esquerda. O último byte encerra a string.
const int ADDR_PIN = 0;
const int ADDR_PORTA = 5;
char senha[5] = "1234";
// Modo identifica a operação de PIN. Fase, mais abaixo, controla os tempos.
enum Modo { NORMAL, VALIDAR_ATUAL, DEFINIR_NOVA, DEFINIR_INICIAL, DEFINIR_RECUPERACAO };
Modo modo = NORMAL;
const unsigned long DEBOUNCE_MS = 40;
const unsigned long TEMPO_TROCA_MS = 5000;
// Calibre o neutro e o tempo com a trava desacoplada antes de montar.
const int SERVO_PARADO_US = 1500;
const int SERVO_ABRIR_US = 1600;
const int SERVO_FECHAR_US = 1400;
const unsigned long TEMPO_MOVIMENTO_MS = 500;

const byte TAM_PIN = 4;
const byte TAM_CODIGO_REC = 16;
// PIN e código usam o mesmo teclado. O buffer comporta a maior entrada mais o terminador.
char digitado[TAM_CODIGO_REC + 1] = "";
// Preserve a primeira proposta do PIN somente em RAM até a segunda entrada coincidir.
char pinEmConfirmacao[TAM_PIN + 1] = "";
bool confirmandoPin = false;
byte quantidade = 0;
bool portaAberta = false;

// Historico circular: 120 registros de 8 bytes, enderecos 32..991.
// Nao sobrepoe PIN, estado da porta ou código de recuperação.
const int ADDR_HIST = 32;
const byte MAX_HIST = 120, TAM_REG = 8, MARCA_REG = 0xA7;
// Os números dos eventos ficam na EEPROM. Mantenha a ordem para ler registros antigos.
enum Evento { EV_INICIO = 1, EV_ABERTURA, EV_FECHAMENTO, EV_PIN_ERRADO,
              EV_BLOQUEIO, EV_REC_ERRADA, EV_REC_OK, EV_SENHA_ALTERADA };
byte totalHist = 0, ultimoSlotHist = 0, posHist = 0;
uint32_t ultimaSeqHist = 0, ultimaAcaoHist = 0;
bool vendoHist = false, aberturaAutenticada = false;

// CRC-16/CCITT do numero sequencial e do tipo; marcador gravado por ultimo.
uint16_t crcRegistro(uint32_t seq, byte evento) {
  uint16_t crc = 0xffff;
  for (byte i = 0; i < 5; ++i) {
    // A sequência de 32 bits é lida do byte menos significativo ao mais significativo.
    byte valor = i < 4 ? byte(seq >> (8 * i)) : evento;
    crc ^= uint16_t(valor) << 8;
    for (byte bit = 0; bit < 8; ++bit)
      crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
  }
  return crc;
}

bool lerRegistro(byte slot, uint32_t &seq, byte &evento) {
  // Formato de 8 bytes: sequência [0–3], tipo [4], CRC [5–6], marcador [7].
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
  // Reconstrua o índice em RAM a partir dos slots válidos depois de ligar.
  totalHist = 0;
  ultimaSeqHist = 0;
  ultimoSlotHist = 0;
  vendoHist = aberturaAutenticada = false;
  for (byte slot = 0; slot < MAX_HIST; ++slot) {
    uint32_t seq;
    byte evento;
    if (!lerRegistro(slot, seq, evento)) continue;
    // A diferença sem sinal mantém a ordem quando a sequência volta a zero.
    if (!totalHist || (seq != ultimaSeqHist && uint32_t(seq - ultimaSeqHist) < 0x80000000UL)) {
      ultimaSeqHist = seq;
      ultimoSlotHist = slot;
    }
    ++totalHist;
  }
}

void registrarEvento(byte evento) {
  // Ao chegar ao fim da área do histórico, a próxima posição volta a ser a primeira.
  byte slot = totalHist ? (ultimoSlotHist + 1) % MAX_HIST : 0;
  uint32_t anterior;
  byte tipoAnterior;
  bool substitui = lerRegistro(slot, anterior, tipoAnterior);
  uint32_t seq = ultimaSeqHist + 1;
  int addr = ADDR_HIST + int(slot) * TAM_REG;
  uint16_t crc = crcRegistro(seq, evento);
  EEPROM.update(addr + 7, 0); // Invalida o slot antes de sobrescrevê-lo.
  for (byte i = 0; i < 4; ++i) EEPROM.update(addr + i, byte(seq >> (8 * i)));
  EEPROM.update(addr + 4, evento);
  EEPROM.update(addr + 5, byte(crc));
  EEPROM.update(addr + 6, byte(crc >> 8));
  EEPROM.update(addr + 7, MARCA_REG); // Publica o registro com os dados e o CRC completos.
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
    case EV_REC_ERRADA: return "Codigo incorreto";
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
  // A contagem na tela começa em 1. O índice interno começa em 0.
  snprintf(titulo, sizeof(titulo), "Reg. %03u/%03u", (unsigned int)(posHist + 1), (unsigned int)totalHist);
  lcd.print(titulo);
  lcd.setCursor(0, 1);
  lcd.print(nomeEvento(evento));
}


// EEPROM: preserva os bytes da senha e do estado usados na v1.0.0.
// Os bytes 8 a 11 ficam reservados para manter o código no endereço original.
const byte ADDR_REC_MAGIC = 6, ADDR_ERROS = 7, ADDR_CHAVE = 12;
// Primeiro uso persistente: REC_PENDENTE → PIN_PENDENTE → REC_MAGIC.
const byte REC_MAGIC = 0xA5, REC_PENDENTE = 0xA6, PIN_PENDENTE = 0xA7;
const byte MAX_ERROS = 3;
const uint32_t ESPERA_REC_MS = 30000UL;
byte erros = 0;
bool chaveConfigurada = false;
byte confirmacoesChave = 0;
char codigoRec[TAM_CODIGO_REC]; // Tamanho fixo, sem byte terminador.

void salvarErros(byte valor) {
  // Persistir a contagem impede que desligar a placa elimine tentativas anteriores.
  erros = valor;
  EEPROM.update(ADDR_ERROS, erros);
}
void carregarChave() {
  byte estado = EEPROM.read(ADDR_REC_MAGIC);
  chaveConfigurada = estado == REC_MAGIC || estado == REC_PENDENTE || estado == PIN_PENDENTE;
  confirmacoesChave = estado == REC_PENDENTE ? 3 : 0;
  // Se o usuário ocultou o código antes de escolher o PIN, retome direto no cadastro.
  if (estado == PIN_PENDENTE) modo = DEFINIR_INICIAL;
  if (chaveConfigurada) {
    EEPROM.get(ADDR_CHAVE, codigoRec);
    erros = EEPROM.read(ADDR_ERROS);
    if (erros > MAX_ERROS) erros = MAX_ERROS;
  }
}
bool bloqueado() { return chaveConfigurada && erros >= MAX_ERROS; }

bool solicitandoCodigo() {
  // Os erros só são zerados ao salvar o novo PIN, inclusive após reiniciar.
  return bloqueado() && modo != DEFINIR_RECUPERACAO;
}

// Estado da varredura e do debounce. '\0' representa ausência de tecla reconhecida.
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

enum Fase { LIVRE, INICIANDO, GERANDO_CODIGO, ABRINDO, NEUTRO_ABERTURA, BEEP_1,
            INTERVALO_BEEPS, BEEP_2, FECHANDO, NEUTRO_FECHAMENTO,
            AVISO_LIMPAR, AVISO_PRESERVAR, ERRO_SONORO, ERRO_SILENCIOSO, ESPERA_REC };
Fase fase = LIVRE;
uint32_t inicioFase = 0;
uint32_t duracaoFase = 0;

// O protótipo explícito impede o Arduino de gerá-lo antes da declaração de Fase.
void iniciarFase(Fase nova, uint32_t duracaoMs);
void iniciarFase(Fase nova, uint32_t duracaoMs) {
  fase = nova;
  inicioFase = lerTimer();
  duracaoFase = duracaoMs * TICKS_POR_MS;
}

void mostrarEntrada() {
  // A tela segue a prioridade da operação: porta aberta, bloqueio, primeiro uso ou PIN.
  lcd.clear();
  lcd.setCursor(0, 0);
  if (portaAberta) {
    lcd.print("Para fechar:");
    lcd.setCursor(0, 1);
    lcd.print("Aperte Confirmar");
    return;
  }
  if (!chaveConfigurada) {
    lcd.print("Falha na geracao");
    lcd.setCursor(0, 1);
    lcd.print("Tente: Confirmar");
    return;
  }
  if (solicitandoCodigo()) {
    lcd.print("Cod. recuperacao");
    lcd.setCursor(0, 1);
    // Sem prefixo: os 16 asteriscos ocupam uma linha inteira.
    for (byte i = 0; i < quantidade; ++i) lcd.print('*');
    return;
  }
  if (confirmacoesChave != 0) {
    for (char digito : codigoRec) lcd.print(digito);
    lcd.setCursor(0, 1);
    lcd.print("Confirmar: ");
    lcd.print(char('0' + confirmacoesChave));
    lcd.print('x');
    return;
  }
  if (confirmandoPin) lcd.print("Confirme seu PIN");
  else if (modo == VALIDAR_ATUAL) lcd.print("Senha atual:");
  else if (modo == DEFINIR_NOVA || modo == DEFINIR_RECUPERACAO) lcd.print("Novo PIN:");
  else if (modo == DEFINIR_INICIAL) lcd.print("Defina seu PIN");
  else lcd.print("Digite a senha");
  lcd.setCursor(0, 1);
  lcd.print("PIN: ");
  for (byte i = 0; i < quantidade; i++) {
    lcd.print('*');
  }
}

// O watchdog tem um oscilador próprio. Leia o bit baixo do Timer2, que usa
// outro relógio, para aproveitar a variação de fase entre os dois.
volatile byte amostraWatchdog = 0;
volatile bool novaAmostraWatchdog = false;
ISR(WDT_vect) {
  amostraWatchdog = TCNT2 & 1;
  novaAmostraWatchdog = true;
}

const uint32_t TEMPO_GERACAO_MS = 30000UL;
const byte SEM_AMOSTRA = 2;
byte primeiraAmostra = SEM_AMOSTRA;
byte digitosGerados = 0, bitsColetados = 0, valorColetado = 0;
bool digitosDiferentes = false;

void iniciarGeracaoCodigo() {
  lcd.clear();
  lcd.print("Gerando codigo");
  primeiraAmostra = SEM_AMOSTRA;
  digitosGerados = bitsColetados = valorColetado = 0;
  digitosDiferentes = false;
  novaAmostraWatchdog = false;
  iniciarFase(GERANDO_CODIGO, TEMPO_GERACAO_MS);
  // A configuração padrão de wdt_disable deixa o prescaler em zero.
  // Ative só a interrupção (cerca de 16 ms), sem habilitar reset automático.
  WDTCSR = _BV(WDIE) | _BV(WDIF);
}

void atualizarGeracaoCodigo() {
  if (fase != GERANDO_CODIGO) return;
  byte amostra = SEM_AMOSTRA;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if (novaAmostraWatchdog) {
      amostra = amostraWatchdog;
      novaAmostraWatchdog = false;
    }
  }
  if (amostra == SEM_AMOSTRA) return;
  if (primeiraAmostra == SEM_AMOSTRA) {
    primeiraAmostra = amostra;
    return;
  }
  // Filtro de Von Neumann: descarte 00 e 11. Os pares 01 e 10 viram 0 e 1.
  byte bit = primeiraAmostra;
  primeiraAmostra = SEM_AMOSTRA;
  if (bit == amostra) return;
  valorColetado = (valorColetado << 1) | bit;
  if (++bitsColetados < 4) return;
  // Use 0 a 9 e descarte 10 a 15 para não favorecer alguns dígitos.
  byte digito = valorColetado;
  bitsColetados = valorColetado = 0;
  if (digito >= 10) return;
  char numero = '0' + digito;
  if (digitosGerados != 0 && numero != codigoRec[0]) digitosDiferentes = true;
  codigoRec[digitosGerados++] = numero;
  if (digitosGerados < TAM_CODIGO_REC) return;

  wdt_disable();
  fase = LIVRE;
  // Rejeite um código de dígito único. Isso não certifica a qualidade da fonte.
  if (digitosDiferentes) {
    EEPROM.put(ADDR_CHAVE, codigoRec);
    salvarErros(0);
    EEPROM.update(ADDR_REC_MAGIC, REC_PENDENTE); // Marcador por último.
    chaveConfigurada = true;
    confirmacoesChave = 3;
  }
  mostrarEntrada();
}

void zerarEntrada() {
  // Os bytes antigos ficam fora da string e não participam da próxima entrada.
  quantidade = 0;
  digitado[0] = '\0';
}

void limparEntrada() {
  zerarEntrada();
  mostrarEntrada();
}

void esperarRecuperacao() {
  // A fase bloqueia novas teclas por 30 s sem interromper a varredura do teclado.
  zerarEntrada();
  lcd.clear();
  lcd.print("Bloqueado 30s");
  iniciarFase(ESPERA_REC, ESPERA_REC_MS);
}

void confirmarCodigo() {
  // Só um código completo conta como tentativa. Se faltarem dígitos, preserve a entrada.
  if (quantidade != TAM_CODIGO_REC) {
    lcd.clear();
    lcd.print("Digite 16 dig.");
    iniciarFase(AVISO_PRESERVAR, 900);
    return;
  }
  if (memcmp(digitado, codigoRec, TAM_CODIGO_REC) != 0) {
    registrarEvento(EV_REC_ERRADA);
    esperarRecuperacao();
    return;
  }
  // Autoriza escolher o novo PIN sem zerar os erros. A trava continua fechada.
  modo = DEFINIR_RECUPERACAO;
  limparEntrada();
}

// Varredura cooperativa: uma linha por vez, sem espera bloqueante.
void ativarLinha() {
  // Com as colunas em pull-up, uma tecla desta linha faz sua coluna ler LOW.
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
      // Um bit por tecla permite testar se houve uma única pressão ou uma combinação.
      pressionadas |= 1U << (linhaAtiva * 3 + coluna);
      teclaLida = TECLAS[linhaAtiva][coluna];
    }
  }
  pinMode(PIN_LINHAS[linhaAtiva], INPUT); // Desativa a linha antes de acionar a próxima.
  linhaAtiva = (linhaAtiva + 1) % 4;
  bool completa = linhaAtiva == 0;
  if (completa) {
    tecla = '\0';
    // Bits 9 e 11 são Limpar e Confirmar. Fora a combinação, aceite só um bit.
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
    // Reinicie os 40 ms sempre que o contato oscilar.
    leituraAnterior = leitura;
    ultimaMudanca = agora;
  }
  if (uint32_t(agora - ultimaMudanca) >= DEBOUNCE_MS * TICKS_POR_MS && leitura != teclaEstavel) {
    teclaEstavel = leitura;
    if (teclaEstavel == '\0') {
      aguardarSoltar = false;
      trocaDisparada = false;
    } else if (teclaEstavel == TECLA_TROCAR_SENHA) {
      // Soltar só um dos botões não deve enviar a outra tecla ao menu.
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
    // A combinação só dispara de novo após soltar e pressionar as duas teclas.
    return TECLA_TROCAR_SENHA;
  }
  return '\0';
}

void pararServo() {
  // Em rotação contínua, o pulso neutro manda parar. Ele não mede a posição.
  trava.writeMicroseconds(SERVO_PARADO_US);
}

void abrirPorta() {
  // Persiste aberto antes do movimento, como medida conservadora.
  EEPROM.update(ADDR_PORTA, 1);
  trava.writeMicroseconds(SERVO_ABRIR_US);
  iniciarFase(ABRINDO, TEMPO_MOVIMENTO_MS);
}

void fecharPorta() {
  // A EEPROM só receberá o estado fechado no final do movimento (em atualizarFase).
  trava.writeMicroseconds(SERVO_FECHAR_US);
  iniciarFase(FECHANDO, TEMPO_MOVIMENTO_MS);
}

// Maquina de estados: cada chamada executa no maximo uma transicao.
void atualizarFase() {
  // Compare o tempo decorrido, inclusive se o contador voltar a zero.
  if (fase == LIVRE || uint32_t(lerTimer() - inicioFase) < duracaoFase) return;
  switch (fase) {
    case INICIANDO:
      fase = LIVRE;
      if (!chaveConfigurada) iniciarGeracaoCodigo();
      else if (bloqueado() && !portaAberta) esperarRecuperacao();
      else mostrarEntrada();
      break;
    case GERANDO_CODIGO:
      wdt_disable();
      fase = LIVRE;
      mostrarEntrada();
      break;
    case ABRINDO:
      pararServo();
      iniciarFase(NEUTRO_ABERTURA, 100);
      break;
    case NEUTRO_ABERTURA:
      portaAberta = true;
      // Somente uma abertura autenticada nesta sessão libera o histórico.
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
      // Só marque fechado depois de terminar o comando de fechamento.
      EEPROM.update(ADDR_PORTA, 0);
      portaAberta = false;
      aberturaAutenticada = vendoHist = false;
      registrarEvento(EV_FECHAMENTO);
      lcd.clear();
      lcd.print("Cofre fechado");
      iniciarFase(AVISO_LIMPAR, 800);
      break;
    case ERRO_SONORO:
      // Desligue o som antes da pausa curta que mantém a mensagem visível.
      controlarBuzzer(false);
      iniciarFase(ERRO_SILENCIOSO, 200);
      break;
    case ERRO_SILENCIOSO:
    case AVISO_LIMPAR:
      fase = LIVRE;
      limparEntrada();
      break;
    case ESPERA_REC:
    case AVISO_PRESERVAR:
      // Retorne à tela do modo atual; a entrada parcial pode continuar ali.
      fase = LIVRE;
      mostrarEntrada();
      break;
    case LIVRE:
      break;
  }
}

void confirmarNovoPin() {
  // A primeira entrada fica em RAM. Só uma confirmação igual altera a EEPROM.
  if (!confirmandoPin) {
    memcpy(pinEmConfirmacao, digitado, sizeof(pinEmConfirmacao));
    confirmandoPin = true;
    limparEntrada();
    return;
  }
  confirmandoPin = false;
  if (memcmp(pinEmConfirmacao, digitado, sizeof(pinEmConfirmacao)) != 0) {
    // O modo continua sendo cadastro ou recuperação, mas é preciso propor o PIN de novo.
    lcd.clear();
    lcd.print("PINs diferentes");
    iniciarFase(AVISO_LIMPAR, 1200);
    return;
  }

  bool recuperando = modo == DEFINIR_RECUPERACAO;
  memcpy(senha, pinEmConfirmacao, sizeof(senha));
  EEPROM.put(ADDR_PIN, senha);
  if (modo == DEFINIR_INICIAL) EEPROM.update(ADDR_REC_MAGIC, REC_MAGIC);
  if (recuperando) salvarErros(0); // Libera o uso só depois de persistir o PIN.
  registrarEvento(recuperando ? EV_REC_OK : EV_SENHA_ALTERADA);
  modo = NORMAL;
  lcd.clear();
  lcd.print("PIN cadastrado!");
  iniciarFase(AVISO_LIMPAR, 1200);
}

void confirmarSenha() {
  if (quantidade != TAM_PIN) {
    lcd.clear();
    lcd.print("Digite 4 numeros");
    iniciarFase(AVISO_PRESERVAR, 900);
    return;
  }
  if (modo == DEFINIR_NOVA || modo == DEFINIR_INICIAL || modo == DEFINIR_RECUPERACAO) {
    confirmarNovoPin();
  } else if (strcmp(digitado, senha) == 0) {
    // Autenticação correta reinicia a contagem, inclusive ao entrar na troca do PIN.
    salvarErros(0);
    if (modo == VALIDAR_ATUAL) {
      modo = DEFINIR_NOVA;
      limparEntrada();
      return;
    }
    zerarEntrada();
    abrirPorta();
  } else {
    if (erros < MAX_ERROS) salvarErros(erros + 1);
    registrarEvento(EV_PIN_ERRADO);
    if (bloqueado()) {
      // Saia de qualquer menu de PIN quando atingir o limite de tentativas.
      registrarEvento(EV_BLOQUEIO);
      modo = NORMAL;
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
  // A mesma tecla tem funções diferentes no primeiro uso, bloqueio e histórico.
  if (fase != LIVRE) return;
  if (!portaAberta) {
    if (!chaveConfigurada) {
      if (tecla == TECLA_CONFIRMAR) iniciarGeracaoCodigo();
      return;
    }
    if (confirmacoesChave != 0) {
      if (tecla == TECLA_CONFIRMAR) {
        if (--confirmacoesChave == 0) {
          // Grave a ocultação antes de pedir o PIN para não voltar a mostrar o código.
          EEPROM.update(ADDR_REC_MAGIC, PIN_PENDENTE);
          modo = DEFINIR_INICIAL;
        }
        mostrarEntrada();
      }
      return;
    }
  }
  if (tecla == TECLA_TROCAR_SENHA) {
    if (!portaAberta && !solicitandoCodigo() && modo == NORMAL) {
      modo = VALIDAR_ATUAL;
      limparEntrada();
    }
    return;
  }
  if (portaAberta) {
    if (vendoHist) {
      if (tecla == TECLA_LIMPAR || tecla == TECLA_CONFIRMAR) {
        // Esta pressão só fecha o histórico. A próxima confirma o fechamento da porta.
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
    if (solicitandoCodigo()) confirmarCodigo();
    else confirmarSenha();
  } else if (tecla >= '0' && tecla <= '9' && quantidade < (solicitandoCodigo() ? TAM_CODIGO_REC : TAM_PIN)) {
    digitado[quantidade++] = tecla;
    // O terminador permite comparar o PIN como texto sem ler além da entrada.
    digitado[quantidade] = '\0';
    mostrarEntrada();
  }
}

void setup() {
  wdt_disable(); // Só o primeiro uso ativa o watchdog em modo de interrupção.
  // EEPROM apagada contém 0xFF. Aceite somente quatro números com terminador.
  char salva[5];
  EEPROM.get(ADDR_PIN, salva);
  bool valida = salva[4] == '\0';
  for (byte i = 0; i < 4; i++) {
    if (salva[i] < '0' || salva[i] > '9') valida = false;
  }
  if (valida) strcpy(senha, salva);
  portaAberta = EEPROM.read(ADDR_PORTA) == 1;
  zerarEntrada();
  confirmandoPin = false;
  modo = NORMAL;
  erros = 0;
  carregarChave();
  // Linhas em alta impedância até a varredura selecionar uma. Colunas começam em HIGH.
  for (byte i = 0; i < 4; i++) pinMode(PIN_LINHAS[i], INPUT);
  for (byte i = 0; i < 3; i++) pinMode(PIN_COLUNAS[i], INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  controlarBuzzer(false);

  // A porta pode ter ficado aberta ao desligar; inicialize o servo em neutro.
  trava.writeMicroseconds(SERVO_PARADO_US);
  trava.attach(PIN_SERVO);
  pararServo();
  lcd.init();
  lcd.backlight();
  carregarHistorico();
  registrarEvento(EV_INICIO);
  configurarTimer();
  // Reinicialize o teclado antes de iniciar as fases temporizadas.
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
  // Cada passagem faz um pouco de cada tarefa. Nenhuma fase espera em um laço.
  char tecla = lerPressionamento();
  if (tecla != '\0' && fase == LIVRE) tratarTecla(tecla);
  atualizarFase();
  atualizarGeracaoCodigo();
  // O histórico fecha sozinho após 30 s, conservando o estado da porta.
  if (vendoHist && uint32_t(lerTimer() - ultimaAcaoHist) >= 30000UL * TICKS_POR_MS) {
    vendoHist = false;
    mostrarEntrada();
  }
}
