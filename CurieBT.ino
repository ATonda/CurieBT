/*
 * CurieBT Geiger Counter - ESP32 WROOM-32
 * Bluetooth Classic SPP + Captive Portal AP (60s po startu)
 *
 * GPIO 23 = Geiger pulse (FALLING interrupt)
 * GPIO 34 = VBAT ADC (dělič R1=100k, R2=100k → max 2.1V na pinu)
 *
 * BT formát (SPP TX):
 *   CPS=X.XX RATE=X.XX\n           každých 250ms
 *   CPS=X.XX RATE=X.XX VBAT=X.X\n  každých 5s
 *
 * CPS výpočet:
 *   Ring buffer 4× 250ms = 1s okno → raw CPS
 *   EMA α=0.3 → výsledek s desetinami
 *
 * PROČ SPP místo BLE NUS:
 *   BLE NUS má connection interval → Android BLE stack hromadí
 *   notifikace při vysokém CPS → UI zamrzá.
 *   SPP = kontinuální stream bez connection intervalu → žádné hromadění.
 *
 * v4: oprava NVS reset + rateIsUsv podmínka
 */

#include <BluetoothSerial.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include "webui.h"

// ── Piny ─────────────────────────────────────────────────────────────────────
#define GEIGER_PIN      23
#define VBAT_PIN        34

// ── Intervaly ────────────────────────────────────────────────────────────────
#define CPS_INTERVAL_MS    250
#define SEND_INTERVAL_MS   250
#define VBAT_INTERVAL_MS   5000
#define AP_TIMEOUT_MS      60000

// ── EMA ──────────────────────────────────────────────────────────────────────
#define EMA_ALPHA  0.3f

// ── Konfigurace (NVS) ────────────────────────────────────────────────────────
struct Config {
  char  ble_name[24]  = "CurieBT";
  char  wifi_pass[34] = "";
  float gm_factor     = 0.5556f;   // SBM-20: 60/108 µSv/CPS
  bool  en_rate       = true;
  float vbat_r1       = 100.0f;
  float vbat_r2       = 100.0f;
  bool  en_vbat       = true;
} cfg;

// ── Globální objekty ──────────────────────────────────────────────────────────
Preferences    prefs;
WebServer      server(80);
DNSServer      dnsServer;
BluetoothSerial btSerial;

// ── SPP stav ─────────────────────────────────────────────────────────────────
bool btConn = false;

// ── AP stav ──────────────────────────────────────────────────────────────────
bool     apActive        = false;
bool     apUsed          = false;
bool     clientConnected = false;
uint32_t apStartMs       = 0;

bool btRestartPending = false;

// ── Geiger – ring buffer ──────────────────────────────────────────────────────
volatile uint32_t isrPulseCount = 0;
portMUX_TYPE      isrMux        = portMUX_INITIALIZER_UNLOCKED;

uint32_t ringBuffer[4] = {0, 0, 0, 0};
int      ringIndex     = 0;

float    currentCPS = 0.0f;
uint32_t lastCpsMs  = 0;
uint32_t lastSendMs = 0;

// ── VBAT ─────────────────────────────────────────────────────────────────────
uint32_t lastVbatMs  = 0;
float    lastVbat    = 0.0f;
bool     vbatPending = false;   // globální flag — pošli VBAT při příštím sendBT

// ── Deep sleep ────────────────────────────────────────────────────────────────
// Po 5 minutách bez BT připojení → deep sleep, probuzení pouze resetem napájení
#define SLEEP_TIMEOUT_MS  (5UL * 60UL * 1000UL)   // 5 minut
uint32_t lastBtActivityMs = 0;   // čas posledního BT připojení nebo pokusu

// ── ISR ──────────────────────────────────────────────────────────────────────
void IRAM_ATTR onPulse() {
  portENTER_CRITICAL_ISR(&isrMux);
  isrPulseCount++;
  portEXIT_CRITICAL_ISR(&isrMux);
}

