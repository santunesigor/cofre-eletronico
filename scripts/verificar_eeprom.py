"""Lê do Arduino a confirmação dos 1.024 bytes apagados."""

import time


def verificar(porta):
    from serial import Serial, SerialException

    try:
        # Use o mesmo baud rate do sketch LimparEEPROM e feche a porta ao sair.
        with Serial(porta, 115200, timeout=0.3, write_timeout=1) as conexao:
            conexao.reset_input_buffer()
            dados = b""
            limite = time.monotonic() + 15
            while time.monotonic() < limite:
                # Abrir a porta pode reiniciar o Uno. Consulte até ele responder.
                conexao.write(b"?")
                dados += conexao.read(128)
                linhas = dados.split(b"\n")
                dados = linhas.pop()  # O fim da linha pode chegar na leitura seguinte.
                if any(linha.rstrip(b"\r") == b"EEPROM bytes FF: 1024/1024" for linha in linhas):
                    print("EEPROM bytes FF: 1024/1024")
                    return True
    except SerialException as erro:
        print(f"Não foi possível verificar a EEPROM pela serial: {erro}")
        return False
    print("Tempo esgotado ao verificar a EEPROM pela serial.")
    return False
