PROJETO: FECHADURA ELETRONICA COM TECLADO MATRICIAL DE BOTOES
================================================================

O QUE O PROJETO FAZ
------------------
- Usa 12 botoes de clique, montados numa matriz 4x3 (nao e um teclado pronto).
- Le 4 numeros, exibe a quantidade digitada no LCD sem revelar a senha.
- A tecla # confirma. A tecla * apaga o que foi digitado.
- Com a senha certa (1234), o servo gira de 0 para 90 graus.
- A porta fecha automaticamente depois de 5 segundos.
- Com senha incorreta, exibe uma mensagem e permite nova tentativa.

COMPONENTES
-----------
1 Arduino Uno R3
1 LCD 16x2 (modelo PARALELO, sem adaptador I2C)
1 Servo motor, como micro servo SG90
12 Botoes pulsadores (pushbuttons / tact switches), normalmente abertos
1 Potenciometro de 10 kOhms para contraste do LCD
1 Resistor de 220 Ohms para luz do LCD
1 Protoboard e fios jumper

PINOS DO ARDUINO UNO
--------------------
D2  -> Linha 1 dos botoes: 1, 2, 3
D3  -> Linha 2 dos botoes: 4, 5, 6
D4  -> Linha 3 dos botoes: 7, 8, 9
D5  -> Linha 4 dos botoes: *, 0, #
D6  -> Coluna 1 dos botoes: 1, 4, 7, *
D7  -> Coluna 2 dos botoes: 2, 5, 8, 0
D8  -> Coluna 3 dos botoes: 3, 6, 9, #
D9  -> Sinal do servomotor
D10 -> LCD RS (pino 4)
D11 -> LCD E / Enable (pino 6)
D12 -> LCD D4 (pino 11)
D13 -> LCD D5 (pino 12)
A0  -> LCD D6 (pino 13)
A1  -> LCD D7 (pino 14)

OUTROS PINOS DO LCD (numeracao fisica do display)
-------------------------------------------------
LCD  1 VSS -> GND
LCD  2 VDD -> 5V
LCD  3 V0  -> pino CENTRAL do potenciometro 10k
LCD  5 RW  -> GND
LCD 15 A   -> 5V atraves de resistor de 220 ohms
LCD 16 K   -> GND
Extremos do potenciometro -> 5V e GND (um extremo em cada)
LCD pinos 7, 8, 9 e 10 (D0-D3) ficam desconectados.

SERVO
-----
Sinal (normalmente laranja/amarelo) -> D9
Vermelho (+) -> 5V
Marrom/preto (-) -> GND

COMO OS BOTOES SAO LIGADOS
-------------------------
A matriz tem 4 linhas e 3 colunas. Cada botao liga UMA linha a UMA coluna
SOMENTE quando pressionado. Nao ligue nenhum botao diretamente ao 5V ou GND.

             Col 1 (D6)   Col 2 (D7)   Col 3 (D8)
Linha 1 D2:      [1]          [2]          [3]
Linha 2 D3:      [4]          [5]          [6]
Linha 3 D4:      [7]          [8]          [9]
Linha 4 D5:      [*]          [0]          [#]

ATENCAO: em botoes de 4 terminais, dois terminais do mesmo lado eletrico
ja estao unidos internamente. Escolha dois terminais de CONTATOS opostos,
que apenas se conectam quando voce aperta. Confira com o multimetro se
estiver montando fisicamente. No Wokwi sao pinos 1.l e 2.l.

COMO USAR NO TINKERCAD
----------------------
1. Entre em https://www.tinkercad.com/circuits e crie um circuito novo.
2. Coloque o Arduino, LCD, micro servo, 12 pushbuttons, potenciometro e resistor.
3. Faca as ligacoes seguindo o mapa acima e o arquivo DIAGRAMA_LIGACOES.png.
4. Abra Codigo > Texto. Apague o exemplo e cole Fechadura_Arduino.ino.
5. Inicie a simulacao. Pressione 1, 2, 3, 4 e depois #.
6. O servo abre 90 graus e fecha apos 5 segundos. Use * para apagar.

IMPORTANTE: Tinkercad Circuits NAO possui funcao de importar um arquivo de
circuito completo, como este pacote. O arquivo .ino e o diagrama estao
prontos, mas a montagem no editor Tinkercad ainda requer posicionar as
pecas e fazer as conexoes. Arquivos .brd que o Tinkercad exporta nao sao
um formato de projeto reimportavel no editor de circuitos.

VERSAO COM CIRCUITO JA DESCRITO PARA OUTRO SIMULADOR (WOKWI)
---------------------------------------------------------
A pasta projeto_wokwi contem:
- diagram.json: todas as pecas e os fios, incluidos os 12 botoes individuais.
- sketch.ino: exatamente o mesmo programa do Tinkercad.
No https://wokwi.com/projects/new/arduino-uno, substitua o conteudo de
sketch.ino e diagram.json pelos arquivos do pacote e inicie a simulacao.
O simulador Wokwi nao modela contraste do LCD, portanto V0 esta ligado ao
GND nele, em vez do potenciometro. O Tinkercad requer o potenciometro.

Na simulacao Wokwi, e possivel usar o mouse ou 1..9, 0, X (=*) e Z (=#).

NOTA SOBRE PROJETO FISICO
-------------------------
No Arduino real, se o servo causar reinicios, use fonte regulada 5V
separada para o servo, com GND ligado ao GND do Arduino (terra comum).
Nao ligue o servo diretamente a uma fechadura fisica pesada sem considerar
o torque e a alimentacao. Este projeto e um prototipo didatico.
