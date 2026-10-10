// EngineSound node for TeslaAmbientLight.
//
// Runs on a classic ESP32 (ESP32-WROOM-32 / ESP32-DevKitC): it is the only
// ESP32 with Bluetooth Classic, which A2DP audio needs. ESP32-S3/C3 won't work.
//
//   master ESP32 (CAN) --Wi-Fi UDP :3334--> this board --Bluetooth A2DP--> Tesla audio
//
// The board joins the master's AP, receives powertrain telemetry, runs a
// virtual engine + automatic gearbox and streams the synthesized sound to the
// car like a phone playing music. Settings page: http://192.168.4.15/
//
// Arduino IDE settings: Board "ESP32 Dev Module",
// Partition Scheme "Minimal SPIFFS (1.9MB APP with OTA)".
// Library: "ESP32-A2DP" by Phil Schatzmann.

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include "BluetoothA2DPSource.h"

#include "TelemetryPacket.h"
#include "EngineModel.h"
#include "EngineSynth.h"

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "EngineSound needs a classic ESP32 (Bluetooth Classic / A2DP). ESP32-S2/S3/C3/C6 have no A2DP."
#endif

// ---- same credentials as the master in ../TeslaAmbientLight.ino ----
const char* ssid = "T3LIGHT";
const char* password = "12345678";
const char* passwordOTA = "12345678";
const IPAddress localIp(192, 168, 4, 15);  // master .10, doors .11-.14

// Telemetry older than this is treated as "car asleep / master gone"
const unsigned long TELEMETRY_TIMEOUT_MS = 600;

WiFiUDP udp;
WebServer web(80);
Preferences prefs;
BluetoothA2DPSource a2dp;

engine::Model model;
engine::Synth synth;

// ---- settings (NVS) ----
struct Settings {
  uint8_t profile = 0;
  uint8_t volume = 70;          // %
  bool enabled = true;
  String btName = "Tesla";      // substring of the car's Bluetooth name
} settings;

// ---- shared between the Wi-Fi loop and the audio callback ----
portMUX_TYPE inputMux = portMUX_INITIALIZER_UNLOCKED;
engine::DriveInput sharedInput;
volatile unsigned long lastPacketMs = 0;
volatile int pendingProfile = -1;
volatile bool btConnected = false;
volatile float statusRpm = 0;
volatile int statusGear = 0;
uint32_t packetsReceived = 0;
EngineTelemetryPacket lastPacket = {};

// ---------------------------------------------------------------------------
// Audio: called by the Bluetooth stack whenever it needs PCM (44.1 kHz stereo)
// ---------------------------------------------------------------------------
int32_t audioCallback(Frame* frames, int32_t count) {
  if (pendingProfile >= 0) {
    const engine::Profile* p = &engine::PROFILES[pendingProfile];
    model.setProfile(p);
    synth.setProfile(p);
    pendingProfile = -1;
  }

  engine::DriveInput in;
  portENTER_CRITICAL(&inputMux);
  in = sharedInput;
  portEXIT_CRITICAL(&inputMux);
  if (millis() - lastPacketMs > TELEMETRY_TIMEOUT_MS) {
    in.awake = false;
    in.valid = false;
  }
  if (!settings.enabled) in.awake = false;

  model.update(count / 44100.0f, in);
  synth.volume = settings.volume / 100.0f;
  synth.render((int16_t*)frames, count, model.state());

  statusRpm = model.state().rpm;
  statusGear = model.state().gear;
  return count;
}

bool btDeviceFound(const char* name, esp_bd_addr_t address, int rssi) {
  String n(name), want = settings.btName;
  n.toLowerCase();
  want.toLowerCase();
  bool match = want.length() > 0 && n.indexOf(want) >= 0;
  Serial.printf("BT found \"%s\" rssi %d%s\n", name, rssi, match ? "  -> connecting" : "");
  return match;
}

void btConnectionChanged(esp_a2d_connection_state_t state, void*) {
  btConnected = state == ESP_A2D_CONNECTION_STATE_CONNECTED;
  Serial.printf("BT %s\n", btConnected ? "connected" : "disconnected");
}

// ---------------------------------------------------------------------------
// Telemetry
// ---------------------------------------------------------------------------
void receiveTelemetry() {
  int size;
  while ((size = udp.parsePacket()) > 0) {
    EngineTelemetryPacket p;
    if (size != sizeof(p)) { udp.flush(); continue; }
    udp.read((uint8_t*)&p, sizeof(p));
    if (p.magic[0] != 'E' || p.magic[1] != 'T' || p.version != ENGINE_TELEMETRY_VERSION) continue;

    engine::DriveInput in;
    in.awake = p.flags & ET_DISPLAY_ON;
    in.valid = p.flags & ET_VALID;
    in.brake = p.flags & ET_BRAKE;
    in.hasTorque = p.flags & ET_HAS_TORQUE;
    in.gear = p.gear;
    in.pedal = p.pedal * 0.004f;
    in.speedKph = p.speed * 0.01f;
    in.torqueNm = (float)p.rearTorque + (float)p.frontTorque;

    portENTER_CRITICAL(&inputMux);
    sharedInput = in;
    portEXIT_CRITICAL(&inputMux);
    lastPacketMs = millis();
    lastPacket = p;
    packetsReceived++;
  }
}

// ---------------------------------------------------------------------------
// Settings web page
// ---------------------------------------------------------------------------
void loadSettings() {
  prefs.begin("engine", true);
  settings.profile = prefs.getUChar("profile", 0);
  settings.volume = prefs.getUChar("volume", 70);
  settings.enabled = prefs.getBool("enabled", true);
  settings.btName = prefs.getString("btname", "Tesla");
  prefs.end();
  if (settings.profile >= engine::PROFILE_COUNT) settings.profile = 0;
}

