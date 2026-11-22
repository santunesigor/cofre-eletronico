# Cofre eletrônico

Protótipo didático de um cofre eletrônico com Arduino Uno, senha numérica, display LCD, servomotor e alerta sonoro. Usamos o **Wokwi** como simulador do circuito.

## Funcionamento

- A senha possui quatro dígitos; a senha inicial é `1234`, usada quando ainda não há senha válida salva na EEPROM.
- O teclado é uma matriz 4×3 construída com 12 botões individuais. O programa faz a leitura da matriz e aplica debounce de 40 ms.
- O LCD mostra os dígitos como asteriscos. **Limpar** apaga toda a entrada e **Confirmar** valida a senha.
- Uma senha correta aciona o servo em um sentido por 500 ms, envia o comando de parada e mantém o cofre aberto até o usuário pressionar **Confirmar** novamente.
- Enquanto aberto, o LCD exibe `Para fechar:` na primeira linha e `Aperte Confirmar` na segunda. Os demais botões são ignorados nesse estado.
- Todas as mensagens cabem nas 16 colunas de cada linha do LCD e permanecem estáticas para facilitar a leitura.
- Ao fechar, o servo gira no sentido contrário por 500 ms e para; o sistema fica pronto para uma nova senha.
- Uma senha incorreta mantém o cofre fechado, exibe uma mensagem de erro e aciona o buzzer a 4.000 Hz por 1,5 segundo. Após a mensagem, a entrada é limpa para uma nova tentativa.
- Confirmar com menos de quatro dígitos mostra `Digite 4 numeros` e preserva a entrada para completá-la.
- Ao iniciar ou reiniciar, o sistema envia apenas o comando de parada ao servo e recupera a senha e o estado aberto/fechado da EEPROM. Se desligar aberto, ao religar o LCD mostra `Para fechar:` / `Aperte Confirmar`; pressione **Confirmar** para fechar normalmente. Na primeira gravação desta versão, sem estado salvo, o programa assume fechado: alinhe a trava nessa posição antes do primeiro uso.
- Para trocar a senha, com o cofre fechado, segure **Limpar + Confirmar juntos por 5 segundos** e solte os dois. O LCD pede `Senha atual:`; digite os quatro números atuais e pressione **Confirmar**. Em `Nova senha:`, digite exatamente quatro números e pressione **Confirmar** para salvar.
- Durante a troca, **Limpar** apaga a entrada, dígitos além do quarto são ignorados e uma senha atual incorreta impede a alteração. A nova senha permanece salva após desligar o Arduino.

## Ajuste do servo físico

Esta versão foi adaptada ao servo que está dando voltas completas. A abertura e o fechamento usam tempo, sem medir o ângulo. No `sketch.ino`:

- `SERVO_PARADO_US = 1500`: pulso de parada inicial; ajuste em pequenos passos se o servo ainda se mover durante esse comando.
- `SERVO_ABRIR_US = 1600` e `SERVO_FECHAR_US = 1400`: comandos em sentidos contrários; troque os valores entre si se o mecanismo estiver invertido.
- `TEMPO_MOVIMENTO_MS = 500`: duração inicial curta; calibre o percurso com o servo desacoplado da trava.

Ao terminar um movimento, o programa envia o neutro e mantém os pulsos ativos em D9, inclusive enquanto aguarda o teclado. Esta é uma tentativa de aumentar a resistência ao deslocamento manual; em um servo de rotação contínua, o neutro não controla a posição nem garante bloqueio. Se o servo girar sozinho, ajuste `SERVO_PARADO_US` antes de usar a trava. Reiniciar não reposiciona a trava. O estado aberto é salvo antes de começar a abertura, e o estado fechado somente depois de terminar o fechamento. Uma queda de energia durante o movimento pode deixar o percurso incompleto; sem sensor, é necessário conferir e realinhar a trava manualmente nesse caso. Os percursos podem variar com a carga e a alimentação; não há confirmação física de fechamento.

A alimentação somente pela USB ainda pode causar queda de tensão no LCD. Limitar o tempo do movimento não corrige falta de corrente nem defeito do servo.

