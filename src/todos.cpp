/*
  Emulador de dispositivos de saúde via BLE, ESP32 DevKit V1 (30 pinos)
  Simula: glicosímetro, termômetro, medidor de pressão arterial e balança de bioimpedância.

  Conexão: serviço UART BLE (padrão Nordic UART Service).
  No tablet/celular Positivo (Android), use o app "Serial Bluetooth Terminal"
  ou "nRF Connect", procure "ESP32-Saude" e ative as notificações.

  Comandos (pelo app ou pelo Monitor Serial, 115200):
    LER         envia uma leitura de todos os dispositivos agora
    INT=10      define o intervalo de TODOS os dispositivos em segundos
    GLI=5       intervalo só do glicosímetro (também TEMP, PA, BIO)
    PA=0        desativa o envio automático daquele dispositivo
    STATUS      mostra os intervalos atuais, a versão do firmware e o modelo
*/
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define NOME_BLE "ESP32-Saude"
#define LED_PIN  2
#define FW_VERSAO "2.0.0-fhir"                  // versão 2: painel converte as leituras para HL7 FHIR
#define MODELO    "ESP32 DevKit V1 (emulador)"  // usado no recurso FHIR Device
#define SVC_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_UUID  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // app escreve comandos
#define TX_UUID  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // ESP32 notifica dados

BLEServer *srv;
BLECharacteristic *tx;
volatile bool conectado = false;
String pendente;  // comando recebido por BLE, tratado no loop

const char *ids[4]   = {"GLI", "TEMP", "PA", "BIO"};
uint32_t intervalo[4] = {15, 10, 20, 30};  // segundos (configurável)
uint32_t ultimo[4]    = {0, 0, 0, 0};

// Estado do "paciente" simulado (varia aos poucos, como medidas reais)
float glic = 95, temp = 36.6, sis = 118, dia = 77, fc = 72, peso = 72.0;
const float ALTURA = 1.70; const int IDADE = 35; const int SEXO = 1;  // 1 = masc

float rnd(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }
float passo(float &x, float d, float mn, float mx) { x = constrain(x + rnd(-d, d), mn, mx); return x; }

String gerar(int i) {
  char b[200];
  unsigned long t = millis() / 1000;
  switch (i) {
    case 0: {  // Glicosímetro, com picos ocasionais (pós refeição)
      int g = passo(glic, 6, 70, 140);
      if (rnd(0, 1) < 0.05) g = rnd(160, 240);
      snprintf(b, sizeof b, "{\"disp\":\"glicosimetro\",\"glicose\":%d,\"un\":\"mg/dL\",\"t\":%lu}", g, t);
      break;
    }
    case 1: {  // Termômetro, febre ocasional
      float c = passo(temp, 0.15, 35.9, 37.3);
      if (rnd(0, 1) < 0.04) c = rnd(37.8, 39.2);
      snprintf(b, sizeof b, "{\"disp\":\"termometro\",\"temp\":%.1f,\"un\":\"C\",\"t\":%lu}", c, t);
      break;
    }
    case 2: {  // Pressão arterial e pulso
      int s = passo(sis, 4, 100, 145);
      int d = passo(dia, 3, 60, 95);
      if (d > s - 25) d = s - 25;
      int p = passo(fc, 3, 55, 100);
      snprintf(b, sizeof b, "{\"disp\":\"pressao\",\"sis\":%d,\"dia\":%d,\"pulso\":%d,\"un\":\"mmHg\",\"t\":%lu}", s, d, p, t);
      break;
    }
    default: {  // Balança de bioimpedância (gordura pela fórmula de Deurenberg)
      float kg = passo(peso, 0.2, 60, 90);
      float imc = kg / (ALTURA * ALTURA);
      float gord = 1.2 * imc + 0.23 * IDADE - 10.8 * SEXO - 5.4 + rnd(-0.5, 0.5);
      float agua = (100 - gord) * 0.73;
      float musc = kg * (1 - gord / 100) * 0.95;
      int imp = 420 + gord * 4 + rnd(-10, 10);  // impedância em ohms
      snprintf(b, sizeof b,
        "{\"disp\":\"bioimpedancia\",\"peso\":%.1f,\"imc\":%.1f,\"gordura\":%.1f,\"agua\":%.1f,\"musculo\":%.1f,\"imp\":%d,\"t\":%lu}",
        kg, imc, gord, agua, musc, imp, t);
    }
  }
  return String(b);
}

