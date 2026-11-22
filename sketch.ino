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

// LCD 16x2 I2C: SDA=A4, SCL=A5 no Arduino Uno.
// 0x27 e o endereco padrao no Wokwi; confirme no LCD fisico.
const byte ENDERECO_LCD = 0x27;
LiquidCrystal_I2C lcd(ENDERECO_LCD, 16, 2);
Servo trava;

const byte PIN_SERVO = 9;
const byte PIN_BUZZER = A2;
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

char leituraAnterior = '\0';
char teclaEstavel = '\0';
unsigned long ultimaMudanca = 0;
unsigned long inicioTroca = 0;
bool aguardarSoltar = false;
bool trocaDisparada = false;

void mostrarEntrada() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (portaAberta) {
    lcd.print("Para fechar:");
    lcd.setCursor(0, 1);
    lcd.print("Aperte Confirmar");
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

// As linhas ficam em alta impedancia. So a linha testada vai para LOW.
// As colunas usam INPUT_PULLUP, dispensando resistores para os 12 botoes.
char lerTeclado() {
  unsigned int pressionadas = 0;
  char tecla = '\0';
  for (byte linha = 0; linha < 4; linha++) {
    digitalWrite(PIN_LINHAS[linha], LOW);
    pinMode(PIN_LINHAS[linha], OUTPUT);
    delayMicroseconds(30);

    for (byte coluna = 0; coluna < 3; coluna++) {
      if (digitalRead(PIN_COLUNAS[coluna]) == LOW) {
        pressionadas |= 1U << (linha * 3 + coluna);
        tecla = TECLAS[linha][coluna];
      }
    }
    pinMode(PIN_LINHAS[linha], INPUT);
  }
  if (pressionadas == ((1U << 9) | (1U << 11))) return TECLA_TROCAR_SENHA;
  if (pressionadas && !(pressionadas & (pressionadas - 1))) return tecla;
  return '\0';
}

// So devolve uma tecla por pressionamento (antirruido/antirrepique).
char lerPressionamento() {
  char leitura = lerTeclado();
  unsigned long agora = millis();
  if (leitura != leituraAnterior) {
    leituraAnterior = leitura;
    ultimaMudanca = agora;
  }
  if (agora - ultimaMudanca >= DEBOUNCE_MS && leitura != teclaEstavel) {
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
      !trocaDisparada && agora - inicioTroca >= TEMPO_TROCA_MS) {
    trocaDisparada = true;
    return TECLA_TROCAR_SENHA;
  }
  return '\0';
}

void pararServo() {
  trava.writeMicroseconds(SERVO_PARADO_US);
  delay(100);
}

// shortcut: movimento por tempo nao mede a posicao; use sensores se precisar garantir o fechamento.
void moverServo(int pulso) {
  trava.writeMicroseconds(pulso);
  trava.attach(PIN_SERVO);
  delay(TEMPO_MOVIMENTO_MS);
  pararServo();
}

void abrirPorta() {
  EEPROM.update(sizeof(senha), 1);
  moverServo(SERVO_ABRIR_US);
  portaAberta = true;
  mostrarEntrada();
}

void fecharPorta() {
  moverServo(SERVO_FECHAR_US);
  EEPROM.update(sizeof(senha), 0);
  portaAberta = false;
  lcd.clear();
  lcd.print("Cofre fechado");
  delay(800);
  limparEntrada();
}

void confirmarSenha() {
  if (quantidade != 4) {
    lcd.clear();
    lcd.print("Digite 4 numeros");
    delay(900);
    mostrarEntrada();
    return;
  }
  if (modo == DEFINIR_NOVA) {
    strcpy(senha, digitado);
    EEPROM.put(0, senha);
    modo = NORMAL;
    lcd.clear();
    lcd.print("Senha alterada!");
    delay(1200);
    limparEntrada();
  } else if (strcmp(digitado, senha) == 0) {
    if (modo == VALIDAR_ATUAL) {
      modo = DEFINIR_NOVA;
      limparEntrada();
      return;
    }
    quantidade = 0;
    digitado[0] = '\0';
    abrirPorta();
  } else {
    lcd.clear();
    lcd.print("Senha incorreta");
    lcd.setCursor(0, 1);
    lcd.print("Tente de novo");
    tone(PIN_BUZZER, 4000, 1500);
    delay(1700);
    limparEntrada();
  }
}

void tratarTecla(char tecla) {
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
  for (byte i = 0; i < 4; i++) pinMode(PIN_LINHAS[i], INPUT);
  for (byte i = 0; i < 3; i++) pinMode(PIN_COLUNAS[i], INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);

  trava.writeMicroseconds(SERVO_PARADO_US);
  trava.attach(PIN_SERVO);
  pararServo();
  lcd.init();
  lcd.backlight();
  mostrarEntrada();
}

void loop() {
  char tecla = lerPressionamento();
  if (tecla != '\0') tratarTecla(tecla);
}
