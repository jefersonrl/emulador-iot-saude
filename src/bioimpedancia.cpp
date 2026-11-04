// Balança de bioimpedância BLE: peso, IMC, gordura (fórmula de Deurenberg), água, músculo e impedância
#define TIPO             "BIO"
#define PREFIXO          "Balanca"
#define INTERVALO_PADRAO 30
#include "ble_saude.h"

static float peso = 72.0;
static const float ALTURA = 1.70; static const int IDADE = 35; static const int SEXO = 1;  // 1 = masc

String gerarLeitura() {
  float kg = passo(peso, 0.2, 60, 90);
  float imc = kg / (ALTURA * ALTURA);
  float gord = 1.2 * imc + 0.23 * IDADE - 10.8 * SEXO - 5.4 + rnd(-0.5, 0.5);
  float agua = (100 - gord) * 0.73;
  float musc = kg * (1 - gord / 100) * 0.95;
  int imp = 420 + gord * 4 + rnd(-10, 10);  // impedância em ohms
  char b[160];
  snprintf(b, sizeof b,
    "\"disp\":\"bioimpedancia\",\"peso\":%.1f,\"imc\":%.1f,\"gordura\":%.1f,\"agua\":%.1f,\"musculo\":%.1f,\"imp\":%d",
    kg, imc, gord, agua, musc, imp);
  return b;
}
