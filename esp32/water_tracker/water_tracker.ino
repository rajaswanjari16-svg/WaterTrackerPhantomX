// =====================================================
// WATER TRACKER - ESP32 FIRMWARE
// 4-Person Hydration Tracker with OLED & Buzzer
// =====================================================
// Required Arduino Library Manager Libraries:
// 1. "Adafruit SSD1306" by Adafruit
// 2. "Adafruit GFX Library" by Adafruit
// Built-in ESP32 libraries used:
//   WiFi, HTTPClient, Preferences, Wire, time.h
// =====================================================

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

// =====================================================
// NETWORK & SERVER SETTINGS
// =====================================================

const char *WIFI_SSID = "S23";
const char *WIFI_PASSWORD = "admin000";

// Laptop IP and Server Port (3000)
const char *SERVER_HOST = "10.107.183.37";
const char *STATE_URL = "http://10.107.183.37:3000/api/state";
const char *COMMAND_URL = "http://10.107.183.37:3000/api/command";
const char *COMMAND_ACK_URL = "http://10.107.183.37:3000/api/command/ack";

// =====================================================
// INDIA TIME (Asia/Kolkata, UTC +5:30)
// =====================================================

const long GMT_OFFSET_SEC = 19800; // 5 hours 30 minutes in seconds
const int DAYLIGHT_OFFSET_SEC = 0;

// =====================================================
// OLED DISPLAY (128x64 I2C, Address 0x3C)
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =====================================================
// HARDWARE PINS
// =====================================================

#define TOUCH1 32 // Button A (Controls P1 in Pair 1, P3 in Pair 2)
#define TOUCH2 33 // Button B (Controls P2 in Pair 1, P4 in Pair 2)
#define BUZZER 13 // Piezo Buzzer

// =====================================================
// HYDRATION SETTINGS
// =====================================================

#define GOAL_ML 3000

// =====================================================
// TIMING THRESHOLDS (FINAL SPECIFICATION)
// =====================================================

#define DEBOUNCE_TIME 50     // Minimum ms to reject electrical noise
#define T1_THRESHOLD_MS 2000 // 0 to < 2000 ms  -> +250 mL
#define T2_THRESHOLD_MS                                                        \
  4000                      // 2000 to < 4000 ms -> +200 mL
                            // >= 4000 ms        -> UNDO
#define BOTH_HOLD_TIME 3000 // Both buttons >= 3000 ms -> Switch Pair
#define TIMER_UPDATE 50     // OLED refresh during button hold (ms)

// Intervals
#define SERVER_UPDATE_INTERVAL 2000 // Send state every 2 sec
#define COMMAND_CHECK_INTERVAL 500  // Poll web commands every 500 ms

unsigned long lastServerUpdate = 0;
unsigned long lastCommandCheck = 0;

// =====================================================
// STATE: 4 INDEPENDENT PEOPLE
// =====================================================

// Water amounts in mL
int water[4] = {0, 0, 0, 0};

// History stack for undo (records exact addition amounts: 250 or 200)
#define MAX_HISTORY 50
int history[4][MAX_HISTORY];
int historyCount[4] = {0, 0, 0, 0};

// Currently selected pair: false = Pair 1 (P1/P2), true = Pair 2 (P3/P4)
bool secondPair = false;

// Button state tracking
bool previousTouch1 = false;
bool previousTouch2 = false;

unsigned long touch1Start = 0;
unsigned long touch2Start = 0;

// Both buttons tracking
bool bothActive = false;
bool pairChanged = false;
unsigned long bothStart = 0;

unsigned long lastTimerUpdate = 0;

// Flash Storage
Preferences preferences;
String savedDate = "";

// Forward declarations
void showScreen();
void sendDataToServer();

// =====================================================
// BUZZER FUNCTIONS
// =====================================================

void beep(int duration) {
  digitalWrite(BUZZER, HIGH);
  delay(duration);
  digitalWrite(BUZZER, LOW);
}

