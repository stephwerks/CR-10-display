#include <U8g2lib.h>
#include <SPI.h>
#include <WiFi.h>

// LCD - Software SPI
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, /* clock=*/ 12, /* data=*/ 11, /* cs=*/ 10);

// Pins
#define ENC_A 5
#define ENC_B 6
#define ENC_BTN 7
#define BUZZER 15

// Encoder state
volatile int encoderPos = 0;
volatile uint8_t lastEncState = 0;
int lastEncoderDiv = 0;

void IRAM_ATTR encoderISR() {
  uint8_t a = digitalRead(ENC_A);
  uint8_t b = digitalRead(ENC_B);
  uint8_t s = (a << 1) | b;

  static const int8_t transition[4][4] = {
    { 0, -1,  1,  0},
    { 1,  0,  0, -1},
    {-1,  0,  0,  1},
    { 0,  1, -1,  0}
  };

  encoderPos += transition[lastEncState][s];
  lastEncState = s;
}

// Button debounce
unsigned long lastBtnPress = 0;
#define DEBOUNCE_MS 200

// UI states
#define ST_SPLASH     0
#define ST_SCANNING   1
#define ST_NET_LIST   2
#define ST_PASSWORD   3
#define ST_CONNECTING 4
#define ST_CONNECTED  5
#define ST_FAILED     6

uint8_t uiState = ST_SPLASH;
unsigned long stateEnterTime = 0;

// Network data
struct WifiNet {
  String ssid;
  int32_t rssi;
  bool open;
};

WifiNet networks[20];
int networkCount = 0;
int listSel = 0;
int listScroll = 0;

// Password entry
String selectedSSID;
bool selectedOpen = false;
String password = "";
#define MAX_PWD_LEN 63

