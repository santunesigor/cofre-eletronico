# Cofre eletrônico

Protótipo didático de um cofre eletrônico com Arduino Uno, senha numérica, display LCD, servomotor e alerta sonoro. Usamos o **Wokwi** como simulador do circuito.

## Funcionamento

- A senha possui quatro dígitos; a senha inicial é `1234`, definida em `SENHA` no arquivo `sketch.ino`.
- O teclado é uma matriz 4×3 construída com 12 botões individuais. O programa faz a leitura da matriz e aplica debounce de 40 ms.
- O LCD mostra os dígitos como asteriscos. **Limpar** apaga toda a entrada e **Confirmar** valida a senha.
- Uma senha correta gira o servo para 90° e mantém o cofre aberto até o usuário pressionar **Confirmar** novamente.
- Enquanto aberto, o LCD exibe `Para fechar:` na primeira linha e `Aperte Confirmar` na segunda. Os demais botões são ignorados nesse estado.
- Todas as mensagens cabem nas 16 colunas de cada linha do LCD e permanecem estáticas para facilitar a leitura.
- Ao fechar, o servo retorna a 0° e o sistema fica pronto para uma nova senha.
- Uma senha incorreta mantém o cofre fechado, exibe uma mensagem de erro e aciona o buzzer a 1.000 Hz por 600 ms. Após a mensagem, a entrada é limpa para uma nova tentativa.
- Confirmar com menos de quatro dígitos mostra `Digite 4 numeros` e preserva a entrada para completá-la.
- Ao iniciar ou reiniciar, o sistema comanda o servo para a posição fechada.

## Componentes

| Quantidade | Componente |
| --- | --- |
| 1 | Arduino Uno R3 |
| 1 | LCD 16×2 paralelo, em modo de 4 bits |
| 1 | Microservomotor |
| 1 | Buzzer piezoelétrico passivo |
| 12 | Botões pulsadores normalmente abertos |
| 1 | Potenciômetro de 10 kΩ para o contraste do LCD |
| 1 | Resistor de 220 Ω para a iluminação do LCD |

## Pinagem

| Arduino | Ligação |
| --- | --- |
| D2–D5 | Linhas da matriz: `1 2 3`, `4 5 6`, `7 8 9`, `Limpar 0 Confirmar` |
| D6–D8 | Colunas da matriz: `1 4 7 Limpar`, `2 5 8 0`, `3 6 9 Confirmar` |
| D9 | Sinal do servo |
| D10 | LCD RS |
| D11 | LCD E |
| D12 | LCD D4 |
| D13 | LCD D5 |
| A0 | LCD D6 |
| A1 | LCD D7 |
| A2 | Positivo do buzzer (pino 2 no Wokwi) |
| GND | Negativo do buzzer (pino 1 no Wokwi) |

LCD: VSS, RW e K em GND; VDD em 5 V; A em 5 V através do resistor de 220 Ω. V0 no terminal central do potenciômetro, cujas extremidades estão em 5 V e GND. D0–D3 ficam desconectados. O servo recebe 5 V e GND, além do sinal em D9.

Cada botão conecta uma linha a uma coluna quando pressionado. As colunas usam `INPUT_PULLUP`, dispensando resistores externos para o teclado. No Wokwi, os contatos usados são `1.l` e `2.l`.

## Simulação no Wokwi

Crie um [projeto Arduino Uno no Wokwi](https://wokwi.com/projects/new/arduino-uno) e copie os arquivos deste repositório para o projeto:

| Arquivo | Conteúdo |
| --- | --- |
| `sketch.ino` | Programa do cofre |
| `diagram.json` | Componentes e ligações |
| `libraries.txt` | Bibliotecas Servo e LiquidCrystal |

Inicie a simulação e digite `1234`, seguido de **Confirmar**, para abrir. Solte o botão e pressione **Confirmar** novamente para fechar. É possível clicar nos botões ou usar os atalhos do teclado:

| Botão | Atalho |
| --- | --- |
| Números | `0` a `9` |
| Limpar | `X` |
| Confirmar | `Z` |

O servomotor representa o mecanismo de trava deste protótipo; a senha fica definida no código e não pode ser alterada pelo teclado.

## Referências

- [Buzzer no Wokwi](https://docs.wokwi.com/parts/wokwi-buzzer)
- [Formato do circuito no Wokwi](https://docs.wokwi.com/diagram-format)
