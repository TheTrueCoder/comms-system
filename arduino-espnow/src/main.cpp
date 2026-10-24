#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <FastLED.h>
#include <esp_wifi.h>

// Simple 12-byte packet (matches the ESP-IDF test's idea of group/filtering)
#pragma pack(push, 1)
struct Msg {
  uint8_t magic;   // 0xB7
  uint8_t version; // 1
  uint8_t group;   // filter to same kit
  uint8_t flags;   // bit0: pressed
  uint32_t seq;
  uint32_t uptimeMs;
};
#pragma pack(pop)

#define MAGIC 0xB7
#define VER   1
#define FLAG_PRESSED (1 << 0)

// Defaults (can be overridden by build flags)
#ifndef BUTTON_PIN
#define BUTTON_PIN 0
#endif
#ifndef LED_PIN
#define LED_PIN 48
#endif
#ifndef ESPNOW_CHANNEL
#define ESPNOW_CHANNEL 1
#endif
#ifndef ESPNOW_GROUP
#define ESPNOW_GROUP 1
#endif

#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

// State
static bool localPressed = false;
static bool remotePressed = false;
static unsigned long lastSend = 0;
static unsigned long lastRx = 0;
static uint32_t seq = 0;

// Broadcast address
static uint8_t broadcastAddr[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// Debounce
static bool lastRaw = false;
static unsigned long lastChange = 0;
const unsigned long DEBOUNCE_MS = 30;
const unsigned long PRESS_REFRESH_MS = 100;
const unsigned long HEARTBEAT_MS = 250;
const unsigned long TIMEOUT_MS = 1000;

// Send current local state
void sendState(bool pressed) {
  Msg m;
  m.magic = MAGIC;
  m.version = VER;
  m.group = ESPNOW_GROUP;
  m.flags = pressed ? FLAG_PRESSED : 0;
  m.seq = ++seq;
  m.uptimeMs = millis();
  esp_now_send(broadcastAddr, (uint8_t*)&m, sizeof(m));
}

// Receive callback
void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != (int)sizeof(Msg)) return;
  Msg m;
  memcpy(&m, data, sizeof(m));
  if (m.magic != MAGIC || m.version != VER || m.group != ESPNOW_GROUP) return;
  // Ignore our own packets if any
  if (memcmp(mac, WiFi.macAddress().c_str(), 6) == 0) {
    // WiFi.macAddress() gives formatted string; easier to just trust group/magic
    // but cheap to skip if identical - not critical for broadcast test
  }
  remotePressed = (m.flags & FLAG_PRESSED) != 0;
  lastRx = millis();
}

// Init ESP-NOW
void initEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, broadcastAddr, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    // Might already exist
  }
  Serial.printf("ESP-NOW ready (ch %d)\n", ESPNOW_CHANNEL);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // FastLED for WS2812
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(32);
  // Quick self-test
  leds[0] = CRGB::Red; FastLED.show(); delay(150);
  leds[0] = CRGB::Green; FastLED.show(); delay(150);
  leds[0] = CRGB::Blue; FastLED.show(); delay(150);
  leds[0] = CRGB::Black; FastLED.show();

  initEspNow();
  // Announce
  sendState(false);
  lastSend = millis();
  lastRx = millis(); // avoid immediate timeout before peer seen
}

void loop() {
  unsigned long now = millis();

  // Debounce button (active low)
  bool raw = (digitalRead(BUTTON_PIN) == LOW);
  if (raw != lastRaw) {
    lastRaw = raw;
    lastChange = now;
  } else if ((now - lastChange) >= DEBOUNCE_MS && raw != localPressed) {
    localPressed = raw;
    Serial.printf("local BOOT %s\n", localPressed ? "pressed" : "released");
    sendState(localPressed);
    lastSend = now;
  }

  // Timeout remote if no packets
  if (lastRx > 0 && (now - lastRx) > TIMEOUT_MS) {
    if (remotePressed) {
      remotePressed = false;
      Serial.println("peer offline (timeout)");
    }
  }

  // LED: on if either pressed
  bool active = localPressed || remotePressed;
  if (active) {
    leds[0] = CRGB::Green;
  } else {
    leds[0] = CRGB::Black;
  }
  FastLED.show();

  // Refresh: fast while pressed, else heartbeat
  unsigned long interval = localPressed ? PRESS_REFRESH_MS : HEARTBEAT_MS;
  if ((now - lastSend) >= interval) {
    sendState(localPressed);
    lastSend = now;
  }

  delay(10);
}
