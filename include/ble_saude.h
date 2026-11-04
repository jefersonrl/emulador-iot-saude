/*
  Base comum dos dispositivos de saúde BLE (um dispositivo por ESP32).

  Cada arquivo de dispositivo define, ANTES de incluir este cabeçalho:
    #define TIPO              "GLI"           identificador usado nos comandos
    #define PREFIXO           "Glicosimetro"  nome BLE (recebe um sufixo único)
    #define INTERVALO_PADRAO  15              segundos entre leituras
  e implementa a função:
    String gerarLeitura();   devolve os campos JSON da leitura, sem chaves

  Comandos aceitos (app, painel ou Monitor Serial a 115200):
    LER        envia uma leitura agora
    STATUS     mostra o intervalo atual, a versão do firmware e o modelo
    INT=10     muda o intervalo para 10 s
    GLI=10     igual ao anterior, mas só vale para o dispositivo desse TIPO
    GLI=0      desliga o envio automático
  Comandos de outro tipo (ex.: TEMP=5 num glicosímetro) são ignorados, assim o
  painel pode mandar o mesmo comando para todos os aparelhos conectados.
*/
#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define LED_PIN  2
#define FW_VERSAO "2.0.0-fhir"                  // versão 2: painel converte as leituras para HL7 FHIR
#define MODELO    "ESP32 DevKit V1 (emulador)"  // usado no recurso FHIR Device
#define SVC_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_UUID  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // app escreve comandos
#define TX_UUID  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // ESP32 notifica dados

String gerarLeitura();  // implementada no arquivo de cada dispositivo

static BLEServer *srv;
static BLECharacteristic *tx;
static volatile bool conectado = false;
static String pendente;          // comando recebido por BLE, tratado no loop
static String nomeBle;           // ex.: Glicosimetro-A1B2
static uint32_t intervalo = INTERVALO_PADRAO;
static uint32_t ultimo = 0;

// Utilitários para gerar dados que variam aos poucos, como medidas reais
static float rnd(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }
static float passo(float &x, float d, float mn, float mx) { x = constrain(x + rnd(-d, d), mn, mx); return x; }

// Envia por Serial e BLE, quebrando em pedaços do tamanho do MTU negociado
static void enviar(const String &s) {
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

static void enviarLeitura() {
  enviar("{\"id\":\"" + nomeBle + "\"," + gerarLeitura() + ",\"t\":" + String(millis() / 1000) + "}");
}

static void status() {
  enviar("{\"id\":\"" + nomeBle + "\",\"intervalos_s\":{\"" TIPO "\":" + String(intervalo) +
         "},\"fw\":\"" FW_VERSAO "\",\"modelo\":\"" MODELO "\"}");
}

static void comando(String c) {
  c.trim(); c.toUpperCase();
  if (c.isEmpty()) return;
  if (c == "LER") { enviarLeitura(); return; }
  if (c == "STATUS") { status(); return; }
  int eq = c.indexOf('=');
  if (eq > 0) {
    String k = c.substring(0, eq);
    if (k == "INT" || k == TIPO) {
      intervalo = c.substring(eq + 1).toInt(); ultimo = millis(); status(); return;
    }
    if (k == "GLI" || k == "TEMP" || k == "PA" || k == "BIO") return;  // é de outro aparelho
  }
  enviar("{\"id\":\"" + nomeBle + "\",\"erro\":\"comando invalido\"}");
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

  // Sufixo com o final do MAC: permite vários aparelhos do mesmo tipo
  char suf[6];
  snprintf(suf, sizeof suf, "%04X", (uint16_t)(ESP.getEfuseMac() >> 32));
  nomeBle = String(PREFIXO) + "-" + suf;

  BLEDevice::init(nomeBle.c_str());
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
  Serial.println("Anunciando como " + nomeBle + ". Comandos: LER, STATUS, INT=s, " TIPO "=s");
}

void loop() {
  if (pendente.length()) { String c = pendente; pendente = ""; comando(c); }
  if (Serial.available()) comando(Serial.readStringUntil('\n'));
  if (intervalo && millis() - ultimo >= intervalo * 1000UL) { ultimo = millis(); enviarLeitura(); }
  delay(20);
}