void saveSettings() {
  prefs.begin("engine", false);
  prefs.putUChar("profile", settings.profile);
  prefs.putUChar("volume", settings.volume);
  prefs.putBool("enabled", settings.enabled);
  prefs.putString("btname", settings.btName);
  prefs.end();
}

String htmlEscape(const String& s) {
  String o;
  for (char c : s) {
    if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;"; else if (c == '&') o += "&amp;"; else o += c;
  }
  return o;
}

void handleRoot() {
  String h;
  h.reserve(3000);
  h += F("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>Engine Sound</title><style>body{font:16px system-ui;margin:16px;max-width:480px;background:#111;color:#eee}"
         "label{display:block;margin:14px 0 4px}select,input[type=text]{width:100%;padding:8px;font-size:16px}"
         "input[type=range]{width:100%}button{margin-top:18px;padding:10px 18px;font-size:16px}"
         "#s{margin-top:20px;padding:10px;background:#222;border-radius:8px;font-family:monospace;white-space:pre}</style></head><body>"
         "<h2>Engine Sound</h2><form method=post action=/save>");
  h += F("<label>Engine</label><select name=profile>");
  for (int i = 0; i < engine::PROFILE_COUNT; i++) {
    h += "<option value=" + String(i) + (i == settings.profile ? " selected>" : ">") + engine::PROFILES[i].name + "</option>";
  }
  h += F("</select><label>Volume <span id=vv></span></label><input type=range name=volume min=0 max=100 oninput='vv.textContent=this.value' value=");
  h += String(settings.volume);
  h += F("><label><input type=checkbox name=enabled value=1");
  if (settings.enabled) h += F(" checked");
  h += F("> Sound on</label><label>Car Bluetooth name contains</label><input type=text name=btname value=\"");
  h += htmlEscape(settings.btName);
  h += F("\"><button>Save</button></form><div id=s>...</div><script>"
         "vv.textContent=document.querySelector('[name=volume]').value;"
         "setInterval(()=>fetch('/status').then(r=>r.json()).then(j=>{s.textContent="
         "'Bluetooth: '+(j.bt?'connected':'searching')+'\\nTelemetry: '+(j.age<1000?'ok':'none')+' ('+j.packets+' pkts)'"
         "+'\\nCar: '+['?','P','R','N','D'][j.carGear]+'  '+j.speed.toFixed(0)+' km/h  pedal '+j.pedal.toFixed(0)+'%  '+j.torque+' Nm'"
         "+'\\nEngine: '+j.rpm.toFixed(0)+' rpm  gear '+(j.gear<0?'R':j.gear==0?'N':j.gear)}).catch(()=>{}),500)"
         "</script></body></html>");
  web.send(200, "text/html", h);
}

void handleSave() {
  if (web.hasArg("profile")) {
    int p = web.arg("profile").toInt();
    if (p >= 0 && p < engine::PROFILE_COUNT && p != settings.profile) {
      settings.profile = p;
      pendingProfile = p;
    }
  }
  if (web.hasArg("volume")) settings.volume = constrain(web.arg("volume").toInt(), 0, 100);
  settings.enabled = web.hasArg("enabled");
  if (web.hasArg("btname")) settings.btName = web.arg("btname").substring(0, 40);
  saveSettings();
  web.sendHeader("Location", "/");
  web.send(303);
}

void handleStatus() {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "{\"bt\":%s,\"age\":%lu,\"packets\":%lu,\"carGear\":%u,\"speed\":%.1f,\"pedal\":%.1f,\"torque\":%d,\"rpm\":%.0f,\"gear\":%d}",
           btConnected ? "true" : "false", millis() - lastPacketMs, (unsigned long)packetsReceived,
           lastPacket.gear <= 4 ? lastPacket.gear : 0, lastPacket.speed * 0.01f, lastPacket.pedal * 0.4f,
           lastPacket.rearTorque + lastPacket.frontTorque, (float)statusRpm, (int)statusGear);
  web.send(200, "application/json", buf);
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(921600);
  Serial.println("\nEngineSound");
  loadSettings();

  const engine::Profile* p = &engine::PROFILES[settings.profile];
  model.setProfile(p);
  synth.init(44100, p);

  // Wi-Fi first (Bluetooth and Wi-Fi share the radio; coexistence is handled by the IDF)
  WiFi.config(localIp, IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password, 1);
  if (WiFi.waitForConnectResult(10000) == WL_CONNECTED)
    Serial.println("Wi-Fi connected");
  else
    Serial.println("Wi-Fi not connected yet, will keep retrying");

  udp.begin(ENGINE_TELEMETRY_PORT);

  web.on("/", HTTP_GET, handleRoot);
  web.on("/save", HTTP_POST, handleSave);
  web.on("/status", HTTP_GET, handleStatus);
  web.begin();

  ArduinoOTA.setHostname("engine-sound");
  ArduinoOTA.setPassword(passwordOTA);
  ArduinoOTA.onStart([]() { a2dp.end(); });  // free the radio and CPU for the update
  ArduinoOTA.begin();

  // Bluetooth: pair once with the car in "Add device" mode, then it reconnects by itself
  a2dp.set_local_name("Engine Sound");
  a2dp.set_ssp_enabled(true);                // car shows a code, confirm it on the screen
  a2dp.set_auto_reconnect(true);
  a2dp.set_ssid_callback(btDeviceFound);
  a2dp.set_on_connection_state_changed(btConnectionChanged);
  a2dp.set_data_callback_in_frames(audioCallback);
  a2dp.set_volume(127);                      // volume is applied in the synth
  a2dp.start();
}

void loop() {
  receiveTelemetry();
  web.handleClient();
  ArduinoOTA.handle();
  delay(2);
}