// ── SPP callback ─────────────────────────────────────────────────────────────
void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    btConn = true;
    lastBtActivityMs = millis();   // reset sleep timeru
    Serial.println("[BT] Připojeno");
  } else if (event == ESP_SPP_CLOSE_EVT) {
    btConn = false;
    Serial.println("[BT] Odpojeno");
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// NVS
// ═══════════════════════════════════════════════════════════════════════════════
void loadConfig() {
  // Detekovat starý faktor (CPM-based) a resetovat NVS
  prefs.begin("curie", true);
  float oldFactor = prefs.getFloat("gm_factor", 0.5556f);
  prefs.end();
  if (oldFactor < 0.01f) {
    Serial.println("[NVS] Stary CPM faktor detekovan — mazam NVS a nastavuji defaulty");
    prefs.begin("curie", false);
    prefs.clear();
    prefs.end();
  }
  prefs.begin("curie", true);
  prefs.getString("ble_name",  cfg.ble_name,  sizeof(cfg.ble_name));
  prefs.getString("wifi_pass", cfg.wifi_pass, sizeof(cfg.wifi_pass));
  cfg.gm_factor = prefs.getFloat("gm_factor", 0.5556f);
  cfg.en_rate   = prefs.getBool("en_rate",    true);
  cfg.vbat_r1   = prefs.getFloat("vbat_r1",   100.0f);
  cfg.vbat_r2   = prefs.getFloat("vbat_r2",   100.0f);
  cfg.en_vbat   = prefs.getBool("en_vbat",    true);
  prefs.end();
  Serial.printf("[NVS] ble=%s gm=%.5f rate=%d vbat=%d\n",
    cfg.ble_name, cfg.gm_factor, cfg.en_rate, cfg.en_vbat);
}

void saveConfig() {
  prefs.begin("curie", false);
  prefs.putString("ble_name",  cfg.ble_name);
  prefs.putString("wifi_pass", cfg.wifi_pass);
  prefs.putFloat("gm_factor",  cfg.gm_factor);
  prefs.putBool("en_rate",     cfg.en_rate);
  prefs.putFloat("vbat_r1",    cfg.vbat_r1);
  prefs.putFloat("vbat_r2",    cfg.vbat_r2);
  prefs.putBool("en_vbat",     cfg.en_vbat);
  prefs.end();
  Serial.println("[NVS] Ulozeno");
}

// ═══════════════════════════════════════════════════════════════════════════════
// VBAT ADC
// ═══════════════════════════════════════════════════════════════════════════════
void initAdc() {
  analogReadResolution(12);
  analogSetPinAttenuation(VBAT_PIN, ADC_11db);
  Serial.println("[ADC] GPIO34 12bit 11dB");
}

float readVbat() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(VBAT_PIN);
    delayMicroseconds(100);
  }
  float vPin = (sum / 16.0f / 4095.0f) * 3.3f;
  float vbat = vPin * (cfg.vbat_r1 + cfg.vbat_r2) / cfg.vbat_r2;
  vbat *= 0.6114f;   // korekční faktor ADC nepřesnosti ESP32 (změřeno: 4.035V / 6.600V)
  if (vbat < 2.5f) vbat = 2.5f;   // dolní clamp — ochrana proti nesmyslným hodnotám
  return vbat;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SPP init
// ═══════════════════════════════════════════════════════════════════════════════
void startBT() {
  btSerial.register_callback(btCallback);
  if (!btSerial.begin(cfg.ble_name)) {
    Serial.println("[BT] Chyba inicializace!");
    return;
  }
  Serial.printf("[BT] SPP spuštěno: %s\n", cfg.ble_name);
}

void doRestartBT() {
  Serial.printf("[BT] Restart: %s\n", cfg.ble_name);
  btSerial.end();
  delay(300);
  btSerial.register_callback(btCallback);
  btSerial.begin(cfg.ble_name);
  btConn = false;
  Serial.printf("[BT] Restart hotov: %s\n", cfg.ble_name);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SPP odesílání
// ═══════════════════════════════════════════════════════════════════════════════
void sendBT(bool addVbat) {
  if (!btConn) return;

  char buf[96];
  int  pos = 0;

  pos += snprintf(buf + pos, sizeof(buf) - pos, "CPS=%.2f", currentCPS);

  if (cfg.en_rate) {
    float rate = currentCPS * cfg.gm_factor;
    pos += snprintf(buf + pos, sizeof(buf) - pos, " RATE=%.2f", rate);
  }

  if (cfg.en_vbat && addVbat) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, " VBAT=%dmV", (int)(lastVbat * 1000));
  }

  pos += snprintf(buf + pos, sizeof(buf) - pos, "\n");

  btSerial.write((uint8_t*)buf, pos);

  Serial.printf("[TX] %s", buf);
}

