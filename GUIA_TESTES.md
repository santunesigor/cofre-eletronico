# Guia de controles e testes do cofre

Use este roteiro para executar os testes no Wokwi ou no circuito.

## Preparação

1. No Wokwi, copie `sketch.ino`, `diagram.json` e `libraries.txt`. Adicione um `chave_dispositivo.h` gerado com `python3 scripts/gerar_chave.py` em uma cópia separada do projeto para a simulação.
2. Em EEPROM vazia, o LCD mostra a chave. Anote os 16 dígitos e pressione **Confirmar** três vezes, soltando o botão entre as pressões. Para testar a persistência, reinicie após a segunda pressão e verifique que a chave reaparece. Depois, pressione três vezes e reinicie para verificar que a chave continua oculta.
3. Em `Defina seu PIN`, escolha quatro dígitos e pressione **Confirmar**. Para testar a persistência do cadastro, reinicie antes de salvar e verifique que o LCD volta a pedir o PIN com a chave oculta. Cadastre o PIN e reinicie novamente. O LCD deve mostrar `Digite a senha` e aceitar o PIN escolhido.
4. No circuito físico, calibre o servo com a trava desacoplada antes dos testes. O Wokwi usa servo posicional e não valida o percurso do servo de rotação contínua.

Use apenas uma chave de teste no simulador. Guarde a chave real e o binário da unidade em local privado. Execute as etapas de bloqueio tendo a chave cadastrada disponível.

## Controles

| Situação | Comando | Resultado |
| --- | --- | --- |
| Digitar PIN | `0` a `9` | Aceita até quatro números; mostra asteriscos. |
| Limpar entrada | **Limpar** (`X` no Wokwi) | Apaga toda a entrada. |
| Abrir | PIN atual + **Confirmar** (`Z`) | Move o servo e toca dois bipes curtos. |
| Fechar | **Confirmar**, na tela `Para fechar:` | Move o servo no sentido contrário. |
| Trocar PIN | Cofre fechado: segurar **Limpar + Confirmar** (`X + Z`) por pelo menos 5 segundos e soltar ambos | Pede o PIN atual; depois, o novo PIN, confirmando cada um. |
| Abrir histórico | `0`, depois de abrir com o PIN | Mostra o evento mais recente. |
| Navegar no histórico | `2` / `8` | Evento mais antigo / mais recente. |
| Sair do histórico | **Limpar** ou **Confirmar** | Volta à tela de fechamento; não fecha nessa mesma pressão. |
| Sair do histórico por tempo | Esperar 30 segundos sem navegar | Volta à tela de fechamento. |
| Recuperar bloqueio | Contrassenha atual de seis números + **Confirmar** | Libera o PIN normal; para abrir, ainda é necessário digitar o PIN. |
| Rever ID durante recuperação | **Limpar** | Apaga a contrassenha e volta à alternância de aviso e ID. |

Solte o botão entre pressões. Uma tecla mantida pressionada gera apenas uma entrada. Durante movimento, bipes, avisos e espera de recuperação, novos comandos são ignorados; aguarde a tela ficar pronta.

## Bateria manual

Execute na ordem. Use `P` para o PIN atual, `N` para um PIN temporário de quatro dígitos diferente de `P`, e `E` para um PIN incorreto. Anote o novo PIN durante a troca. Não use o valor `E` se ele coincidir com o PIN salvo.

