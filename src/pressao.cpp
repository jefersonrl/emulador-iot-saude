// Medidor de pressão arterial BLE: sistólica, diastólica e pulso
#define TIPO             "PA"
#define PREFIXO          "Pressao"
#define INTERVALO_PADRAO 20
#include "ble_saude.h"

static float sis = 118, dia = 77, fc = 72;

String gerarLeitura() {
  int s = passo(sis, 4, 100, 145);
  int d = passo(dia, 3, 60, 95);
  if (d > s - 25) d = s - 25;
  int p = passo(fc, 3, 55, 100);
  char b[100];
  snprintf(b, sizeof b, "\"disp\":\"pressao\",\"sis\":%d,\"dia\":%d,\"pulso\":%d,\"un\":\"mmHg\"", s, d, p);
  return b;
}
