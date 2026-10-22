// =====================================================================
//  BB8_REMOTE - display.h     (Guide: OLED live robot/Remote status)
//
//  SSD1306 128x64. One status page (7 lines x 21 characters):
//
//   [B][H][C]   11.9V  65%     Body/Head/Camera online + Body battery
//   ACTIVE   own:R ARM         Body state / control owner / armed
//   DRV y: +40 x: -10          current movement command (Joystick 1)
//   MOT m1:+152 m3:  +0        Body motor outputs
//   HEAD  90  75 110           head servo angles, or "~ WAVING ~"
//   EYE NORMAL    CAM#3S       eye design (Head) | captures (S = SD card)
//   R +2.1 P -1.0 M1 F00       Body roll/pitch, magnet, fault bits (hex)
//
//  Events (PB1-PB4, joystick pushes, capture result) show a short
//  pop-up box. Holding PB4 shows a reset progress bar.
// =====================================================================
#pragma once

#if ENABLE_OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(128, 64, &Wire, -1, 400000, 400000);
static bool oledOk = false;

void displayBegin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL, 400000);
  Wire.beginTransmission(OLED_ADDR);
  oledOk = (Wire.endTransmission() == 0) &&
           oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR, true, false);
  Serial.printf("[OLED] %s\n", oledOk ? "OK" : "NOT FOUND - check SDA 21 / SCL 22 / 3.3 V");
  if (!oledOk) return;
  oled.setRotation(OLED_FLIP ? 2 : 0);
  oled.setTextColor(SSD1306_WHITE);
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setCursor(10, 8);
  oled.print("BB-8");
  oled.setTextSize(1);
  oled.setCursor(64, 14);
  oled.print("REMOTE");
  oled.setCursor(0, 36);
  oled.print("Calibrating...");
  oled.setCursor(0, 48);
  oled.print("Do NOT touch sticks");
  oled.display();
}

static void linkBox(int x, const char* label, bool on) {
  if (on) { oled.fillRect(x, 0, 9, 9, SSD1306_WHITE); oled.setTextColor(SSD1306_BLACK); }
  else    { oled.drawRect(x, 0, 9, 9, SSD1306_WHITE); oled.setTextColor(SSD1306_WHITE); }
  oled.setCursor(x + 2, 1);
  oled.print(label);
  oled.setTextColor(SSD1306_WHITE);
}

static void drawPopup() {
  oled.fillRoundRect(4, 20, 120, 24, 4, SSD1306_BLACK);
  oled.drawRoundRect(4, 20, 120, 24, 4, SSD1306_WHITE);
  oled.fillRoundRect(6, 22, 116, 20, 3, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  int w = strlen(R.popup) * 6;
  oled.setCursor(max(8, 64 - w / 2), 28);
  oled.print(R.popup);
  oled.setTextColor(SSD1306_WHITE);
}

static void drawStatusPage() {
  uint32_t now = millis();
  bool bodyUp = linkAlive(R.body.rxMs, LINK_TIMEOUT_MS);
  bool headUp = linkAlive(R.headNode.rxMs, LINK_TIMEOUT_MS * 2);
  bool camUp  = linkAlive(R.cam.rxMs, CAM_LINK_TIMEOUT_MS);
  BodyInfo& b = R.body;

  // line 0: links + battery
  linkBox(0, "B", bodyUp);
  linkBox(11, "H", headUp);
  linkBox(22, "C", camUp);
  oled.setCursor(36, 1);
  if (!bodyUp)               oled.print("");
  else if (!b.battPresent)   oled.print("   batt: USB");
  else if ((b.battLow || b.battCrit) && (now / 400) % 2) oled.printf(" %s %.1fV", b.battCrit ? "CRIT" : "LOW!", b.battV);
  else                       oled.printf("  %5.1fV %3d%%", b.battV, b.battPct);

  oled.setCursor(0, 12);                          // line 1: state / owner
  if (!ENABLE_WIFI)          oled.print("WI-FI OFF (test)");
  else if (!R.netUp)         oled.print("WiFi connecting...");
  else if (!bodyUp)          oled.print("BODY OFFLINE");
  else oled.printf("%-8s own:%c %s", b.state, b.owner, b.armed ? "ARM" : "---");

  oled.setCursor(0, 21);                          // line 2: movement
  if (R.joyFault)            oled.print("JOYSTICK FAULT!");
  else if (bodyUp && !b.armed && (R.j1y || R.j1x) && b.owner != 'W')
                             oled.print("CENTRE JOYSTICK 1");
  else oled.printf("DRV y:%+4d x:%+4d", R.j1y, R.j1x);

  oled.setCursor(0, 30);                          // line 3: motors
  if (bodyUp) oled.printf("MOT m1:%+4d m3:%+4d", b.m1, b.m3);
  else        oled.print("MOT  --");

  oled.setCursor(0, 39);                          // line 4: head
  if (bodyUp && b.waving) oled.print("HEAD  ~ WAVING ~");
  else if (bodyUp) oled.printf("HEAD %3.0f %3.0f %3.0f", b.servo[0], b.servo[1], b.servo[2]);
  else             oled.printf("HEAD>%3.0f %3.0f %3.0f", R.head[0], R.head[1], R.head[2]);

  oled.setCursor(0, 48);                          // line 5: eyes + camera
  if (headUp) oled.printf("EYE %-7s", R.headNode.eyeName);
  else        oled.print("EYE --     ");
  oled.setCursor(72, 48);
  if (!camUp)                 oled.print("CAM --");
  else if (R.capWaitMs)       oled.print("CAM ...");
  else if (!R.cam.camOk)      oled.print("CAM ERR");
  else oled.printf("CAM#%lu%s", (unsigned long)R.cam.cap, R.cam.sd ? "S" : "");

  oled.setCursor(0, 57);                          // line 6: angle / magnet / faults
  if (bodyUp) oled.printf("R%+5.1f P%+5.1f M%d F%02X", b.roll, b.pitch, (int)b.mag, b.faults);
  else        oled.printf("RSSI %d  tx %lu", R.rssi, (unsigned long)(R.txCount % 100000));
}

void displayUpdate() {
  if (!oledOk) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  drawStatusPage();
  if (R.resetHold > 0 && R.resetHold < 1) {       // PB4 hold progress
    oled.fillRect(0, 20, 128, 24, SSD1306_BLACK);
    oled.drawRect(0, 20, 128, 24, SSD1306_WHITE);
    oled.setCursor(4, 23);
    oled.print("HOLD = FULL RESET");
    oled.fillRect(4, 34, (int)(120 * R.resetHold), 6, SSD1306_WHITE);
  } else if (R.popup[0] && (int32_t)(R.popupUntil - millis()) > 0) {
    drawPopup();
  }
  oled.display();
}

#else
void displayBegin() { Serial.println("[OLED] disabled in config.h"); }
void displayUpdate() {}
#endif
