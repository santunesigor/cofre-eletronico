"""Grava o cofre em um Arduino Uno R3 (ATmega328P).

Conecte o Uno, feche o Monitor Serial e execute na raiz do projeto:
  Linux/macOS: python3 scripts/gravar.py
  Windows:     py -3 scripts/gravar.py

Use --portas para listar as portas ou passe uma porta, como COM3 ou /dev/ttyACM1.
Prepare o computador uma vez seguindo o README. A gravação apaga toda a EEPROM.
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from gerar_chave import gerar_cabecalho
from verificar_eeprom import verificar

SCRIPTS = Path(__file__).resolve().parent
PLACA = "arduino:avr:uno"
# Identificadores USB do Uno AVR e do modelo usado no projeto.
# Eles identificam o modelo mesmo depois de mudar de conector ou de computador.
USB_UNO = {
    (0x2341, 0x0043), (0x2341, 0x0001), (0x2A03, 0x0043),
    (0x2341, 0x0243), (0x2341, 0x006A), (0x0843, 0x5740),
}


def mostrar_portas(portas):
    for porta in portas:
        modelo = "Arduino Uno" if (porta.vid, porta.pid) in USB_UNO else porta.description
        print(f"  {porta.device} — {modelo or 'porta serial'}")
    if not portas:
        print("Nenhuma porta USB serial encontrada.")


def gravar(cli, porta):
    def arduino(*argumentos):
        subprocess.run([cli, *argumentos], check=True)

    # O cabeçalho e o binário contêm o código. Guarde-os só na pasta temporária.
    with tempfile.TemporaryDirectory(prefix="cofre-") as tmp:
        pasta = Path(tmp)
        cofre = pasta / "Cofre"
        limpeza = SCRIPTS / "LimparEEPROM"
        cofre.mkdir()
        shutil.copyfile(SCRIPTS.parent / "sketch.ino", cofre / "Cofre.ino")

        print("1/3 Gerando o código de recuperação e compilando.", flush=True)
        gerar_cabecalho(cofre / "chave_dispositivo.h")
        biblioteca = os.environ.get("ARDUINO_SERVO_LIB")
        opcao_servo = ["--library", biblioteca] if biblioteca else []
        # Compile os dois sketches antes de modificar a placa.
        for sketch, opcoes in ((cofre, opcao_servo), (limpeza, [])):
            arduino("compile", "--fqbn", PLACA, *opcoes,
                    "--build-path", pasta / f"build-{sketch.name}",
                    "--output-dir", pasta / "hex" / sketch.name, sketch)

        def enviar(sketch):
            # --verify pede ao Arduino CLI que leia a flash e confira a gravação.
            arquivo = pasta / "hex" / sketch.name / f"{sketch.name}.ino.hex"
            arduino("upload", "--fqbn", PLACA, "--port", porta, "--verify",
                    "--input-file", arquivo, sketch)

        print("2/3 Apagando e verificando a EEPROM.", flush=True)
        enviar(limpeza)
        eeprom_limpa = verificar(porta)

        # Mesmo se a confirmação serial falhar, tire o programa auxiliar da placa.
        print("3/3 Gravando e verificando o firmware do cofre.", flush=True)
        enviar(cofre)
        if not eeprom_limpa:
            raise SystemExit("Firmware gravado, mas a limpeza da EEPROM não foi confirmada.")
        print("EEPROM limpa e firmware gravado e verificado.")


def main():
    parser = argparse.ArgumentParser(description="Grava o cofre no Uno e apaga toda a EEPROM.")
    parser.add_argument("porta", nargs="?", help="porta USB, por exemplo COM3 ou /dev/ttyACM1")
    parser.add_argument("--portas", action="store_true", help="lista as portas USB sem gravar")
    args = parser.parse_args()
    try:
        from serial.tools.list_ports import comports
    except ImportError:
        raise SystemExit("Instale o pySerial conforme o README: python -m pip install -r requirements.txt")

    portas_usb = []
    if args.portas or not args.porta:
        portas_usb = sorted(porta for porta in comports() if porta.vid is not None)
    if args.portas:
        mostrar_portas(portas_usb)
        return
    porta = args.porta
    if not porta:
        unos = [p.device for p in portas_usb if (p.vid, p.pid) in USB_UNO]
        # Só escolha automaticamente se houver um único Uno reconhecido.
        if len(unos) != 1:
            mostrar_portas(portas_usb)
            raise SystemExit("Conecte um único Uno ou informe a porta como argumento.")
        porta = unos[0]

    if os.name == "posix" and not os.access(porta, os.R_OK | os.W_OK):
        raise SystemExit(f"Sem acesso à porta {porta}. Confira a conexão e as permissões.")
    cli = shutil.which(os.environ.get("ARDUINO_CLI", "arduino-cli"))
    if not cli:
        raise SystemExit("Arduino CLI não encontrado. Instale-o e confira o PATH conforme o README.")
    print(f"Arduino Uno na porta {porta}. A gravação apaga toda a EEPROM.", flush=True)
    try:
        gravar(cli, porta)
    except subprocess.CalledProcessError:
        raise SystemExit("O Arduino CLI encontrou um erro. Confira a mensagem acima e o README.")


if __name__ == "__main__":
    main()
