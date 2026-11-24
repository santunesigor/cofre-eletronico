#!/usr/bin/env bash
# Uso no Linux/macOS: ./scripts/gravar.sh [porta]
# No Windows, rode: py -3 scripts/gravar.py [porta]
# Sem informar a porta, o Python procura um único Uno compatível.
# A gravação apaga toda a EEPROM. Use --portas para só conferir as portas.
scripts=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || exit
exec python3 "$scripts/gravar.py" "$@"