// Character ribbon: A-Z a-z 0-9 symbols DEL OK
const char ribbon[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 !@#$%^&*()-_=+[]{}|;:',.<>?/~`\"\\";
const int RIBBON_LEN = sizeof(ribbon) - 1;
const int RIBBON_DEL = RIBBON_LEN;
const int RIBBON_OK = RIBBON_LEN + 1;
const int RIBBON_TOTAL = RIBBON_LEN + 2;
int ribbonPos = 0;

// Connecting
unsigned long connectStart = 0;
#define CONNECT_TIMEOUT 15000

// Scan trigger
bool scanReady = false;

// Error message
String failMsg;

void buzz(int ms) {
  digitalWrite(BUZZER, HIGH);
  delay(ms);
  digitalWrite(BUZZER, LOW);
}

bool btnPressed() {
  if (!digitalRead(ENC_BTN) && (millis() - lastBtnPress > DEBOUNCE_MS)) {
    lastBtnPress = millis();
    buzz(20);
    return true;
  }
  return false;
}

int encoderDelta() {
  int cur = encoderPos / 4;
  int delta = cur - lastEncoderDiv;
  lastEncoderDiv = cur;
  return delta;
}

void changeState(uint8_t s) {
  uiState = s;
  stateEnterTime = millis();
}

void sortAndDedup() {
  for (int i = 0; i < networkCount; i++) {
    for (int j = i + 1; j < networkCount; j++) {
      if (networks[i].ssid == networks[j].ssid) {
        if (networks[j].rssi > networks[i].rssi) {
          networks[i].rssi = networks[j].rssi;
          networks[i].open = networks[j].open;
        }
        for (int k = j; k < networkCount - 1; k++) {
          networks[k] = networks[k + 1];
        }
        networkCount--;
        j--;
      }
    }
  }

  for (int i = 0; i < networkCount - 1; i++) {
    for (int j = i + 1; j < networkCount; j++) {
      if (networks[j].rssi > networks[i].rssi) {
        WifiNet tmp = networks[i];
        networks[i] = networks[j];
        networks[j] = tmp;
      }
    }
  }
}

void doScan() {
  int n = WiFi.scanNetworks();
  networkCount = 0;
  for (int i = 0; i < n && networkCount < 20; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    networks[networkCount].ssid = ssid;
    networks[networkCount].rssi = WiFi.RSSI(i);
    networks[networkCount].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    networkCount++;
  }
  WiFi.scanDelete();
  sortAndDedup();
  listSel = 0;
  listScroll = 0;
}

// --- Drawing ---

void drawSplash() {
  u8g2.setFont(u8g2_font_ncenB08_tr);
  const char* t1 = "WiFi Scanner";
  int w1 = u8g2.getStrWidth(t1);
  u8g2.setCursor((128 - w1) / 2, 28);
  u8g2.print(t1);

  u8g2.setFont(u8g2_font_5x8_tr);
  const char* t2 = "CR-10 + ESP32-S3";
  int w2 = u8g2.getStrWidth(t2);
  u8g2.setCursor((128 - w2) / 2, 44);
  u8g2.print(t2);
}

void drawScanning() {
  u8g2.setFont(u8g2_font_ncenB08_tr);
  const char* t = "Scanning";
  int w = u8g2.getStrWidth(t);
  int dots = ((millis() - stateEnterTime) / 400) % 4;
  u8g2.setCursor((128 - w) / 2 - 6, 36);
  u8g2.print(t);
  for (int i = 0; i < dots; i++) u8g2.print(".");
}

void drawNetList() {
  u8g2.setFont(u8g2_font_5x8_tr);
  int totalEntries = networkCount + 1;
  int visible = 5;

  if (listSel < listScroll) listScroll = listSel;
  if (listSel >= listScroll + visible) listScroll = listSel - visible + 1;

  for (int i = 0; i < visible; i++) {
    int idx = listScroll + i;
    if (idx >= totalEntries) break;
    int y = 10 + i * 11;

    if (idx == listSel) {
      u8g2.setCursor(0, y);
      u8g2.print(">");
    }

    if (idx < networkCount) {
      String label = networks[idx].ssid;
      if (networks[idx].open) label += "*";
      if (label.length() > 16) label = label.substring(0, 16);
      u8g2.setCursor(8, y);
      u8g2.print(label);

      char rssi[8];
      snprintf(rssi, sizeof(rssi), "%d", networks[idx].rssi);
      int rw = u8g2.getStrWidth(rssi);
      u8g2.setCursor(128 - rw, y);
      u8g2.print(rssi);
    } else {
      u8g2.setCursor(8, y);
      u8g2.print("[Rescan]");
    }
  }

  if (totalEntries > visible) {
    int barH = max(4, (visible * 55) / totalEntries);
    int barY = 2 + (listScroll * (55 - barH)) / (totalEntries - visible);
    u8g2.drawBox(126, barY, 2, barH);
  }
}

void drawPassword() {
  u8g2.setFont(u8g2_font_5x8_tr);

  String hdr = selectedSSID;
  if (hdr.length() > 21) hdr = hdr.substring(0, 21);
  u8g2.setCursor(0, 8);
  u8g2.print(hdr);

  u8g2.setCursor(0, 22);
  String display = password;
  if (display.length() > 20) {
    display = display.substring(display.length() - 20);
  }
  u8g2.print(display);
  if ((millis() / 500) % 2 == 0) {
    u8g2.print("_");
  }

  u8g2.drawHLine(0, 28, 128);

  u8g2.setFont(u8g2_font_6x10_tr);
  int centerY = 46;

  int windowHalf = 5;
  for (int i = -windowHalf; i <= windowHalf; i++) {
    int ri = ribbonPos + i;
    while (ri < 0) ri += RIBBON_TOTAL;
    ri = ri % RIBBON_TOTAL;

    int x = 64 + i * 11;
    if (x < 0 || x > 122) continue;

    char label[4];
    if (ri < RIBBON_LEN) {
      label[0] = ribbon[ri];
      label[1] = '\0';
    } else if (ri == RIBBON_DEL) {
      strcpy(label, "DE");
    } else {
      strcpy(label, "OK");
    }

    if (i == 0) {
      int lw = u8g2.getStrWidth(label);
      u8g2.drawFrame(64 - lw / 2 - 2, centerY - 9, lw + 4, 13);
    }

    int lw = u8g2.getStrWidth(label);
    u8g2.setCursor(x - lw / 2, centerY);
    u8g2.print(label);
  }

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(0, 63);
  char hint[22];
  snprintf(hint, sizeof(hint), "%d chars", password.length());
  u8g2.print(hint);
}

void drawConnecting() {
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(0, 20);
  u8g2.print("Connecting to:");
  u8g2.setCursor(0, 32);
  String s = selectedSSID;
  if (s.length() > 21) s = s.substring(0, 21);
  u8g2.print(s);

  int dots = ((millis() - stateEnterTime) / 400) % 4;
  u8g2.setCursor(0, 48);
  for (int i = 0; i < dots; i++) u8g2.print(".");
}

void drawConnected() {
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("Connected!");

  u8g2.setCursor(0, 24);
  String s = selectedSSID;
  if (s.length() > 21) s = s.substring(0, 21);
  u8g2.print(s);

  u8g2.setCursor(0, 38);
  char rssi[16];
  snprintf(rssi, sizeof(rssi), "RSSI: %d dBm", WiFi.RSSI());
  u8g2.print(rssi);

  u8g2.setCursor(0, 50);
  u8g2.print("IP: ");
  u8g2.print(WiFi.localIP());

  u8g2.setCursor(0, 63);
  u8g2.print("Click to disconnect");
}

void drawFailed() {
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(0, 20);
  u8g2.print("Connection Failed");

  u8g2.setCursor(0, 36);
  String m = failMsg;
  if (m.length() > 21) m = m.substring(0, 21);
  u8g2.print(m);

  u8g2.setCursor(0, 56);
  u8g2.print("Click to retry");
}

// --- Main ---

void setup() {
  u8g2.begin();

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BTN, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  lastEncState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);

  attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), encoderISR, CHANGE);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  changeState(ST_SPLASH);
}

