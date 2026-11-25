# Cofre eletrônico

Protótipo didático de um cofre eletrônico com Arduino Uno, senha numérica, display LCD, servomotor e alerta sonoro. Usamos o **Wokwi** como simulador do circuito.

## Como compilar e gravar na Arduino IDE

Este projeto usa **Arduino Uno R3 ou compatível com ATmega328P**. Os dois sketches estão no repositório e podem ser abertos diretamente na IDE.

| Arquivo | Quando abrir |
| --- | --- |
| [CofreEletronico.ino](CofreEletronico/CofreEletronico.ino) | Para compilar e gravar o programa do cofre. |
| [LimparEEPROM.ino](LimparEEPROM/LimparEEPROM.ino) | Só quando quiser apagar os dados e simular o primeiro uso. |

1. Instale a [Arduino IDE](https://www.arduino.cc/en/software). No **Gerenciador de Placas**, instale **Arduino AVR Boards**. No **Gerenciador de Bibliotecas**, instale **Servo** e **LiquidCrystal I2C**.
2. Conecte o Uno por um cabo USB de dados. Selecione **Ferramentas → Placa → Arduino AVR Boards → Arduino Uno**. Em **Ferramentas → Porta**, selecione a porta que aparece ao conectar a placa. O nome muda conforme o computador.
3. Abra `CofreEletronico/CofreEletronico.ino`. Clique em **Verificar (✓)** para só compilar ou em **Carregar (→)** para compilar e gravar. O carregamento normal mantém os dados existentes na EEPROM.

### Começar do zero pela IDE

**Este procedimento apaga o PIN, o código de recuperação e o histórico.** Use-o para repetir o primeiro uso ou começar uma demonstração com o cofre vazio.

1. Abra `LimparEEPROM/LimparEEPROM.ino` e clique em **Carregar (→)**.
2. Abra o **Monitor Serial**, selecione **115200 baud**, digite `?` e envie. Aguarde `EEPROM bytes FF: 1024/1024`. Essa resposta confirma que cada byte foi apagado.
3. Feche o Monitor Serial. Abra `CofreEletronico/CofreEletronico.ino` e clique em **Carregar (→)** novamente. O programa de limpeza é temporário e precisa ser substituído pelo programa do cofre.
4. No LCD, aguarde `Gerando codigo`. Anote os 16 dígitos exibidos e pressione **Confirmar** três vezes. Escolha um PIN de quatro dígitos e digite-o novamente para confirmar.

### Por que fizemos assim

- A IDE permite compilar e gravar em outro computador sem preparar scripts ou depender do nome que o sistema deu à porta USB.
- A flash guarda o programa. A EEPROM guarda o PIN, o código e o histórico. Por isso a atualização normal preserva os dados e a limpeza é uma etapa separada.
- Quando a EEPROM está vazia, o Uno gera o código no primeiro boot. O firmware compara o Timer2 com o oscilador independente do watchdog e aproveita a variação entre os relógios. Se a coleta não render um código em 30 segundos, o LCD mostra `Falha na geracao`. Pressione **Confirmar** para tentar novamente no circuito físico. Este é um gerador didático, sem garantia criptográfica. [Datasheet do ATmega328P](https://www.microchip.com/content/dam/mchp/documents/MCU08/ProductDocuments/DataSheets/ATmega48A-PA-88A-PA-168A-PA-328-P-DS-DS40002061B.pdf)

## Funcionamento

- A senha possui quatro dígitos. Em uma unidade nova, o usuário define o PIN no primeiro uso. Em unidades de versões anteriores, o firmware preserva o PIN existente, inclusive o padrão `1234` quando ele ainda é usado.
- O teclado é uma matriz 4×3 construída com 12 botões individuais. O programa faz a leitura da matriz e aplica debounce de 40 ms.
- O LCD mostra os dígitos como asteriscos. **Limpar** apaga toda a entrada e **Confirmar** valida a senha.
- Uma senha correta aciona o servo em um sentido por 500 ms, envia o comando de parada e toca **dois bipes rápidos de confirmação** (4 kHz, 70 ms cada, com intervalo de 50 ms). O cofre permanece aberto até o usuário pressionar **Confirmar** novamente.
- Enquanto aberto, o LCD exibe `Para fechar:` na primeira linha e `Aperte Confirmar` na segunda. Os demais botões são ignorados nesse estado, exceto `0` para consultar o histórico após uma abertura autenticada.
- As mensagens do LCD permanecem estáticas, inclusive no bloqueio.
- Ao fechar, o servo gira no sentido contrário por 500 ms e para; o sistema fica pronto para uma nova senha.
- Uma senha incorreta mantém o cofre fechado, exibe uma mensagem de erro e aciona o buzzer a 4.000 Hz por 1,5 segundo. Após a mensagem, a entrada é limpa para uma nova tentativa.
- Confirmar com menos de quatro dígitos mostra `Digite 4 numeros` e preserva a entrada para completá-la.
- Ao iniciar ou reiniciar, o sistema envia apenas o comando de parada ao servo e recupera a senha e o estado aberto/fechado da EEPROM. Se desligar aberto, ao religar o LCD mostra `Para fechar:` / `Aperte Confirmar`; pressione **Confirmar** para fechar normalmente. Na primeira gravação desta versão, sem estado salvo, o programa assume fechado: alinhe a trava nessa posição antes do primeiro uso.
- Para trocar a senha, com o cofre fechado, segure **Limpar + Confirmar juntos por 5 segundos** e solte os dois. O LCD pede `Senha atual:`; digite os quatro números atuais e pressione **Confirmar**. Em `Novo PIN:`, digite quatro números, confirme, repita o novo PIN e confirme para salvar.
- Durante a troca, **Limpar** apaga a entrada, dígitos além do quarto são ignorados e uma senha atual incorreta impede a alteração. A nova senha permanece salva após desligar o Arduino.

## Histórico de acessos

O firmware mantém os últimos **120 eventos** na EEPROM (endereços 32 a 991), incluindo inicializações, aberturas e fechamentos comandados, PIN incorreto, bloqueio, código de recuperação incorreto, recuperação bem-sucedida e alteração de senha. Não armazena os dígitos informados nem o código de recuperação no histórico. Não há data e hora de calendário porque o Uno não possui RTC; os registros são ordenados por sequência.

Para consultar, abra o cofre com a senha e pressione **0** enquanto estiver aberto. O LCD mostra `Reg. 001/120` (posição do registro e total existente) e o tipo do evento. Pressione **2** para registros mais antigos, **8** para mais recentes e **Limpar** ou **Confirmar** para voltar à tela de fechamento. O histórico também sai após 30 segundos sem navegação. Após reiniciar com o cofre aberto, é necessário fechá-lo e autenticar-se novamente para consultar.

A memória é circular: ao lotar, o próximo evento substitui o mais antigo. Cada registro usa número sequencial, tipo, CRC-16 e marcador de validade gravado por último. Uma gravação interrompida pode perder o registro em curso, mas não invalida os demais. Os registros sobrevivem a reinícios; sem sensores, abertura e fechamento correspondem à conclusão do comando do servo, não à confirmação física da porta. O histórico não é inviolável contra acesso físico à EEPROM.

## Bloqueio e código de recuperação

Na primeira inicialização com o cofre fechado, o firmware gera e salva o código na EEPROM se ainda não houver um. Anote os 16 dígitos em local seguro e pressione **Confirmar** três vezes separadas. A terceira pressão encerra a exibição mesmo após reiniciar. Se desligar antes dela, o código volta a aparecer e são pedidas três pressões. O Arduino Uno não oferece armazenamento seguro para esse código. Ele pode ser lido por quem tiver acesso à memória da placa.

Em `Defina seu PIN`, escolha quatro dígitos e pressione **Confirmar**. Em `Confirme seu PIN`, digite o mesmo PIN e confirme. Se os PINs divergirem, escolha e confirme novamente. Se desligar antes de salvar, o cofre volta à tela de cadastro com o código oculto.

Após três PINs completos incorretos consecutivos, o LCD mostra `Cod. recuperacao`. Digite diretamente os 16 números anotados no primeiro uso e pressione **Confirmar**. A segunda linha mostra um asterisco por dígito, e **Limpar** apaga a entrada. Se o código estiver correto, escolha e confirme um novo PIN de quatro dígitos. O cofre só sai do bloqueio quando o novo PIN é salvo. Depois, use o novo PIN para abrir.

Um código completo incorreto inicia uma espera de 30 segundos. Reiniciar com o cofre bloqueado mantém o bloqueio e exige a mesma espera. Se a energia cair antes da confirmação do novo PIN, informe o código novamente. A recuperação funciona pelo teclado, sem depender de aplicativo ou conexão de rede.

## Temporização por hardware

O Arduino Uno R3 (ATmega328P, 16 MHz) usa o **Timer2 em modo CTC**, com prescaler 8 e OCR2A = 249, gerando uma interrupção a cada 125 µs (8 kHz). A interrupção incrementa um contador de ticks e alterna o pino A2 quando o buzzer está ativo, produzindo 4 kHz. O Timer1 permanece reservado à biblioteca Servo; o Timer0 do núcleo Arduino não é reconfigurado.

O teclado, o debounce de 40 ms, a combinação de 5 segundos, o movimento do servo, os bipes e as mensagens do LCD são controlados por uma máquina de estados não bloqueante. O sketch não chama `delay()`, `delayMicroseconds()`, `millis()`, `micros()` ou `tone()`. As bibliotecas e as operações de I2C/EEPROM podem continuar usando esperas internas. Durante movimentos e avisos, o teclado continua sendo varrido, mas novas ações são ignoradas para evitar comandos concorrentes.

O Timer2 fica reservado ao firmware: não use simultaneamente bibliotecas que o reconfigurem ou PWM por hardware nos pinos D3/D11. A pinagem do circuito permanece a mesma.

## Ajuste do servo físico

Esta versão foi adaptada ao servo que está dando voltas completas. A abertura e o fechamento usam tempo, sem medir o ângulo. No `CofreEletronico/CofreEletronico.ino`:

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

**Endereço I2C:** a simulação utiliza `0x27`, definido em `ENDERECO_LCD` no sketch principal e no atributo `i2cAddress` do `diagram.json`. Se o seu módulo físico tiver outro endereço (por exemplo `0x3F`), identifique-o com um scanner I2C e ajuste `ENDERECO_LCD` antes de gravar o Arduino. Se a tela acender mas os caracteres não aparecerem, confira o endereço e ajuste o trimpot de contraste na placa I2C.

Para o circuito físico, use alimentação 5 V adequada ao servo e GND comum; dependendo da carga do mecanismo, pode ser necessária uma fonte separada para o servo (com terra compartilhado).

## Simulação no Wokwi

Crie um [projeto Arduino Uno no Wokwi](https://wokwi.com/projects/new/arduino-uno) e copie os arquivos deste repositório para o projeto:

| Arquivo | Conteúdo |
| --- | --- |
| `CofreEletronico/CofreEletronico.ino` | Copie o conteúdo para o `sketch.ino` do Wokwi. |
| `diagram.json` | Componentes e ligações |
| `libraries.txt` | Bibliotecas Servo e LiquidCrystal I2C |

O diagrama separa o teclado dos periféricos e usa trajetos definidos para os fios: laranja para as linhas e azul para as colunas. No menu de três pontos da simulação, escolha **Fit** para enquadrar todo o circuito; amplie para conferir os pinos.

A geração do código depende da variação entre dois relógios no Uno físico. Use o Wokwi para conferir o circuito e a pinagem. Se o simulador parar em `Falha na geracao`, teste o primeiro uso na placa física.

É possível clicar nos botões ou usar os atalhos do teclado:

| Botão | Atalho |
| --- | --- |
| Números | `0` a `9` |
| Limpar | `X` |
| Confirmar | `Z` |

O servo do diagrama Wokwi é posicional e não reproduz o movimento contínuo do servo físico; use a simulação para conferir o teclado, as mensagens e a troca de senha. O percurso real precisa ser calibrado no hardware. No Wokwi, use os atalhos **X + Z** juntos por 5 segundos para iniciar a troca de senha. A persistência da EEPROM entre sessões depende do simulador.

## Referências

- [Buzzer no Wokwi](https://docs.wokwi.com/parts/wokwi-buzzer)
- [Formato do circuito no Wokwi](https://docs.wokwi.com/diagram-format)
- [LCD1602 I2C no Wokwi](https://docs.wokwi.com/parts/wokwi-lcd1602)