// Buzzer feedback tailored for each action
void soundAdd250() { beep(100); }

void soundAdd200() {
  beep(70);
  delay(50);
  beep(70);
}

void soundUndo() {
  beep(160);
  delay(60);
  beep(160);
}

void soundSwitchPair() {
  beep(80);
  delay(60);
  beep(80);
}

void soundGoalComplete() {
  beep(100);
  delay(60);
  beep(100);
  delay(60);
  beep(120);
  delay(60);
  beep(300);
}

void soundWarning() { beep(300); }

// =====================================================
// INDIA TIME & DATE CHECK
// =====================================================

String getToday() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000)) {
    return "";
  }
  char dateBuffer[11];
  strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &timeinfo);
  return String(dateBuffer);
}

// =====================================================
// STORAGE: SAVE & LOAD
// =====================================================

void saveData() {
  preferences.putInt("p1", water[0]);
  preferences.putInt("p2", water[1]);
  preferences.putInt("p3", water[2]);
  preferences.putInt("p4", water[3]);
  preferences.putString("date", savedDate);

  // Persist history counts and stack
  for (int p = 0; p < 4; p++) {
    String countKey = "hc" + String(p);
    preferences.putInt(countKey.c_str(), historyCount[p]);
    String histKey = "h" + String(p);
    preferences.putBytes(histKey.c_str(), history[p], sizeof(history[p]));
  }
}

void resetData() {
  for (int p = 0; p < 4; p++) {
    water[p] = 0;
    historyCount[p] = 0;
    for (int i = 0; i < MAX_HISTORY; i++) {
      history[p][i] = 0;
    }
  }
  secondPair = false;
  savedDate = getToday();
  saveData();

  Serial.println(
      "[RESET] Daily water data and history reset for Asia/Kolkata midnight.");
}

void loadData() {
  String today = getToday();
  savedDate = preferences.getString("date", "");

  if (today != "" && savedDate != "" && savedDate != today) {
    resetData();
    return;
  }

  if (savedDate == "") {
    resetData();
    return;
  }

  water[0] = preferences.getInt("p1", 0);
  water[1] = preferences.getInt("p2", 0);
  water[2] = preferences.getInt("p3", 0);
  water[3] = preferences.getInt("p4", 0);

  for (int p = 0; p < 4; p++) {
    String countKey = "hc" + String(p);
    historyCount[p] = preferences.getInt(countKey.c_str(), 0);
    String histKey = "h" + String(p);
    preferences.getBytes(histKey.c_str(), history[p], sizeof(history[p]));
  }

  Serial.println("[STORAGE] Saved water data loaded from flash.");
}

void checkNewDay() {
  String today = getToday();
  if (today != "" && savedDate != "" && today != savedDate) {
    Serial.println("[NEW DAY] Midnight reached in India. Resetting.");
    resetData();
    showScreen();
    sendDataToServer();
  }
}

// =====================================================
// WIFI CONNECTION
// =====================================================

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) {
    delay(400);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi Connected. ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection pending (will auto-reconnect).");
  }
}

// =====================================================
// OLED SCREENS
// =====================================================

