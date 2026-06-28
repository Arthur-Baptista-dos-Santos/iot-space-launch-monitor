#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include "secrets.h"

// Pinos
#define PIN_DHT        15
#define PIN_MQ2_A      34   // AOUT: percentual para exibicao
#define PIN_MQ2_D      35   // DOUT: alarme digital confiavel
#define PIN_TRIG        5
#define PIN_ECHO       19
#define PIN_LED_GREEN   2
#define PIN_LED_YELLOW  4
#define PIN_LED_RED    13
#define PIN_LED_BLUE   26
#define PIN_BUZZER     14

// Limiares
#define LIMIAR_GAS      45    // %  - acima: risco de vazamento
#define LIMIAR_TEMP     40.0  // C  - acima: risco de ignicao
#define LIMIAR_DIST_CM  150   // cm - abaixo: intruso na zona de exclusao

// WiFi / MQTT (credenciais definidas em secrets.h, nunca commitado)
const char* WIFI_SSID   = WIFI_SSID_SECRET;
const char* WIFI_PASS   = WIFI_PASS_SECRET;
const char* MQTT_BROKER = MQTT_BROKER_SECRET;
const int   MQTT_PORT   = 8883;
const char* MQTT_USER   = MQTT_USER_SECRET;
const char* MQTT_PASS   = MQTT_PASS_SECRET;
const char* MQTT_TOPIC  = "fiap/iot/lancamento/PLT-01/telemetria";
const char* MQTT_ALERT  = "fiap/iot/lancamento/PLT-01/alerta";
const char* MQTT_CMD    = "fiap/iot/lancamento/PLT-01/comando";

// ── Metadados ──────────────────────────────────────────────────────────────
const char* TAG    = "PLT-01";
const char* NOME   = "Plataforma de Lancamento Espacial";
const char* LOCAL  = "Base de Lancamento FIAP - SP";

// ── Estados ────────────────────────────────────────────────────────────────
enum Estado { LIVRE, ATENCAO, PERIGO, CRITICO };
const char* estadoStr[] = { "LIVRE", "ATENCAO", "PERIGO", "CRITICO" };

WiFiClientSecure wifiClient;
PubSubClient     mqtt(wifiClient);
DHTesp           dht;

float   temp = 0, umid = 0;
int     gasPct = 0;
long    distCm = 999;
Estado  estado = LIVRE, estadoAnterior = LIVRE;
bool    wifiOk = false, mqttOk = false;
static  bool buzAlto = false;
bool    ledAzulRemoto = false; // controle remoto do LED azul
unsigned long tSerial = 0, tMQTT = 0, tBuz = 0, tDHT = 0, tDist = 0;

// ── WiFi ───────────────────────────────────────────────────────────────────
void handleWifi() {
  static bool started = false;
  static unsigned long t0 = 0;
  if (wifiOk) return;
  if (!started) { WiFi.begin(WIFI_SSID, WIFI_PASS); started = true; t0 = millis(); return; }
  if (WiFi.status() == WL_CONNECTED) { wifiOk = true; Serial.println("[WiFi] Conectado!"); return; }
  if (millis() - t0 > 8000) { started = false; }
}

// ── Callback MQTT (recebe comandos) ───────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println("[CMD] Recebido: " + msg);
  if (String(topic) == MQTT_CMD) {
    if (msg == "LED_AZUL_ON")  { ledAzulRemoto = true;  Serial.println("[CMD] LED Azul LIGADO remotamente!"); }
    if (msg == "LED_AZUL_OFF") { ledAzulRemoto = false; Serial.println("[CMD] LED Azul DESLIGADO remotamente!"); }
    if (msg == "BUZZER_ON")    { tone(PIN_BUZZER, 1000); delay(500); noTone(PIN_BUZZER); }
    if (msg == "RESET")        { ledAzulRemoto = false; noTone(PIN_BUZZER); Serial.println("[CMD] RESET executado!"); }
  }
}

// ── MQTT ───────────────────────────────────────────────────────────────────
void handleMqtt() {
  static unsigned long last = 0;
  if (!wifiOk || mqttOk) return;
  if (millis() - last < 5000) return;
  last = millis();
  String id = "ESP32-PLT-" + String(random(0xffff), HEX);
  if (mqtt.connect(id.c_str(), MQTT_USER, MQTT_PASS)) {
    mqttOk = true;
    mqtt.subscribe(MQTT_CMD);
    Serial.println("[MQTT] Conectado! Inscrito em: " + String(MQTT_CMD));
  } else {
    Serial.println("[MQTT] Falhou rc=" + String(mqtt.state()));
  }
}

// ── Motivo do alerta ───────────────────────────────────────────────────────
String calcularMotivo() {
  bool gasAlto  = digitalRead(PIN_MQ2_D) == HIGH;
  bool tempAlta = temp >= LIMIAR_TEMP;
  bool intruso  = distCm <= LIMIAR_DIST_CM;
  if (gasAlto && tempAlta)  return "GAS + TEMPERATURA CRITICA";
  if (gasAlto)              return "VAZAMENTO DE GAS DETECTADO";
  if (intruso)              return "INTRUSO NA ZONA DE EXCLUSAO";
  if (tempAlta)             return "TEMPERATURA ALTA";
  if (distCm < 300)         return "OBJETO SE APROXIMANDO";
  return "NORMAL";
}

