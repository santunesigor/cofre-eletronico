#!/usr/bin/env bash
set -euo pipefail
umask 077

if [[ $# -ne 1 || ${1:-} == --help ]]; then
  printf 'Uso: %s /dev/ttyACM0\n' "$0"
  printf 'Gera uma chave nova e apaga toda a EEPROM antes de gravar.\n'
  if [[ ${1:-} == --help ]]; then exit 0; fi
  exit 2
fi

port=$1
if [[ ! -c "$port" || ! -r "$port" || ! -w "$port" ]]; then
  printf 'Porta serial indisponível: %s\n' "$port" >&2
  exit 1
fi

scripts=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cli=${ARDUINO_CLI:-arduino-cli}
board=arduino:avr:uno
build_dir=$(mktemp -d)
trap 'rm -rf -- "$build_dir"' EXIT

mkdir -p "$build_dir/Cofre/scripts"
cp -- "$scripts/../sketch.ino" "$build_dir/Cofre/Cofre.ino"
cp -- "$scripts/gerar_chave.py" "$build_dir/Cofre/scripts/"
python3 "$build_dir/Cofre/scripts/gerar_chave.py"

libraries=()
if [[ -n ${ARDUINO_SERVO_LIB:-} ]]; then
  libraries=(--library "$ARDUINO_SERVO_LIB")
fi
"$cli" compile --fqbn "$board" "${libraries[@]}" \
  --build-path "$build_dir/build-cofre" --output-dir "$build_dir/firmware" "$build_dir/Cofre"
"$cli" compile --fqbn "$board" \
  --build-path "$build_dir/build-limpeza" --output-dir "$build_dir/limpeza" "$scripts/LimparEEPROM"

"$cli" upload --fqbn "$board" --port "$port" --verify \
  --input-file "$build_dir/limpeza/LimparEEPROM.ino.hex" "$scripts/LimparEEPROM"

# Restaura o firmware mesmo se a verificação pela serial falhar.
verified=0
if python3 "$scripts/verificar_eeprom.py" "$port"; then verified=1; fi
"$cli" upload --fqbn "$board" --port "$port" --verify \
  --input-file "$build_dir/firmware/Cofre.ino.hex" "$build_dir/Cofre"
if [[ $verified -ne 1 ]]; then
  printf 'Firmware gravado, mas a limpeza da EEPROM não foi confirmada.\n' >&2
  exit 1
fi
printf 'EEPROM limpa e firmware gravado e verificado.\n'