// Normal Screen showing active pair and live water amounts
void showScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  int pLeft = secondPair ? 2 : 0;
  int pRight = secondPair ? 3 : 1;

  // Header banner: Active Pair
  display.setTextSize(1);
  display.setCursor(18, 1);
  if (secondPair) {
    display.print("PAIR 2  [P3 & P4]");
  } else {
    display.print("PAIR 1  [P1 & P2]");
  }
  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  // --- PERSON LEFT (P1 or P3) ---
  display.setCursor(0, 16);
  display.print("P");
  display.print(pLeft + 1);
  display.print(":");

  display.setCursor(24, 16);
  display.print(water[pLeft]);
  display.print("mL");

  // Percentage progress
  int pctLeft = (water[pLeft] * 100) / GOAL_ML;
  if (pctLeft > 100) pctLeft = 100;
  display.setCursor(80, 16);
  display.print(pctLeft);
  display.print("%");

  // Progress Bar Left
  display.drawRect(24, 26, 102, 6, SSD1306_WHITE);
  int fillLeft = (water[pLeft] * 98) / GOAL_ML;
  if (fillLeft > 98)
    fillLeft = 98;
  if (fillLeft > 0) {
    display.fillRect(26, 28, fillLeft, 2, SSD1306_WHITE);
  }

  // --- PERSON RIGHT (P2 or P4) ---
  display.setCursor(0, 38);
  display.print("P");
  display.print(pRight + 1);
  display.print(":");

  display.setCursor(24, 38);
  display.print(water[pRight]);
  display.print("mL");

  // Percentage progress
  int pctRight = (water[pRight] * 100) / GOAL_ML;
  if (pctRight > 100) pctRight = 100;
  display.setCursor(80, 38);
  display.print(pctRight);
  display.print("%");

  // Progress Bar Right
  display.drawRect(24, 48, 102, 6, SSD1306_WHITE);
  int fillRight = (water[pRight] * 98) / GOAL_ML;
  if (fillRight > 98)
    fillRight = 98;
  if (fillRight > 0) {
    display.fillRect(26, 50, fillRight, 2, SSD1306_WHITE);
  }

  // Bottom Line: Connection & hint
  display.setCursor(2, 57);
  display.setTextSize(1);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("WiFi OK");
  } else {
    display.print("WiFi ..");
  }

  display.setCursor(55, 57);
  display.print("Hold=Options");

  display.display();
}

// Live Hold Timer for Single Button showing real-time action category
void showHoldTimer(int person, unsigned long startTime) {
  unsigned long elapsed = millis() - startTime;
  float seconds = elapsed / 1000.0;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Person Header
  display.setTextSize(1);
  display.setCursor(20, 2);
  display.print("PERSON ");
  display.print(person + 1);
  display.print("  [HOLD]");
  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  // Elapsed Seconds
  display.setTextSize(2);
  display.setCursor(26, 18);
  display.print(seconds, 1);
  display.setTextSize(1);
  display.print(" sec");

  // Action Preview Box based on final hold thresholds:
  // 0 - <2s: +250 mL
  // 2s - <4s: +200 mL
  // >=4s: UNDO
  display.drawRect(10, 40, 108, 20, SSD1306_WHITE);
  display.setCursor(18, 46);
  display.setTextSize(1);

  if (elapsed < T1_THRESHOLD_MS) {
    display.print("Action: +250 mL");
  } else if (elapsed < T2_THRESHOLD_MS) {
    display.print("Action: +200 mL");
  } else {
    display.print(">> UNDO LAST <<");
  }

  display.display();
}

// Live Hold Timer for Both Buttons (Pair Switch)
void showBothTimer(unsigned long startTime) {
  unsigned long elapsed = millis() - startTime;
  float seconds = elapsed / 1000.0;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(16, 2);
  display.println("SWITCH PAIR HOLD");
  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(26, 18);
  display.print(seconds, 1);
  display.setTextSize(1);
  display.print(" sec");

  display.drawRect(10, 40, 108, 20, SSD1306_WHITE);
  display.setCursor(18, 46);

  if (elapsed < BOTH_HOLD_TIME) {
    display.print("Hold 3s to Switch");
  } else {
    display.print("SWITCHING NOW!");
  }

  display.display();
}

// Action Confirmation Feedback on OLED
void showActionFeedback(int person, String actionText) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(35, 4);
  display.print("PERSON ");
  display.println(person + 1);
  display.drawLine(15, 16, 112, 16, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(12, 24);
  display.println(actionText);

  display.setTextSize(1);
  display.setCursor(24, 48);
  display.print("Total: ");
  display.print(water[person]);
  display.print(" mL");

  display.display();
  delay(650);
  showScreen();
}

