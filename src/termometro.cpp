// Termômetro BLE: temperatura entre 35,9 e 37,3 °C, com febre ocasional
#define TIPO             "TEMP"
#define PREFIXO          "Termometro"
#define INTERVALO_PADRAO 10
#include "ble_saude.h"

static float temp = 36.6;

String gerarLeitura() {
  float c = passo(temp, 0.15, 35.9, 37.3);
  if (rnd(0, 1) < 0.04) c = rnd(37.8, 39.2);
  char b[80];
  snprintf(b, sizeof b, "\"disp\":\"termometro\",\"temp\":%.1f,\"un\":\"C\"", c);
  return b;
}
