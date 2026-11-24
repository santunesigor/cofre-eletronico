# Guia de controles e testes do cofre

Use este roteiro para executar os testes no Wokwi ou no circuito.

## Preparação

1. Para testar o primeiro uso, grave os dois sketches pela Arduino IDE na ordem descrita no [README](README.md#começar-do-zero-pela-ide). Use o Wokwi para conferir a fiação. A geração no firmware depende da variação entre os relógios reais.
2. Em EEPROM vazia, aguarde `Gerando codigo`. O LCD passa a mostrar o código. Anote os 16 dígitos e pressione **Confirmar** três vezes, soltando o botão entre as pressões. Para testar a persistência, reinicie após a segunda pressão e verifique que o código reaparece. Depois, pressione três vezes e reinicie para verificar que o código continua oculto.
3. Em `Defina seu PIN`, escolha quatro dígitos e pressione **Confirmar**. Repita o PIN e confirme para salvar. Para testar a persistência do cadastro, reinicie antes de salvar e verifique que o LCD volta a pedir o PIN com o código oculto. Cadastre o PIN e reinicie novamente. O LCD deve mostrar `Digite a senha` e aceitar o PIN escolhido.
4. No circuito físico, calibre o servo com a trava desacoplada antes dos testes. O Wokwi usa servo posicional e não valida o percurso do servo de rotação contínua.

Use um código de demonstração para ensaiar. Guarde o código real em local privado. Execute as etapas de bloqueio tendo o código cadastrado disponível.

## Controles

| Situação | Comando | Resultado |
| --- | --- | --- |
| Digitar PIN | `0` a `9` | Aceita até quatro números; mostra asteriscos. |
| Limpar entrada | **Limpar** (`X` no Wokwi) | Apaga toda a entrada. |
| Abrir | PIN atual + **Confirmar** (`Z`) | Move o servo e toca dois bipes curtos. |
| Fechar | **Confirmar**, na tela `Para fechar:` | Move o servo no sentido contrário. |
| Trocar PIN | Cofre fechado: segurar **Limpar + Confirmar** (`X + Z`) por pelo menos 5 segundos e soltar ambos | Pede o PIN atual; depois, o novo PIN e a repetição dele. |
| Abrir histórico | `0`, depois de abrir com o PIN | Mostra o evento mais recente. |
| Navegar no histórico | `2` / `8` | Evento mais antigo / mais recente. |
| Sair do histórico | **Limpar** ou **Confirmar** | Volta à tela de fechamento; não fecha nessa mesma pressão. |
| Sair do histórico por tempo | Esperar 30 segundos sem navegar | Volta à tela de fechamento. |
| Recuperar bloqueio | Código de 16 dígitos + **Confirmar** | Permite escolher e confirmar um novo PIN. |
| Limpar código em digitação | **Limpar** | Apaga os asteriscos. |

Solte o botão entre pressões. Uma tecla mantida pressionada gera apenas uma entrada. Durante movimento, bipes, avisos e espera de recuperação, novos comandos são ignorados; aguarde a tela ficar pronta.

## Bateria manual

Execute na ordem. Use `P` para o PIN atual, `N` para um PIN temporário de quatro dígitos diferente de `P`, e `E` para um PIN incorreto. Anote o novo PIN durante a troca. Não use o valor `E` se ele coincidir com o PIN salvo.

| Nº | Ação | Resultado esperado |
| --- | --- | --- |
| 1 | Iniciar com o cofre fechado e o código cadastrado. | LCD mostra `Digite a senha`; servo recebe parada, sem percurso de abertura. |
| 2 | Digitar dois números e confirmar. Esperar o aviso acabar. | `Digite 4 numeros`; os dois números permanecem para completar a entrada. Limpar antes do próximo teste. |
| 3 | Segurar um número por 1 segundo; soltar e digitar até cinco números ao todo. | A tecla segurada conta uma vez; entrada limitada a quatro asteriscos. **Limpar** remove todos. |
| 4 | Digitar `E` e confirmar uma vez. | `Senha incorreta`, buzzer por 1,5 segundo, cofre fechado e entrada limpa após cerca de 1,7 segundo. |
| 5 | Digitar `P` e confirmar; aguardar os bipes. | Servo abre por 500 ms, recebe parada e toca dois bipes. LCD: `Para fechar:` / `Aperte Confirmar`. A autenticação zera os erros anteriores. |
| 6 | Com o cofre aberto, pressionar números diferentes de `0` e **Limpar**. | Cofre permanece aberto e não recebe entrada de PIN. |
| 7 | Pressionar `0`, navegar com `2` e `8`, sair com **Confirmar**. | Exibe eventos, respeita os limites da lista e retorna à tela de fechamento mantendo o cofre aberto. Pressionar **Confirmar** novamente fecha. |
| 8 | Abrir com `P`, entrar no histórico e aguardar 30 segundos sem navegar. Depois fechar. | Histórico sai sozinho; fechamento só ocorre pelo comando na tela de fechamento. |
| 9 | Fechado, segurar a combinação de troca por menos de 5 segundos e soltar. Repetir por mais de 5 segundos e soltar ambos. | Primeiro não inicia troca; depois mostra `Senha atual:`. Soltar um botão antes do outro não deve confirmar entrada involuntariamente. |
| 10 | Na troca, informar `P`, confirmar, informar `N`, confirmar, repetir `N` e confirmar. Reiniciar com o cofre fechado. | `PIN cadastrado!`; `N` continua válido após reiniciar. Abrir com `N` e fechar; usar `N` como PIN atual nos próximos testes. |
| 11 | Abrir com o PIN atual, reiniciar e pressionar `0`. | Recupera estado aberto sem refazer o percurso; histórico não abre. Fechar, autenticar novamente e pressionar `0`: histórico disponível e eventos anteriores preservados. Sair e fechar. |
| 12 | Informar três PINs incorretos consecutivos, aguardando cada aviso. | LCD mostra `Cod. recuperacao`. |
| 13 | Digitar os 16 números do código anotado, sem confirmar. Pressionar **Limpar**. | O LCD mostra os 16 asteriscos. **Limpar** apaga todos. |
| 14 | Digitar um código de 16 dígitos incorreto e confirmar. Reiniciar enquanto bloqueado. | O LCD mostra `Bloqueado 30s` após o erro e após reiniciar. |
| 15 | Digitar o código correto, escolher um novo PIN e repetir com um dígito diferente. Em seguida, escolher e confirmar duas entradas iguais. | A diferença reinicia a escolha. O PIN igual é salvo, libera o bloqueio e mantém o cofre fechado. |
| 16 | Abrir com o novo PIN e consultar o histórico. | Registro `Desbloqueado` disponível. Nenhum PIN ou código aparece no histórico. |

No final, feche o cofre. Se desejar restaurar o PIN anterior, faça a troca autenticada e valide uma abertura e um fechamento.

## Testes automatizados existentes

Na raiz do repositório:

```bash
python3 tests/test_senha.py
```

- **Firmware:** requer Python 3 e compilador C++ (`g++` por padrão, ou outro informado em `CXX`). Compila o sketch com substitutos das APIs Arduino e verifica teclado, temporização, PIN, EEPROM, servo, bipes, recuperação e histórico circular. Inclui capacidade de 120 registros, CRC e simulação de gravação interrompida.
- A compilação específica para Arduino Uno está definida em `.github/workflows/arduino-build.yml`.