| Nº | Ação | Resultado esperado |
| --- | --- | --- |
| 1 | Iniciar com o cofre fechado e a chave cadastrada. | LCD mostra `Digite a senha`; servo recebe parada, sem percurso de abertura. |
| 2 | Digitar dois números e confirmar. Esperar o aviso acabar. | `Digite 4 numeros`; os dois números permanecem para completar a entrada. Limpar antes do próximo teste. |
| 3 | Segurar um número por 1 segundo; soltar e digitar até cinco números ao todo. | A tecla segurada conta uma vez; entrada limitada a quatro asteriscos. **Limpar** remove todos. |
| 4 | Digitar `E` e confirmar uma vez. | `Senha incorreta`, buzzer por 1,5 segundo, cofre fechado e entrada limpa após cerca de 1,7 segundo. |
| 5 | Digitar `P` e confirmar; aguardar os bipes. | Servo abre por 500 ms, recebe parada e toca dois bipes. LCD: `Para fechar:` / `Aperte Confirmar`. A autenticação zera os erros anteriores. |
| 6 | Com o cofre aberto, pressionar números diferentes de `0` e **Limpar**. | Cofre permanece aberto e não recebe entrada de PIN. |
| 7 | Pressionar `0`, navegar com `2` e `8`, sair com **Confirmar**. | Exibe eventos, respeita os limites da lista e retorna à tela de fechamento mantendo o cofre aberto. Pressionar **Confirmar** novamente fecha. |
| 8 | Abrir com `P`, entrar no histórico e aguardar 30 segundos sem navegar. Depois fechar. | Histórico sai sozinho; fechamento só ocorre pelo comando na tela de fechamento. |
| 9 | Fechado, segurar a combinação de troca por menos de 5 segundos e soltar. Repetir por mais de 5 segundos e soltar ambos. | Primeiro não inicia troca; depois mostra `Senha atual:`. Soltar um botão antes do outro não deve confirmar entrada involuntariamente. |
| 10 | Na troca, informar `P`, confirmar, informar `N` e confirmar. Reiniciar com o cofre fechado. | `Senha alterada!`; `N` continua válido após reiniciar. Abrir com `N` e fechar; usar `N` como PIN atual nos próximos testes. |
| 11 | Abrir com o PIN atual, reiniciar e pressionar `0`. | Recupera estado aberto sem refazer o percurso; histórico não abre. Fechar, autenticar novamente e pressionar `0`: histórico disponível e eventos anteriores preservados. Sair e fechar. |
| 12 | Informar três PINs completos incorretos consecutivos, aguardando cada aviso. | Após o terceiro, bloqueia. LCD alterna mensagens rolantes e `ID:` com 12 caracteres hexadecimais, mostrado estaticamente por 6 segundos. |
| 13 | Bloqueado, digitar um número e depois **Limpar**. Aguardar a mudança de minuto do desafio. | Primeiro mostra `Contrassenha:` com asterisco; limpar volta aos avisos/ID. A cada janela de 60 segundos o ID muda e a entrada de recuperação é apagada. |
| 14 | Ainda bloqueado, reiniciar e esperar. | Bloqueio persiste; mostra `Bloqueado 30s`, ignora comandos durante a espera e depois gera novo ID. |
| 15 | No calculador, informar a chave cadastrada e o ID atual, calcular e digitar os seis números no cofre. Confirmar antes de o ID mudar. | Volta a `Digite a senha` sem abrir o servo. PIN atual + **Confirmar** abre normalmente. |
| 16 | Abrir o histórico após a recuperação. | Contém eventos de erro, bloqueio, inicialização, desbloqueio e abertura, conforme as ações realizadas. Não exibe PIN nem chave. |

Teste adicional da espera por erro de recuperação: bloqueie novamente com três PINs errados; calcule a contrassenha atual e altere seu último dígito antes de enviá-la, dentro da mesma janela. Deve aparecer `Bloqueado 30s`; após a espera, use o novo ID para recuperar corretamente. Um ID vencido não serve como referência para esse teste.

No final, feche o cofre. Se desejar restaurar o PIN anterior, faça a troca autenticada e valide uma abertura e um fechamento.

## Testes automatizados existentes

Na raiz do repositório:

```bash
python3 tests/test_senha.py
node tests/test_scanner.cjs
```

- **Firmware:** requer Python 3 e compilador C++ (`g++` por padrão, ou outro informado em `CXX`). Compila o sketch com substitutos das APIs Arduino e verifica teclado, temporização, PIN, EEPROM, servo, bipes, recuperação e histórico circular. Inclui capacidade de 120 registros, CRC e simulação de gravação interrompida.
- **Calculador/scanner:** requer Node.js; não precisa instalar as dependências OCR para esse teste. Verifica extração de ID, rejeição de leituras ambíguas, vetor de referência SipHash e geração de código com seis dígitos. Não testa câmera nem reconhecimento de imagem real.
- A compilação específica para Arduino Uno está definida em `.github/workflows/arduino-build.yml`; executar o teste de firmware no computador não substitui essa compilação.

## Teste manual do scanner (opcional)

Prepare os arquivos e sirva a pasta:

```bash
npm install
npm run prepare:ocr
python3 -m http.server 8000
```

No computador, abra `http://localhost:8000/recuperacao.html`. Primeiro valide a digitação manual do ID. Depois, com o cofre bloqueado e exibindo `ID:`, teste **Escanear LCD com a câmera** e **Ler uma foto do LCD**. Informe a chave cadastrada, enquadre a primeira linha e confira se o ID reconhecido coincide com o LCD e se o código libera a digitação do PIN. A câmera espera duas capturas consecutivas com o mesmo ID.

Para celular, use o pacote com a pasta `ocr/` em HTTPS, conforme o README; HTTP pelo IP local pode impedir a câmera. A validação física ainda precisa conferir contraste do LCD, foco, iluminação, alimentação e percurso da trava. O firmware não tem sensor para confirmar a posição real da porta.
