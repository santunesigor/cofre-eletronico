# Cofre eletrônico

Protótipo didático de um cofre eletrônico com Arduino Uno, senha numérica, display LCD, servomotor e alerta sonoro. Usamos o **Wokwi** como simulador do circuito.

## Funcionamento

- A senha possui quatro dígitos; a senha inicial é `1234`, usada quando ainda não há senha válida salva na EEPROM.
- O teclado é uma matriz 4×3 construída com 12 botões individuais. O programa faz a leitura da matriz e aplica debounce de 40 ms.
- O LCD mostra os dígitos como asteriscos. **Limpar** apaga toda a entrada e **Confirmar** valida a senha.
- Uma senha correta aciona o servo em um sentido por 500 ms, envia o comando de parada e toca **dois bipes rápidos de confirmação** (4 kHz, 70 ms cada, com intervalo de 50 ms). O cofre permanece aberto até o usuário pressionar **Confirmar** novamente.
- Enquanto aberto, o LCD exibe `Para fechar:` na primeira linha e `Aperte Confirmar` na segunda. Os demais botões são ignorados nesse estado, exceto `0` para consultar o histórico após uma abertura autenticada.
- As mensagens normais permanecem estáticas. No bloqueio por três erros, as frases de recuperação rolam nas duas linhas do LCD.
- Ao fechar, o servo gira no sentido contrário por 500 ms e para; o sistema fica pronto para uma nova senha.
- Uma senha incorreta mantém o cofre fechado, exibe uma mensagem de erro e aciona o buzzer a 4.000 Hz por 1,5 segundo. Após a mensagem, a entrada é limpa para uma nova tentativa.
- Confirmar com menos de quatro dígitos mostra `Digite 4 numeros` e preserva a entrada para completá-la.
- Ao iniciar ou reiniciar, o sistema envia apenas o comando de parada ao servo e recupera a senha e o estado aberto/fechado da EEPROM. Se desligar aberto, ao religar o LCD mostra `Para fechar:` / `Aperte Confirmar`; pressione **Confirmar** para fechar normalmente. Na primeira gravação desta versão, sem estado salvo, o programa assume fechado: alinhe a trava nessa posição antes do primeiro uso.
- Para trocar a senha, com o cofre fechado, segure **Limpar + Confirmar juntos por 5 segundos** e solte os dois. O LCD pede `Senha atual:`; digite os quatro números atuais e pressione **Confirmar**. Em `Nova senha:`, digite exatamente quatro números e pressione **Confirmar** para salvar.
- Durante a troca, **Limpar** apaga a entrada, dígitos além do quarto são ignorados e uma senha atual incorreta impede a alteração. A nova senha permanece salva após desligar o Arduino.

## Histórico de acessos

O firmware mantém os últimos **120 eventos** na EEPROM (endereços 32 a 991), incluindo inicializações, aberturas e fechamentos comandados, PIN incorreto, bloqueio, contrassenha incorreta, recuperação bem-sucedida e alteração de senha. Não armazena os dígitos informados nem a chave de recuperação no histórico. Não há data e hora de calendário porque o Uno não possui RTC; os registros são ordenados por sequência.

Para consultar, abra o cofre com a senha e pressione **0** enquanto estiver aberto. O LCD mostra `Reg. 001/120` (posição do registro e total existente) e o tipo do evento. Pressione **2** para registros mais antigos, **8** para mais recentes e **Limpar** ou **Confirmar** para voltar à tela de fechamento. O histórico também sai após 30 segundos sem navegação. Após reiniciar com o cofre aberto, é necessário fechá-lo e autenticar-se novamente para consultar.

A memória é circular: ao lotar, o próximo evento substitui o mais antigo. Cada registro usa número sequencial, tipo, CRC-16 e marcador de validade gravado por último. Uma gravação interrompida pode perder o registro em curso, mas não invalida os demais. Os registros sobrevivem a reinícios; sem sensores, abertura e fechamento correspondem à conclusão do comando do servo, não à confirmação física da porta. O histórico não é inviolável contra acesso físico à EEPROM.

## Bloqueio e contrassenha (versão em desenvolvimento)

Na primeira inicialização desta versão, com o cofre fechado, configure uma chave de recuperação de **16 dígitos aleatórios**. Abra o arquivo [recuperacao.html](recuperacao.html) em um navegador local, gere a chave e guarde-a em local seguro. Digite-a no teclado do cofre, pressione Confirmar, repita e confirme. Ela fica salva na EEPROM e não deve ser enviada ao GitHub. Se o cofre estiver aberto ao atualizar, feche-o antes da configuração.

