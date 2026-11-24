"""Cria uma chave de recuperação privada na pasta do sketch."""

import os
from pathlib import Path
import secrets


def main():
    arquivo = Path(__file__).resolve().parents[1] / "chave_dispositivo.h"
    chave = f"{secrets.randbelow(10**16):016d}"
    try:
        fd = os.open(arquivo, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        raise SystemExit("Já existe chave_dispositivo.h. Use uma cópia separada para outra unidade.")
    with os.fdopen(fd, "w") as saida:
        saida.write(f'#define CHAVE_DISPOSITIVO "{chave}"\n')
    print("chave_dispositivo.h criado. Use-o apenas na compilação desta unidade.")


if __name__ == "__main__":
    main()
