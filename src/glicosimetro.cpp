// Glicosímetro BLE: glicose entre 70 e 140 mg/dL, com picos ocasionais (pós refeição)
#define TIPO             "GLI"
#define PREFIXO          "Glicosimetro"
#define INTERVALO_PADRAO 15
#include "ble_saude.h"

static float glic = 95;

String gerarLeitura() {
  int g = passo(glic, 6, 70, 140);
  if (rnd(0, 1) < 0.05) g = rnd(160, 240);
  char b[80];
  snprintf(b, sizeof b, "\"disp\":\"glicosimetro\",\"glicose\":%d,\"un\":\"mg/dL\"", g);
  return b;
}