## Componentes

| Quantidade | Componente |
| --- | --- |
| 1 | Arduino Uno R3 |
| 1 | LCD 16×2 com módulo I2C integrado (PCF8574) |
| 1 | Microservomotor |
| 1 | Buzzer piezoelétrico passivo |
| 12 | Botões pulsadores normalmente abertos |

O LCD I2C dispensa o potenciômetro e o resistor externos do circuito paralelo: o módulo normalmente já possui ajuste de contraste (trimpot) e circuito de iluminação.

## Pinagem

| Arduino | Ligação |
| --- | --- |
| D2–D5 | Linhas da matriz: `1 2 3`, `4 5 6`, `7 8 9`, `Limpar 0 Confirmar` |
| D6–D8 | Colunas da matriz: `1 4 7 Limpar`, `2 5 8 0`, `3 6 9 Confirmar` |
| D9 | Sinal do servo |
| A2 | Positivo do buzzer (pino 2 no Wokwi) |
| A4 (SDA) | LCD SDA |
| A5 (SCL) | LCD SCL |
| 5V | LCD VCC e alimentação do servo |
| GND | LCD GND, negativo do buzzer e GND do servo |

O display usa apenas **GND, VCC, SDA e SCL**. Os pinos D10–D13 e A0–A1 ficam livres. Cada botão conecta uma linha a uma coluna quando pressionado. As colunas usam `INPUT_PULLUP`, dispensando resistores externos para o teclado. No Wokwi, os contatos usados são `1.l` e `2.l`.

**Endereço I2C:** a simulação utiliza `0x27`, definido em `ENDERECO_LCD` no `sketch.ino` e no atributo `i2cAddress` do `diagram.json`. Se o seu módulo físico tiver outro endereço (por exemplo `0x3F`), identifique-o com um scanner I2C e ajuste `ENDERECO_LCD` antes de gravar o Arduino. Se a tela acender mas os caracteres não aparecerem, confira o endereço e ajuste o trimpot de contraste na placa I2C.

Para o circuito físico, use alimentação 5 V adequada ao servo e GND comum; dependendo da carga do mecanismo, pode ser necessária uma fonte separada para o servo (com terra compartilhado).

## Simulação no Wokwi

Crie um [projeto Arduino Uno no Wokwi](https://wokwi.com/projects/new/arduino-uno) e copie os arquivos deste repositório para o projeto:

| Arquivo | Conteúdo |
| --- | --- |
| `sketch.ino` | Programa do cofre |
| `diagram.json` | Componentes e ligações |
| `libraries.txt` | Bibliotecas Servo e LiquidCrystal I2C |

O diagrama separa o teclado dos periféricos e usa trajetos definidos para os fios: laranja para as linhas e azul para as colunas. No menu de três pontos da simulação, escolha **Fit** para enquadrar todo o circuito; amplie para conferir os pinos.

Inicie a simulação e digite `1234`, seguido de **Confirmar**, para abrir. Solte o botão e pressione **Confirmar** novamente para fechar. É possível clicar nos botões ou usar os atalhos do teclado:

| Botão | Atalho |
| --- | --- |
| Números | `0` a `9` |
| Limpar | `X` |
| Confirmar | `Z` |

O servo do diagrama Wokwi é posicional e não reproduz o movimento contínuo do servo físico; use a simulação para conferir o teclado, as mensagens e a troca de senha. O percurso real precisa ser calibrado no hardware. No Wokwi, use os atalhos **X + Z** juntos por 5 segundos para iniciar a troca de senha. A persistência da EEPROM entre sessões depende do simulador.

Para executar o teste de regressão do firmware no computador (com Python 3 e g++): `python3 tests/test_senha.py`.

## Referências

- [Buzzer no Wokwi](https://docs.wokwi.com/parts/wokwi-buzzer)
- [Formato do circuito no Wokwi](https://docs.wokwi.com/diagram-format)
- [LCD1602 I2C no Wokwi](https://docs.wokwi.com/parts/wokwi-lcd1602)