// ═══════════════════════════════════════════════════════════════════════════════
// AP / Captive Portal
// ═══════════════════════════════════════════════════════════════════════════════
void handleCaptive() {
  String uri  = server.uri();
  String host = server.hostHeader();
  if (uri == "/generate_204" || uri == "/gen_204") {
    if (!clientConnected) { server.sendHeader("Location","http://192.168.4.1/",true); server.send(302,"text/plain",""); }
    else server.send(204,"text/plain","");
    return;
  }
  if (uri == "/hotspot-detect.html" || host.indexOf("apple") >= 0 || uri == "/library/test/success.html") {
    server.send(200,"text/html","<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); return;
  }
  if (uri == "/ncsi.txt" || uri == "/connecttest.txt") { server.send(200,"text/plain","Microsoft NCSI"); return; }
  if (uri != "/") { server.sendHeader("Location","http://192.168.4.1/",true); server.send(302,"text/plain",""); return; }
  clientConnected = true;
  server.send_P(200,"text/html",INDEX_HTML);
}

void handleConfig() {
  JsonDocument doc;
  doc["ble_name"]  = cfg.ble_name;
  doc["wifi_pass"] = cfg.wifi_pass;
  doc["gm_factor"] = cfg.gm_factor;
  doc["en_rate"]   = cfg.en_rate;
  doc["vbat_r1"]   = cfg.vbat_r1;
  doc["vbat_r2"]   = cfg.vbat_r2;
  doc["en_vbat"]   = cfg.en_vbat;
  String out; serializeJson(doc, out);
  server.send(200,"application/json",out);
}

void handleSave() {
  if (!server.hasArg("plain")) { server.send(400,"text/plain","No body"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400,"text/plain","JSON error"); return; }
  bool nameChanged = false;
  if (doc["ble_name"].is<const char*>()) {
    const char* newName = doc["ble_name"];
    if (strlen(newName) > 0 && strcmp(newName, cfg.ble_name) != 0) nameChanged = true;
    strlcpy(cfg.ble_name, newName, sizeof(cfg.ble_name));
  }
  strlcpy(cfg.wifi_pass, doc["wifi_pass"] | "", sizeof(cfg.wifi_pass));
  cfg.gm_factor = doc["gm_factor"] | cfg.gm_factor;
  cfg.en_rate   = doc["en_rate"]   | cfg.en_rate;
  cfg.vbat_r1   = doc["vbat_r1"]   | cfg.vbat_r1;
  cfg.vbat_r2   = doc["vbat_r2"]   | cfg.vbat_r2;
  cfg.en_vbat   = doc["en_vbat"]   | cfg.en_vbat;
  saveConfig();
  server.send(200,"text/plain","OK");
  Serial.println("[WEB] Ulozeno, zastavuji AP");
  stopAP();
  if (nameChanged) btRestartPending = true;
}

void handleStatus() {
  char buf[128];
  snprintf(buf, sizeof(buf),
    "{\"cps\":%.2f,\"vbat\":%.1f,\"ble\":%s,\"uptime\":%lu}",
    currentCPS, lastVbat, btConn ? "true" : "false", millis() / 1000UL);
  server.send(200,"application/json",buf);
}

void setupWebHandlers() {
  server.on("/",                    HTTP_GET,  handleCaptive);
  server.on("/config",              HTTP_GET,  handleConfig);
  server.on("/save",                HTTP_POST, handleSave);
  server.on("/status",              HTTP_GET,  handleStatus);
  server.on("/generate_204",        HTTP_GET,  handleCaptive);
  server.on("/gen_204",             HTTP_GET,  handleCaptive);
  server.on("/hotspot-detect.html", HTTP_GET,  handleCaptive);
  server.on("/ncsi.txt",            HTTP_GET,  handleCaptive);
  server.on("/connecttest.txt",     HTTP_GET,  handleCaptive);
  server.onNotFound(handleCaptive);
}

void startAP() {
  if (apActive || apUsed) return;
  Serial.println("[AP] Spoustim...");
  // BT Classic a WiFi sdílejí anténu — BT zůstává zapnuté
  // Odesílání se automaticky zpomalí na 1× za sekundu (viz loop)
  WiFi.disconnect(true, true); delay(100);
  WiFi.mode(WIFI_AP); delay(100);
  IPAddress apIP(192,168,4,1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0));
  const char* pass = (strlen(cfg.wifi_pass) >= 8) ? cfg.wifi_pass : nullptr;
  // SSID = "CurieBT 192.168.4.1" — jméno + pevná IP
  char ssid[40];
  snprintf(ssid, sizeof(ssid), "%s 192.168.4.1", cfg.ble_name);
  WiFi.softAP(ssid, pass, 6, 0, 4); delay(200);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);
  MDNS.begin("curie");
  MDNS.addService("http","tcp",80);
  setupWebHandlers();
  server.begin();
  apActive        = true;
  apStartMs       = millis();
  clientConnected = false;
  Serial.printf("[AP] SSID=%s IP=192.168.4.1\n", cfg.ble_name);
}