void loop() {
  int delta = encoderDelta();
  bool btn = btnPressed();

  switch (uiState) {
    case ST_SPLASH:
      if (millis() - stateEnterTime > 2000) {
        changeState(ST_SCANNING);
        scanReady = false;
      }
      break;

    case ST_SCANNING:
      if (!scanReady) {
        scanReady = true;
      } else {
        doScan();
        changeState(ST_NET_LIST);
      }
      break;

    case ST_NET_LIST: {
      int totalEntries = networkCount + 1;
      if (delta != 0) {
        listSel += delta;
        if (listSel < 0) listSel = totalEntries - 1;
        if (listSel >= totalEntries) listSel = 0;
      }
      if (btn) {
        if (listSel < networkCount) {
          selectedSSID = networks[listSel].ssid;
          selectedOpen = networks[listSel].open;
          if (selectedOpen) {
            password = "";
            WiFi.begin(selectedSSID.c_str());
            connectStart = millis();
            changeState(ST_CONNECTING);
          } else {
            password = "";
            ribbonPos = 0;
            lastEncoderDiv = encoderPos / 4;
            changeState(ST_PASSWORD);
          }
        } else {
          changeState(ST_SCANNING);
          scanReady = false;
        }
      }
      break;
    }

    case ST_PASSWORD:
      if (delta != 0) {
        ribbonPos += delta;
        while (ribbonPos < 0) ribbonPos += RIBBON_TOTAL;
        ribbonPos = ribbonPos % RIBBON_TOTAL;
      }
      if (btn) {
        if (ribbonPos < RIBBON_LEN) {
          if (password.length() < MAX_PWD_LEN) {
            password += ribbon[ribbonPos];
          }
        } else if (ribbonPos == RIBBON_DEL) {
          if (password.length() > 0) {
            password.remove(password.length() - 1);
          }
        } else {
          if (password.length() > 0) {
            WiFi.begin(selectedSSID.c_str(), password.c_str());
            connectStart = millis();
            changeState(ST_CONNECTING);
          }
        }
      }
      break;

    case ST_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        changeState(ST_CONNECTED);
      } else if (millis() - connectStart > CONNECT_TIMEOUT) {
        WiFi.disconnect();
        failMsg = "Timeout";
        changeState(ST_FAILED);
      } else if (WiFi.status() == WL_CONNECT_FAILED) {
        WiFi.disconnect();
        failMsg = "Wrong password?";
        changeState(ST_FAILED);
      }
      break;

    case ST_CONNECTED:
      if (btn) {
        WiFi.disconnect();
        changeState(ST_SCANNING);
        scanReady = false;
      }
      break;

    case ST_FAILED:
      if (btn) {
        changeState(ST_SCANNING);
        scanReady = false;
      }
      break;
  }

  u8g2.clearBuffer();
  switch (uiState) {
    case ST_SPLASH:     drawSplash();     break;
    case ST_SCANNING:   drawScanning();   break;
    case ST_NET_LIST:   drawNetList();    break;
    case ST_PASSWORD:   drawPassword();   break;
    case ST_CONNECTING: drawConnecting(); break;
    case ST_CONNECTED:  drawConnected();  break;
    case ST_FAILED:     drawFailed();     break;
  }
  u8g2.sendBuffer();
}
