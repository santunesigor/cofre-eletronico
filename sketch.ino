/*
 * COFRE ELETRONICO - Arduino Uno / Wokwi
 * Componentes: LCD 16x2 paralelo, microservo, buzzer e 12 botoes (matriz 4x3)
 * Senha inicial: 1234 | Confirmar valida ou fecha | Limpar apaga
 * Nao precisa da biblioteca Keypad: varredura da matriz feita no codigo.
 * Simulacao no Wokwi com as bibliotecas Servo e LiquidCrystal.
 */

#include <LiquidCrystal.h>
#include <Servo.h>
#include <string.h>

// LCD RS, E, D4, D5, D6, D7
LiquidCrystal lcd(10, 11, 12, 13, A0, A1);
Servo trava;

const byte PIN_SERVO = 9;
const byte PIN_BUZZER = A2;
// Codigos internos dos botoes rotulados no circuito.
const char TECLA_LIMPAR = '*';
const char TECLA_CONFIRMAR = '#';
const byte PIN_LINHAS[4] = {2, 3, 4, 5};
const byte PIN_COLUNAS[3] = {6, 7, 8};
const char TECLAS[4][3] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {TECLA_LIMPAR, '0', TECLA_CONFIRMAR}
};

const char SENHA[] = "1234";  // Altere aqui para personalizar
const unsigned long DEBOUNCE_MS = 40;
const byte ANGULO_FECHADO = 0;
const byte ANGULO_ABERTO = 90;

char digitado[5] = "";
byte quantidade = 0;
bool portaAberta = false;

char leituraAnterior = '\0';
char teclaEstavel = '\0';
unsigned long ultimaMudanca = 0;

void mostrarEntrada() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Digite a senha");
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
  for (byte linha = 0; linha < 4; linha++) {
    digitalWrite(PIN_LINHAS[linha], LOW);
    pinMode(PIN_LINHAS[linha], OUTPUT);
    delayMicroseconds(30);

    for (byte coluna = 0; coluna < 3; coluna++) {
      if (digitalRead(PIN_COLUNAS[coluna]) == LOW) {
        pinMode(PIN_LINHAS[linha], INPUT);
        return TECLAS[linha][coluna];
      }
    }
    pinMode(PIN_LINHAS[linha], INPUT);
  }
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
    if (teclaEstavel != '\0') return teclaEstavel;
  }
  return '\0';
}

void abrirPorta() {
  portaAberta = true;
  trava.write(ANGULO_ABERTO);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Para fechar:");
  lcd.setCursor(0, 1);
  lcd.print("Aperte Confirmar");
}

void fecharPorta() {
  trava.write(ANGULO_FECHADO);
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
  if (strcmp(digitado, SENHA) == 0) {
    quantidade = 0;
    digitado[0] = '\0';
    abrirPorta();
  } else {
    lcd.clear();
    lcd.print("Senha incorreta");
    lcd.setCursor(0, 1);
    lcd.print("Tente de novo");
    tone(PIN_BUZZER, 1000, 600);
    delay(1200);
    limparEntrada();
  }
}

void tratarTecla(char tecla) {
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
  for (byte i = 0; i < 4; i++) pinMode(PIN_LINHAS[i], INPUT);
  for (byte i = 0; i < 3; i++) pinMode(PIN_COLUNAS[i], INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);

  trava.attach(PIN_SERVO);
  trava.write(ANGULO_FECHADO);
  lcd.begin(16, 2);
  mostrarEntrada();
}

void loop() {
  char tecla = lerPressionamento();
  if (tecla != '\0') tratarTecla(tecla);
}