// ── Publicar ───────────────────────────────────────────────────────────────
void publicar() {
  if (!mqttOk || !mqtt.connected()) { mqttOk = false; return; }
  StaticJsonDocument<512> doc;
  doc["tag"]    = TAG;
  doc["nome"]   = NOME;
  doc["local"]  = LOCAL;
  doc["estado"] = estadoStr[estado];
  doc["motivo"] = calcularMotivo();
  JsonObject m = doc.createNestedObject("medicoes");
  m["temperatura"]  = temp;
  m["umidade"]      = umid;
  m["gas_pct"]      = gasPct;
  m["gas_alarme"]   = (digitalRead(PIN_MQ2_D) == HIGH);
  m["distancia_cm"] = distCm;
  m["intruso"]      = (distCm <= LIMIAR_DIST_CM);
  char buf[512];
  serializeJson(doc, buf);
  const char* topic = (estado >= PERIGO) ? MQTT_ALERT : MQTT_TOPIC;
  bool ok = mqtt.publish(topic, buf);
  Serial.println(ok ? "[MQTT] Publicado!" : "[MQTT] Falha!");
  Serial.println(buf);
}

// ── Medir distância HC-SR04 ────────────────────────────────────────────────
long medirDistancia() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long dur = pulseIn(PIN_ECHO, HIGH, 30000);
  return (dur == 0) ? 999 : dur / 58;
}

// ── Determinar estado ──────────────────────────────────────────────────────
Estado calcularEstado() {
  bool gasAlto   = digitalRead(PIN_MQ2_D) == HIGH; // DOUT: HIGH quando gás detectado
  bool tempAlta  = temp >= LIMIAR_TEMP;
  bool intruso   = distCm <= LIMIAR_DIST_CM;

  if (gasAlto && tempAlta)       return CRITICO;
  if (gasAlto || intruso)        return PERIGO;
  if (tempAlta || distCm < 300)  return ATENCAO;
  return LIVRE;
}

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("  PLATAFORMA DE LANCAMENTO - GS 2026.1");
  Serial.println("========================================");
  pinMode(PIN_LED_GREEN,  OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED,    OUTPUT);
  pinMode(PIN_LED_BLUE,   OUTPUT);
  pinMode(PIN_BUZZER,     OUTPUT);
  pinMode(PIN_TRIG,       OUTPUT);
  pinMode(PIN_ECHO,       INPUT);
  pinMode(PIN_MQ2_D, INPUT);
  dht.setup(PIN_DHT, DHTesp::DHT22);
  wifiClient.setInsecure();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(500);
  // Blink de boot
  int leds[] = {PIN_LED_GREEN, PIN_LED_YELLOW, PIN_LED_RED, PIN_LED_BLUE};
  for (int i = 0; i < 4; i++) {
    digitalWrite(leds[i], HIGH); delay(150);
    digitalWrite(leds[i], LOW);
  }
  Serial.println("[Setup] Hardware OK!\n");
}

// ── Loop ───────────────────────────────────────────────────────────────────
void loop() {
  handleWifi();
  handleMqtt();
  if (mqttOk) mqtt.loop();

  unsigned long t = millis();

  // Leituras
  if (t - tDHT > 2000) {
    tDHT = t;
    TempAndHumidity d = dht.getTempAndHumidity();
    if (!isnan(d.temperature)) temp = d.temperature;
    if (!isnan(d.humidity))    umid = d.humidity;
  }
  if (t - tDist > 200) {
    tDist = t;
    distCm = medirDistancia();
  }
  gasPct = map(analogRead(PIN_MQ2_A), 0, 4095, 0, 100);

  // Estado
  estadoAnterior = estado;
  estado = calcularEstado();

  // Publica imediatamente se estado piorou
  if (estado > estadoAnterior) {
    Serial.println(">>> MUDANCA DE ESTADO: " + String(estadoStr[estado]));
    publicar();
  }

  // LEDs conforme estado
  digitalWrite(PIN_LED_GREEN,  estado == LIVRE    ? (t / 800) % 2 : LOW);
  digitalWrite(PIN_LED_YELLOW, estado == ATENCAO  ? HIGH : LOW);
  digitalWrite(PIN_LED_RED,    estado >= PERIGO   ? HIGH : LOW);
  digitalWrite(PIN_LED_BLUE,   ledAzulRemoto ? HIGH : (estado == CRITICO ? (t / 100) % 2 : LOW));

  // Buzzer
  if (estado < PERIGO) {
    noTone(PIN_BUZZER);
  } else if (estado == CRITICO && t - tBuz > 250) {
    tBuz = t; buzAlto = !buzAlto;
    tone(PIN_BUZZER, buzAlto ? 1400 : 600);
  } else if (estado == PERIGO && t - tBuz > 500) {
    tBuz = t; tone(PIN_BUZZER, 900);
  }

  // Serial
  if (t - tSerial > 2000) {
    tSerial = t;
    Serial.println("----------------------------");
    Serial.println("TAG      : " + String(TAG));
    Serial.println("Estado   : " + String(estadoStr[estado]));
    Serial.printf ("Temp     : %.1f C\n", temp);
    Serial.printf ("Umidade  : %.1f %%\n", umid);
    Serial.println("Gas      : " + String(gasPct) + "%");
    Serial.println("Distancia: " + String(distCm) + " cm");
    Serial.println("WiFi     : " + String(wifiOk ? "OK" : "conectando..."));
    Serial.println("MQTT     : " + String(mqttOk ? "OK" : "offline"));
    Serial.println("----------------------------");
  }

  // Publicação periódica
  if (t - tMQTT > 5000) {
    tMQTT = t;
    publicar();
  }
}