// Goal celebration screen
void congratulations(int personNum) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(24, 4);
  display.println("GOAL !!");

  display.setTextSize(1);
  display.setCursor(16, 28);
  display.print("PERSON ");
  display.print(personNum);
  display.println(" REACHED 3L");

  display.setCursor(20, 46);
  display.println("3000 mL DONE!");

  display.display();

  soundGoalComplete();
  delay(1200);
  showScreen();
}

// =====================================================
// PAIR SWITCHING
// =====================================================

void changePair() {
  secondPair = !secondPair;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(14, 10);
  display.println("SWITCHED!");

  display.setTextSize(2);
  display.setCursor(12, 34);
  if (secondPair) {
    display.println("PAIR 3 & 4");
  } else {
    display.println("PAIR 1 & 2");
  }

  display.display();
  soundSwitchPair();
  delay(800);
  showScreen();
  sendDataToServer();
}

// =====================================================
// WATER ADDITION & UNDO LOGIC
// =====================================================

void addWater(int person, int amount) {
  bool wasBelowGoal = (water[person] < GOAL_ML);

  water[person] += amount;

  // Record addition in undo history stack
  if (historyCount[person] < MAX_HISTORY) {
    history[person][historyCount[person]] = amount;
    historyCount[person]++;
  }

  saveData();

  // Buzzer feedback
  if (amount == 250) {
    soundAdd250();
  } else if (amount == 200) {
    soundAdd200();
  } else {
    soundAdd250();
  }

  showActionFeedback(person, "+" + String(amount) + " mL");

  Serial.print("Person ");
  Serial.print(person + 1);
  Serial.print(" added ");
  Serial.print(amount);
  Serial.print(" mL -> Total: ");
  Serial.print(water[person]);
  Serial.println(" mL");

  sendDataToServer();

  // Check goal completion celebration
  if (wasBelowGoal && water[person] >= GOAL_ML) {
    congratulations(person + 1);
  }
}

void undoWater(int person) {
  if (historyCount[person] <= 0) {
    Serial.println("Nothing to undo for Person " + String(person + 1));
    soundWarning();
    showActionFeedback(person, "NO HISTORY");
    return;
  }

  // Pop most recent addition
  historyCount[person]--;
  int previousAmount = history[person][historyCount[person]];

  water[person] -= previousAmount;
  if (water[person] < 0) {
    water[person] = 0;
  }

  saveData();
  soundUndo();
  showActionFeedback(person, "UNDO -" + String(previousAmount) + "mL");

  Serial.print("Person ");
  Serial.print(person + 1);
  Serial.print(" undid ");
  Serial.print(previousAmount);
  Serial.print(" mL -> Total: ");
  Serial.print(water[person]);
  Serial.println(" mL");

  sendDataToServer();
}

// =====================================================
// PROCESS SINGLE BUTTON RELEASE
// Timing thresholds:
//   0 to < 2.0s  -> +250 mL
//   2.0 to < 4.0s -> +200 mL
//   >= 4.0s       -> UNDO
// =====================================================

void processTouch(int buttonNumber, unsigned long duration) {
  if (duration < DEBOUNCE_TIME) {
    return; // Ignore jitter/accidental tap
  }

  int person = 0;
  if (buttonNumber == 1) {
    person = secondPair ? 2 : 0; // P3 or P1
  } else {
    person = secondPair ? 3 : 1; // P4 or P2
  }

  if (duration < T1_THRESHOLD_MS) {
    // 0 to less than 2 seconds -> +250 mL
    addWater(person, 250);
  } else if (duration < T2_THRESHOLD_MS) {
    // 2 seconds to less than 4 seconds -> +200 mL
    addWater(person, 200);
  } else {
    // 4 seconds or longer -> UNDO
    undoWater(person);
  }
}

// =====================================================
// SERVER COMMUNICATION
// =====================================================