Após três senhas completas incorretas consecutivas, o LCD rola as mensagens `Digite a contrassenha` e `Entre em contato com o fabricante`, alternando com a exibição estática do identificador (ID) de 12 caracteres hexadecimais por seis segundos. Ao digitar o primeiro número, a rolagem para e o LCD apresenta `Contrassenha:` e `PIN: ` seguido de asteriscos. Pressionar Limpar volta ao aviso e permite escanear novamente. O ID muda a cada minuto usando o Timer2; o Arduino não tem relógio de calendário. Informe a chave secreta e o ID atual no calculador offline. Digite a contrassenha de seis dígitos no cofre e pressione Confirmar. Uma contrassenha válida apenas libera a digitação do PIN normal, sem abrir diretamente a trava. Uma tentativa de recuperação errada exige 30 segundos de espera e gera uma nova sessão. Reiniciar não elimina o bloqueio e também exige espera antes de apresentar um novo ID. O contador de erros é zerado após autenticação normal correta ou recuperação válida.

A função SipHash-2-4 usa a chave de 16 caracteres ASCII e a mensagem de oito bytes little-endian correspondente ao ID hexadecimal. O resultado reduzido módulo 1.000.000 produz o código de seis dígitos (incluindo zeros à esquerda). O ID combina um contador persistente de sessões e uma janela de tempo, não depende de internet nem de relógio externo. A chave deve ser gerada aleatoriamente e guardada; sem ela, a recuperação exige manutenção física do firmware/EEPROM. Configure em ambiente confiável. Este é um protótipo: a EEPROM não é protegida contra leitura ou regravação física e o código de seis dígitos não oferece segurança equivalente a um cofre certificado.

### Scanner do LCD

O calculador também lê a primeira linha do LCD pela câmera (OCR local), confirma o mesmo ID em duas capturas consecutivas e calcula automaticamente se a chave estiver preenchida. Há opção de foto e digitação manual. Confira sempre o ID reconhecido antes de usar a contrassenha.

Para preparar os arquivos OCR, com Node.js instalado, execute `npm install` e `npm run prepare:ocr`. Depois sirva a pasta com `python3 -m http.server 8000` e abra `http://localhost:8000/recuperacao.html` no próprio computador. Para o celular, hospede o HTML e a pasta `ocr/` juntos em um endereço HTTPS; HTTP por IP da rede local normalmente não permite acesso à câmera. O workflow **Preparar scanner do LCD** gera um ZIP pronto com os arquivos OCR. Nenhuma imagem ou chave é enviada a serviços externos; as bibliotecas e modelos são distribuídos localmente no pacote. A digitação manual funciona mesmo sem os arquivos OCR.

Para disponibilizar no celular via GitHub Pages, em **Settings → Pages → Build and deployment → Source**, selecione **GitHub Actions**. Depois abra o workflow **Preparar scanner do LCD**, clique em **Run workflow** e marque `publicar`. O workflow publica o calculador e os arquivos OCR juntos, via HTTPS. O código publicado não contém a sua chave secreta.

## Temporização por hardware

O Arduino Uno R3 (ATmega328P, 16 MHz) usa o **Timer2 em modo CTC**, com prescaler 8 e OCR2A = 249, gerando uma interrupção a cada 125 µs (8 kHz). A interrupção incrementa um contador de ticks e alterna o pino A2 quando o buzzer está ativo, produzindo 4 kHz. O Timer1 permanece reservado à biblioteca Servo; o Timer0 do núcleo Arduino não é reconfigurado.

O teclado, o debounce de 40 ms, a combinação de 5 segundos, o movimento do servo, os bipes e as mensagens do LCD são controlados por uma máquina de estados não bloqueante. O sketch não chama `delay()`, `delayMicroseconds()`, `millis()`, `micros()` ou `tone()`. As bibliotecas e as operações de I2C/EEPROM podem continuar usando esperas internas. Durante movimentos e avisos, o teclado continua sendo varrido, mas novas ações são ignoradas para evitar comandos concorrentes.

O Timer2 fica reservado ao firmware: não use simultaneamente bibliotecas que o reconfigurem ou PWM por hardware nos pinos D3/D11. A pinagem do circuito permanece a mesma.

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
