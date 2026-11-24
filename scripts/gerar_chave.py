"""Cria um código de recuperação privado na pasta do sketch."""

import os
from pathlib import Path
import secrets


def gerar_cabecalho(arquivo):
    chave = f"{secrets.randbelow(10**16):016d}"  # Inclui chaves que começam por zero.
    try:
        # O_EXCL evita sobrescrever um código; 0600 restringe o arquivo ao dono.
        fd = os.open(arquivo, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        raise SystemExit("Já existe chave_dispositivo.h. Use uma cópia separada para outra unidade.")
    with os.fdopen(fd, "w") as saida:
        # O compilador lê este valor. O script informa só o nome do arquivo ao operador.
        saida.write(f'#define CHAVE_DISPOSITIVO "{chave}"\n')
    print("Código de recuperação gerado no cabeçalho privado.", flush=True)


if __name__ == "__main__":
    # Se usado à parte, crie o cabeçalho na pasta do sketch.
    gerar_cabecalho(Path(__file__).resolve().parents[1] / "chave_dispositivo.h")