void sendDataToServer() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  http.setTimeout(1200);
  http.begin(STATE_URL);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"person1\":" + String(water[0]) + ",";
  json += "\"person2\":" + String(water[1]) + ",";
  json += "\"person3\":" + String(water[2]) + ",";
  json += "\"person4\":" + String(water[3]) + ",";
  json += "\"pair\":" + String(secondPair ? 2 : 1);
  json += "}";

  int httpCode = http.POST(json);
  if (httpCode == 200) {
    // State synchronized
  } else {
    Serial.print("[HTTP] State send returned: ");
    Serial.println(httpCode);
  }
  http.end();
}

// Helpers for simple JSON parsing on ESP32 without bulky external lib
long getJsonNumber(String json, String key) {
  String search = "\"" + key + "\":";
  int start = json.indexOf(search);
  if (start < 0)
    return -1;
  start += search.length();
  int end = json.indexOf(",", start);
  if (end < 0)
    end = json.indexOf("}", start);
  if (end < 0)
    return -1;
  return json.substring(start, end).toInt();
}

String getJsonString(String json, String key) {
  String search = "\"" + key + "\":\"";
  int start = json.indexOf(search);
  if (start < 0)
    return "";
  start += search.length();
  int end = json.indexOf("\"", start);
  if (end < 0)
    return "";
  return json.substring(start, end);
}

void checkServerCommands() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  http.setTimeout(1000);
  http.begin(COMMAND_URL);

  int code = http.GET();
  if (code == 200) {
    String response = http.getString();
    long commandId = getJsonNumber(response, "id");
    String action = getJsonString(response, "action");
    long personNum = getJsonNumber(response, "person");

    if (commandId > 0 && personNum >= 1 && personNum <= 4) {
      Serial.println("[COMMAND] Executing web command #" + String(commandId) +
                     " (" + action + ") for P" + String(personNum));

      int personIdx = personNum - 1;
      if (action == "add") {
        addWater(personIdx, 250);
      } else if (action == "undo") {
        undoWater(personIdx);
      }

      // Send ACK back to server
      HTTPClient ack;
      ack.setTimeout(1000);
      ack.begin(COMMAND_ACK_URL);
      ack.addHeader("Content-Type", "application/json");
      String ackJson = "{\"id\":" + String(commandId) + "}";
      ack.POST(ackJson);
      ack.end();
    }
  }
  http.end();
}

