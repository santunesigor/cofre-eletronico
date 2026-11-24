"""Confirma pela serial que o Arduino Uno apagou os 1.024 bytes."""

import os
import select
import sys
import termios
import time
import tty


def verificar(porta):
    fd = os.open(porta, os.O_RDWR | os.O_NOCTTY)
    try:
        tty.setraw(fd)
        config = termios.tcgetattr(fd)
        config[4] = config[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, config)
        termios.tcflush(fd, termios.TCIFLUSH)
        esperado = b"EEPROM bytes FF: 1024/1024"
        dados = b""
        limite = time.monotonic() + 15
        while time.monotonic() < limite:
            os.write(fd, b"?")
            if select.select([fd], [], [], 0.4)[0]:
                dados += os.read(fd, 4096)
                linhas = dados.split(b"\n")
                dados = linhas.pop()
                if any(linha.rstrip(b"\r") == esperado for linha in linhas):
                    print(esperado.decode())
                    return True
    finally:
        os.close(fd)
    return False


if __name__ == "__main__" and not verificar(sys.argv[1]):
    raise SystemExit("Tempo esgotado ao verificar a EEPROM pela serial.")
