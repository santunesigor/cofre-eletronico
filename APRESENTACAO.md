# Roteiro de apresentação do cofre

## Antes da aula

- Separe o cofre, o cabo USB, a alimentação adequada ao servo e o computador. Confira se a trava está fisicamente fechada e alinhada.
- Com o cofre parado, confirme que o LCD mostra o texto esperado e que o teclado e o buzzer respondem. Faça pelo menos uma abertura e um fechamento completos. Se o servo se mover parado ou fizer força no fim do percurso, calibre com a trava desacoplada conforme o README.
- Tenha o código de recuperação anotado em papel. Se quiser mostrar o primeiro uso ao professor, use um código e um PIN exclusivos da demonstração. Em um cofre que guarda dados reais, apresente o primeiro uso sem revelar o código ou o PIN.

## Demonstração de cinco minutos

| Etapa | O que fazer | O que explicar |
| --- | --- | --- |
| 1. Primeiro uso | Se o cofre estiver reinicializado, retire o adesivo, anote o código de 16 dígitos e pressione **Confirmar** três vezes. Em `Defina seu PIN`, escolha quatro números, confirme, repita o PIN e confirme. | O código serve para recuperação e permanece na EEPROM. O usuário escolhe o PIN que será usado no dia a dia. |
| 2. Abrir e consultar | Digite o PIN e confirme. Aguarde os dois bipes. Pressione `0` para o histórico, `2` para voltar e `8` para avançar. Saia com **Limpar** e feche com **Confirmar**. | Teclado, LCD, servo e buzzer têm papéis distintos. O histórico registra os eventos, sem guardar o que foi digitado. |
| 3. Provocar o bloqueio | Digite um PIN incorreto três vezes, confirmando e aguardando o aviso a cada tentativa. | Erros completos, bloqueio e reinício ficam registrados. Reiniciar a placa não libera a digitação do PIN. |
| 4. Recuperar | Em `Cod. recuperacao`, digite os 16 números anotados e confirme. Escolha um PIN diferente do anterior, confirme, repita e confirme. | O visor mostra um asterisco por dígito. Depois de salvar o novo PIN, o cofre volta à tela de entrada. |
| 5. Encerrar | Abra com o PIN, consulte o histórico e mostre o evento `Desbloqueado`. Saia do histórico e feche o cofre. | A EEPROM preserva o PIN, o bloqueio e os últimos 120 eventos após desligar. |

## Como explicar o firmware

Abra `CofreEletronico/CofreEletronico.ino` e siga esta ordem. Use os nomes das funções para localizar os trechos no editor.

1. **`setup()` e `loop()`.** O primeiro carrega a EEPROM, configura os periféricos e inicia o Timer2. Depois, cada volta do laço atende teclado, avisos, servo e recuperação. `Modo` indica qual operação de PIN está em curso e `Fase` indica o que está aguardando um intervalo.
2. **`configurarTimer()` e `atualizarFase()`.** A 16 MHz, o Timer2 divide o relógio por 8 e conta de 0 a 249. Isso dá 8.000 interrupções por segundo, oito por milissegundo. Alternar o pino do buzzer a cada interrupção gera 4 kHz. O firmware agenda o próximo passo com a diferença entre dois valores do contador. O Timer1 fica com a biblioteca Servo.
3. **`lerTeclado()` e `lerPressionamento()`.** As 12 teclas usam quatro linhas e três colunas. O programa ativa uma linha por vez e aguarda a leitura ficar estável por 40 ms. A combinação **Limpar + Confirmar** por cinco segundos entra na troca de PIN.
4. **`carregarChave()` e `confirmarSenha()`.** O primeiro uso tem três estados salvos na EEPROM. O cofre mostra o código, pede um PIN de quatro dígitos e a confirmação dele e só então libera o uso comum. Se faltar energia na tela de PIN, o cadastro continua pendente e o código permanece oculto.
5. **`confirmarCodigo()` e `confirmarNovoPin()`.** O código de 16 dígitos autoriza escolher um novo PIN. A primeira entrada do PIN fica em RAM. Só quando as duas entradas coincidem ele vai para a EEPROM e o bloqueio é liberado.
6. **`registrarEvento()` e `carregarHistorico()`.** São 120 posições de 8 bytes. Ao lotar, o próximo evento ocupa a posição mais antiga. Cada registro tem sequência, tipo, CRC e um marcador salvo por último. Assim, uma escrita interrompida pode invalidar uma posição sem perder as outras.

Se o professor perguntar sobre as limitações, explique que o servo é acionado por tempo e o circuito não confirma fisicamente a posição da porta. O histórico ordena os eventos por sequência, pois não há relógio de calendário. O Uno também permite acesso físico à EEPROM. É um protótipo didático.

## Mapa dos arquivos

| Arquivo | O que procurar |
| --- | --- |
| [CofreEletronico.ino](CofreEletronico/CofreEletronico.ino) | Código que roda continuamente no Arduino. Os comentários explicam o Timer2, as prioridades do LCD, o bloqueio, a confirmação do PIN e a organização da memória. |
| [diagram.json](diagram.json) e [libraries.txt](libraries.txt) | Configuração do Wokwi. `parts` lista as peças, `attrs` descreve propriedades como o endereço do LCD e `connections` liga dois pinos pelo identificador da peça. A lista de bibliotecas indica as dependências do simulador. O JSON usa o formato que o Wokwi lê diretamente. |
| [LimparEEPROM.ino](LimparEEPROM/LimparEEPROM.ino) | Sketch temporário aberto na IDE para apagar os dados. Envie `?` no Monitor Serial a 115200 baud para conferir a limpeza. |
| [tests/test_senha.py](tests/test_senha.py) | Monta um Arduino simulado em C++, chama as funções do sketch e compara a tela, a memória e os pulsos do servo com o esperado. |
| [arduino-build.yml](.github/workflows/arduino-build.yml) e [release-v1.yml](.github/workflows/release-v1.yml) | O primeiro valida o diagrama, roda os testes e compila no GitHub Actions. O segundo registra a publicação da versão histórica v1.0.0. |

## Regravar por conta própria

Siga o [roteiro da Arduino IDE no README](README.md#como-compilar-e-gravar-na-arduino-ide). Se quiser mostrar o primeiro uso, grave `LimparEEPROM.ino`, confirme `1024/1024` no Monitor Serial e depois grave `CofreEletronico.ino`. O Uno gera o novo código sem precisar do computador depois de gravado.

| Se acontecer | Faça isto |
| --- | --- |
| A porta sumiu | Reconecte o cabo USB de dados e selecione novamente **Ferramentas → Porta**. Feche o Monitor Serial antes de carregar. |
| Sem permissão no Linux | Execute `sudo setfacl -m "u:$(id -un):rw" /dev/ttyACM1`, usando a porta mostrada no seu computador. A permissão dura até desconectar a placa. |
| O LCD mostrar `Falha na geracao` | Pressione **Confirmar** no Uno físico. O firmware tenta coletar novas amostras dos relógios. |
| O LCD mostrar `Bloqueado 30s` | Aguarde 30 segundos. O cofre exige essa espera após reiniciar bloqueado ou informar um código errado. |

Para um ensaio mais longo, use o [guia de controles e testes](GUIA_TESTES.md). Para mostrar a fiação, abra [o diagrama](diagram.json) no Wokwi ou consulte a tabela de pinagem no [README](README.md).