// Envia por Serial e BLE, quebrando em pedaços do tamanho do MTU negociado
void enviar(const String &s) {
  Serial.println(s);
  if (!conectado) return;
  String m = s + "\n";
  uint16_t mtu = srv->getPeerMTU(srv->getConnId());
  size_t tam = (mtu > 23) ? mtu - 3 : 20;
  for (size_t p = 0; p < m.length(); p += tam) {
    String parte = m.substring(p, p + tam);
    tx->setValue((uint8_t *)parte.c_str(), parte.length());
    tx->notify();
    delay(15);
  }
}

void status() {
  char b[200];
  snprintf(b, sizeof b, "{\"id\":\"" NOME_BLE "\",\"intervalos_s\":{\"GLI\":%u,\"TEMP\":%u,\"PA\":%u,\"BIO\":%u},"
           "\"fw\":\"" FW_VERSAO "\",\"modelo\":\"" MODELO "\"}",
           (unsigned)intervalo[0], (unsigned)intervalo[1], (unsigned)intervalo[2], (unsigned)intervalo[3]);
  enviar(b);
}

void comando(String c) {
  c.trim(); c.toUpperCase();
  if (c.isEmpty()) return;
  if (c == "LER") { for (int i = 0; i < 4; i++) enviar(gerar(i)); return; }
  if (c == "STATUS") { status(); return; }
  int eq = c.indexOf('=');
  if (eq > 0) {
    String k = c.substring(0, eq);
    uint32_t v = c.substring(eq + 1).toInt();
    bool ok = false;
    for (int i = 0; i < 4; i++)
      if (k == "INT" || k == ids[i]) { intervalo[i] = v; ultimo[i] = millis(); ok = true; }
    if (ok) { status(); return; }
  }
  enviar("{\"erro\":\"comando invalido\"}");
}

class ServidorCB : public BLEServerCallbacks {
  void onConnect(BLEServer *s) override { conectado = true; digitalWrite(LED_PIN, HIGH); }
  void onDisconnect(BLEServer *s) override {
    conectado = false; digitalWrite(LED_PIN, LOW);
    s->startAdvertising();  // volta a ficar visível
  }
};

class RxCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override { pendente = c->getValue().c_str(); }
};

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  BLEDevice::init(NOME_BLE);
  BLEDevice::setMTU(185);
  srv = BLEDevice::createServer();
  srv->setCallbacks(new ServidorCB());

  BLEService *svc = srv->createService(SVC_UUID);
  tx = svc->createCharacteristic(TX_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  tx->addDescriptor(new BLE2902());
  BLECharacteristic *rx = svc->createCharacteristic(RX_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(new RxCB());
  svc->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SVC_UUID);
  adv->setScanResponse(true);  // nome vai na resposta de varredura (cabe nos 31 bytes)
  BLEDevice::startAdvertising();
  Serial.println("Anunciando como " NOME_BLE ". Comandos: LER, STATUS, INT=s, GLI=s, TEMP=s, PA=s, BIO=s");
}

void loop() {
  if (pendente.length()) { String c = pendente; pendente = ""; comando(c); }
  if (Serial.available()) comando(Serial.readStringUntil('\n'));

  for (int i = 0; i < 4; i++) {
    if (intervalo[i] && millis() - ultimo[i] >= intervalo[i] * 1000UL) {
      ultimo[i] = millis();
      enviar(gerar(i));
    }
  }
  delay(20);
}