// =====================================================
// ARDUINO SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  // Buttons & Buzzer Pins
  pinMode(TOUCH1, INPUT);
  pinMode(TOUCH2, INPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  // OLED Initialization (SDA=21, SCL=22)
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed. Check OLED wiring!");
    while (true)
      delay(100);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(18, 20);
  display.println("WATER TRACKER");
  display.setCursor(22, 36);
  display.println("Initializing...");
  display.display();

  // Storage
  preferences.begin("watertracker", false);

  // Connect Wi-Fi
  connectWiFi();

  // Configure India Time (Asia/Kolkata)
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org",
             "time.nist.gov");
  delay(1200);

  // Load Saved Data
  loadData();

  // Initial Screen
  showScreen();

  // Initial Sync
  sendDataToServer();

  Serial.println("====================================================");
  Serial.println("💧 WATER TRACKER FIRMWARE READY");
  Serial.println("Timing logic:");
  Serial.println("  Hold < 2.0s  -> +250 mL");
  Serial.println("  Hold 2.0-4.0s-> +200 mL");
  Serial.println("  Hold >= 4.0s -> UNDO last addition");
  Serial.println("  Both >= 3.0s -> Switch Pair (P1/P2 <-> P3/P4)");
  Serial.println("====================================================");
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {
  unsigned long now = millis();

  // 1. Check midnight daily reset in India every 25 seconds
  static unsigned long lastDayCheck = 0;
  if (now - lastDayCheck > 25000) {
    lastDayCheck = now;
    checkNewDay();
  }

  // 2. Wi-Fi reconnection handling
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnect = 0;
    if (now - lastReconnect > 10000) {
      lastReconnect = now;
      WiFi.reconnect();
    }
  }

  // 3. Periodic state sync to server
  if (now - lastServerUpdate > SERVER_UPDATE_INTERVAL) {
    lastServerUpdate = now;
    sendDataToServer();
  }

  // 4. Poll website commands
  if (now - lastCommandCheck > COMMAND_CHECK_INTERVAL) {
    lastCommandCheck = now;
    checkServerCommands();
  }

  // 5. Read physical inputs
  bool touch1 = digitalRead(TOUCH1);
  bool touch2 = digitalRead(TOUCH2);

  // ---------------------------------------------------
  // A. BOTH BUTTONS HELD (PAIR SWITCH LOGIC)
  // ---------------------------------------------------
  if (touch1 && touch2) {
    if (!bothActive) {
      bothActive = true;
      pairChanged = false;
      bothStart = now;
      lastTimerUpdate = 0;

      // Invalidate individual button timers so they never trigger on release
      touch1Start = 0;
      touch2Start = 0;
    }

    // Refresh live OLED switch countdown
    if (!pairChanged && (now - lastTimerUpdate >= TIMER_UPDATE)) {
      showBothTimer(bothStart);
      lastTimerUpdate = now;
    }

    // Threshold reached: Switch pair
    if (!pairChanged && (now - bothStart >= BOTH_HOLD_TIME)) {
      pairChanged = true;
      changePair();
    }

    previousTouch1 = touch1;
    previousTouch2 = touch2;
    delay(5);
    return;
  }

  // Both were active, waiting for full release
  if (bothActive) {
    if (!touch1 && !touch2) {
      // Both released: reset state safely
      bothActive = false;
      pairChanged = false;
      previousTouch1 = false;
      previousTouch2 = false;
      touch1Start = 0;
      touch2Start = 0;
      showScreen();
      delay(20);
      return;
    }
    // One still held, wait until both released
    previousTouch1 = touch1;
    previousTouch2 = touch2;
    delay(5);
    return;
  }

  // ---------------------------------------------------
  // B. SINGLE BUTTON PRESS & TIMER LOGIC
  // ---------------------------------------------------

  // Button 1 Press Start
  if (touch1 && !previousTouch1 && !touch2) {
    touch1Start = now;
    lastTimerUpdate = 0;
  }

  // Button 2 Press Start
  if (touch2 && !previousTouch2 && !touch1) {
    touch2Start = now;
    lastTimerUpdate = 0;
  }

  // Live OLED display during Button 1 hold
  if (touch1 && !touch2 && touch1Start > 0) {
    if (now - lastTimerUpdate >= TIMER_UPDATE) {
      int person = secondPair ? 2 : 0;
      showHoldTimer(person, touch1Start);
      lastTimerUpdate = now;
    }
  }

  // Live OLED display during Button 2 hold
  if (touch2 && !touch1 && touch2Start > 0) {
    if (now - lastTimerUpdate >= TIMER_UPDATE) {
      int person = secondPair ? 3 : 1;
      showHoldTimer(person, touch2Start);
      lastTimerUpdate = now;
    }
  }

  // Button 1 Release -> Determine exact final action
  if (!touch1 && previousTouch1 && !touch2) {
    if (touch1Start > 0) {
      unsigned long duration = now - touch1Start;
      touch1Start = 0;
      processTouch(1, duration);
    }
    previousTouch1 = false;
    delay(20);
    return;
  }

  // Button 2 Release -> Determine exact final action
  if (!touch2 && previousTouch2 && !touch1) {
    if (touch2Start > 0) {
      unsigned long duration = now - touch2Start;
      touch2Start = 0;
      processTouch(2, duration);
    }
    previousTouch2 = false;
    delay(20);
    return;
  }

  previousTouch1 = touch1;
  previousTouch2 = touch2;
  delay(5);
}