void stopAP() {
  if (!apActive) return;
  dnsServer.stop();
  MDNS.end();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(200);
  apActive = false;
  apUsed   = true;
  Serial.println("[AP] Vypnuto — BT nyni na plny vykon (250ms)");
}

// ═══════════════════════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("╔════════════════════════════════╗");
  Serial.println("║   CurieBT - ESP32 WROOM  v10  ║");
  Serial.println("║   Bluetooth Classic SPP        ║");
  Serial.println("╚════════════════════════════════╝");
  Serial.println();

  loadConfig();
  initAdc();

  pinMode(GEIGER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), onPulse, FALLING);
  Serial.printf("[GEIGER] GPIO%d OK\n", GEIGER_PIN);

  // BT spustit ihned — WiFi a BT sdílejí anténu
  // Dokud AP běží, odesíláme 1× za sekundu (méně BT aktivity = stabilnější WiFi)
  // Po vypnutí AP → normálně 4× za sekundu
  startBT();
  startAP();

  lastVbat         = readVbat();
  lastVbatMs       = millis();
  lastCpsMs        = millis();
  lastSendMs       = millis();
  lastBtActivityMs = millis();   // start sleep timeru od spuštění

  Serial.println("[SYS] Pripraven\n");
}

// ═══════════════════════════════════════════════════════════════════════════════
// LOOP
// ═══════════════════════════════════════════════════════════════════════════════
void loop() {
  uint32_t now = millis();

  // ── AP obsluha ──────────────────────────────────────────────────────────────
  if (apActive) {
    dnsServer.processNextRequest();
    server.handleClient();
    if (now - apStartMs >= AP_TIMEOUT_MS) {
      Serial.println("[AP] Timeout — vypinam");
      stopAP();
    }
  }

  // ── Deep sleep po 5 minutách bez BT připojení ────────────────────────────
  // Pokud AP neběží a BT není připojeno déle než SLEEP_TIMEOUT_MS → deep sleep
  if (!apActive && !btConn && (now - lastBtActivityMs >= SLEEP_TIMEOUT_MS)) {
    Serial.printf("[SYS] %lu min bez BT — jdu do deep sleep\n", SLEEP_TIMEOUT_MS / 60000);
    Serial.flush();
    delay(100);
    esp_deep_sleep_start();
  }

  // ── BT restart pending ──────────────────────────────────────────────────────
  if (btRestartPending) {
    btRestartPending = false;
    doRestartBT();
  }

  // ── CPS výpočet (každých 250ms) ────────────────────────────────────────────
  if (now - lastCpsMs >= CPS_INTERVAL_MS) {
    lastCpsMs = now;

    portENTER_CRITICAL(&isrMux);
    uint32_t pulses = isrPulseCount;
    isrPulseCount   = 0;
    portEXIT_CRITICAL(&isrMux);

    ringBuffer[ringIndex] = pulses;
    ringIndex = (ringIndex + 1) % 4;

    uint32_t total = 0;
    for (int i = 0; i < 4; i++) total += ringBuffer[i];

    currentCPS = currentCPS * (1.0f - EMA_ALPHA) + (float)total * EMA_ALPHA;

    Serial.printf("[CPS] raw=%u ring=%u+%u+%u+%u=%u cps=%.2f\n",
      pulses,
      ringBuffer[0], ringBuffer[1], ringBuffer[2], ringBuffer[3],
      total, currentCPS);
  }

  // ── VBAT refresh (každých 5s) ──────────────────────────────────────────────
  if (cfg.en_vbat && now - lastVbatMs >= VBAT_INTERVAL_MS) {
    lastVbatMs  = now;
    lastVbat    = readVbat();
    vbatPending = true;   // nastavit flag — pošle se při příštím sendBT
    Serial.printf("[VBAT] %.2fV\n", lastVbat);
  }

  // ── SPP odesílání ─────────────────────────────────────────────────────────
  // Dokud AP běží → 1× za sekundu (méně BT aktivity = stabilnější WiFi)
  // Po vypnutí AP → normálně 4× za sekundu
  const uint32_t sendInterval = apActive ? 1000 : SEND_INTERVAL_MS;
  if (now - lastSendMs >= sendInterval) {
    lastSendMs = now;
    sendBT(vbatPending);
    vbatPending = false;   // vymazat flag po odeslání
  }
}
