#!/bin/bash
# Grava cada dispositivo de saúde num ESP32 diferente, uma placa por vez.
# Uso: ./gravar.sh                    (os quatro, em sequência)
#      ./gravar.sh termometro pressao (só os escolhidos)
cd "$(dirname "$0")"
PIO="$HOME/.platformio/penv/bin/pio"; PY="$HOME/.platformio/penv/bin/python"
[ -x "$PIO" ] || PIO="$(command -v pio)"
[ -n "$PIO" ] || { echo "PlatformIO não encontrado. Abra o projeto no VS Code uma vez antes."; exit 1; }

ENVS=(glicosimetro termometro pressao bioimpedancia)
[ $# -gt 0 ] && ENVS=("$@")

echo "== Compilando: ${ENVS[*]} =="
ARGS=""; for e in "${ENVS[@]}"; do ARGS="$ARGS -e $e"; done
"$PIO" run $ARGS || { echo "Erro na compilação. Copie a mensagem acima e mande para o Claude."; exit 1; }

nome_ble() {  # reinicia a placa e mostra o nome BLE que ela anuncia
  [ -x "$PY" ] || return
  "$PY" - "$1" <<'P' 2>/dev/null
import sys, time, serial
s = serial.Serial(sys.argv[1], 115200, timeout=0.5)
s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
t = time.time()
while time.time() - t < 6:
    l = s.readline().decode(errors="ignore").strip()
    if "Anunciando" in l:
        print("   >>> " + l.split(".")[0]); break
P
}

for env in "${ENVS[@]}"; do
  while true; do
    echo
    read -r -p "Conecte SOMENTE a placa do [$env] no USB e aperte Enter (ou digite p para pular): " r
    [ "$r" = "p" ] && break
    PORTAS=( $(ls /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.wchusbserial* /dev/cu.usbmodem* 2>/dev/null) )
    if [ ${#PORTAS[@]} -eq 0 ]; then echo "Nenhuma placa encontrada. Confira o cabo e o driver."; continue; fi
    PORTA="${PORTAS[0]}"
    if [ ${#PORTAS[@]} -gt 1 ]; then
      echo "Mais de uma placa conectada:"; i=1; for p in "${PORTAS[@]}"; do echo "  $i) $p"; i=$((i+1)); done
      read -r -p "Número da porta: " n; PORTA="${PORTAS[$((n-1))]}"
    fi
    echo "== Gravando [$env] em $PORTA (se travar em Connecting..., segure o botão BOOT) =="
    if "$PIO" run -e "$env" -t upload --upload-port "$PORTA"; then
      echo "OK: [$env] gravado."; nome_ble "$PORTA"
      echo "   Coloque uma etiqueta nessa placa e desconecte."
      break
    fi
    read -r -p "Falhou. Tentar de novo? (Enter = sim, n = pular): " r; [ "$r" = "n" ] && break
  done
done
echo; echo "Pronto. Abra interface/painel_saude.html no Chrome e use + Bluetooth para cada placa."
