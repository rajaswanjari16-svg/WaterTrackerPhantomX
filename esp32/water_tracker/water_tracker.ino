#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>

// =====================================================
// WIFI
// =====================================================

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// YOUR LAPTOP IP
const char* SERVER_URL =
  "http://192.168.101.97:3000/api/state";

// =====================================================
// OLED
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// =====================================================
// PINS
// =====================================================

#define TOUCH1 32
#define TOUCH2 33
#define BUZZER 13

// =====================================================
// WATER SETTINGS
// =====================================================

#define DAILY_GLASS_GOAL 8
#define GLASS_SIZE_ML 250
#define GOAL_ML (DAILY_GLASS_GOAL * GLASS_SIZE_ML)

// =====================================================
// TIMING
// =====================================================

#define SHORT_LIMIT 2000
#define BOTH_HOLD_TIME 3000
#define TIMER_UPDATE 50

// =====================================================
// WATER TOTALS
// =====================================================

int water[4] = {
  0, 0, 0, 0
};

// =====================================================
// HISTORY
// =====================================================

#define MAX_HISTORY 30

int history[4][MAX_HISTORY];

int historyCount[4] = {
  0, 0, 0, 0
};

// =====================================================
// CURRENT PAIR
// =====================================================

bool secondPair = false;

// =====================================================
// TOUCH STATES
// =====================================================

bool previousTouch1 = false;
bool previousTouch2 = false;

unsigned long touch1Start = 0;
unsigned long touch2Start = 0;

// =====================================================
// BOTH BUTTON STATE
// =====================================================

bool bothActive = false;
bool pairChanged = false;

unsigned long bothStart = 0;

// =====================================================
// OLED TIMER
// =====================================================

unsigned long lastTimerUpdate = 0;

// =====================================================
// SEND DATA TO SERVER
// =====================================================

void sendDataToServer() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi not connected.");
    return;
  }

  HTTPClient http;

  http.begin(SERVER_URL);

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  String json = "{";

  json += "\"person1\":";
  json += water[0];

  json += ",\"person2\":";
  json += water[1];

  json += ",\"person3\":";
  json += water[2];

  json += ",\"person4\":";
  json += water[3];

  json += "}";

  Serial.println();
  Serial.println("Sending data to server:");
  Serial.println(json);

  int responseCode =
    http.POST(json);

  Serial.print("Server response: ");
  Serial.println(responseCode);

  if (responseCode > 0) {

    String response =
      http.getString();

    Serial.println("Server says:");
    Serial.println(response);
  }

  http.end();
}

// =====================================================
// BUZZER
// =====================================================

void beep(int duration) {

  digitalWrite(
    BUZZER,
    HIGH
  );

  delay(duration);

  digitalWrite(
    BUZZER,
    LOW
  );
}

// =====================================================
// GET GLASS COUNT
// =====================================================

int getGlasses(int person) {

  return water[person] / GLASS_SIZE_ML;
}

// =====================================================
// NORMAL OLED SCREEN
// =====================================================

void showScreen() {

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  // HEADER
  display.setTextSize(1);

  display.setCursor(32, 0);

  if (secondPair) {

    display.println(
      "PERSON 3 & 4"
    );

  } else {

    display.println(
      "PERSON 1 & 2"
    );
  }

  display.drawLine(
    0,
    10,
    127,
    10,
    SSD1306_WHITE
  );

  // SELECT PEOPLE

  int pLeft;
  int pRight;

  if (secondPair) {

    pLeft = 2;
    pRight = 3;

  } else {

    pLeft = 0;
    pRight = 1;
  }

  // ===================================================
  // LEFT PERSON
  // ===================================================

  display.setTextSize(1);

  display.setCursor(0, 17);

  display.print("P");
  display.print(pLeft + 1);

  display.setCursor(25, 16);

  display.print(
    getGlasses(pLeft)
  );

  display.print("/");

  display.print(
    DAILY_GLASS_GOAL
  );

  display.setCursor(70, 16);

  display.print(
    water[pLeft]
  );

  display.print("ml");

  // Progress bar

  display.drawRect(
    25,
    27,
    100,
    6,
    SSD1306_WHITE
  );

  int widthLeft =
    (getGlasses(pLeft) * 96)
    / DAILY_GLASS_GOAL;

  if (widthLeft > 96) {

    widthLeft = 96;
  }

  if (widthLeft > 0) {

    display.fillRect(
      27,
      29,
      widthLeft,
      2,
      SSD1306_WHITE
    );
  }

  // ===================================================
  // RIGHT PERSON
  // ===================================================

  display.setCursor(0, 40);

  display.print("P");
  display.print(pRight + 1);

  display.setCursor(25, 39);

  display.print(
    getGlasses(pRight)
  );

  display.print("/");

  display.print(
    DAILY_GLASS_GOAL
  );

  display.setCursor(70, 39);

  display.print(
    water[pRight]
  );

  display.print("ml");

  // Progress bar

  display.drawRect(
    25,
    50,
    100,
    6,
    SSD1306_WHITE
  );

  int widthRight =
    (getGlasses(pRight) * 96)
    / DAILY_GLASS_GOAL;

  if (widthRight > 96) {

    widthRight = 96;
  }

  if (widthRight > 0) {

    display.fillRect(
      27,
      52,
      widthRight,
      2,
      SSD1306_WHITE
    );
  }

  display.display();
}

// =====================================================
// HOLD TIMER
// =====================================================

void showHoldTimer(
  unsigned long startTime
) {

  unsigned long elapsed =
    millis() - startTime;

  float seconds =
    elapsed / 1000.0;

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(48, 5);

  display.println("HOLD");

  display.setTextSize(3);

  display.setCursor(18, 25);

  display.print(
    seconds,
    1
  );

  display.setTextSize(1);

  display.print(" sec");

  display.display();
}

// =====================================================
// BOTH BUTTON TIMER
// =====================================================

void showBothTimer(
  unsigned long startTime
) {

  unsigned long elapsed =
    millis() - startTime;

  float seconds =
    elapsed / 1000.0;

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(45, 4);

  display.println("SWITCH");

  display.setTextSize(3);

  display.setCursor(18, 24);

  display.print(
    seconds,
    1
  );

  display.setTextSize(1);

  display.print(" sec");

  display.display();
}

// =====================================================
// CONGRATULATIONS
// =====================================================

void congratulations(
  int person
) {

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(12, 0);

  display.println("GREAT!");

  display.setCursor(25, 23);

  display.print("PERSON ");

  display.println(person);

  display.setTextSize(1);

  display.setCursor(30, 46);

  display.println(
    "GOAL COMPLETE"
  );

  display.display();

  beep(150);

  delay(100);

  beep(150);

  delay(100);

  beep(150);

  delay(100);

  beep(400);

  delay(2000);

  showScreen();
}

// =====================================================
// ADD ONE GLASS
// =====================================================

void addGlass(
  int person
) {

  if (
    water[person] >= GOAL_ML
  ) {

    beep(300);

    Serial.print(
      "Person "
    );

    Serial.print(
      person + 1
    );

    Serial.println(
      " already completed goal."
    );

    return;
  }

  int amount =
    GLASS_SIZE_ML;

  if (
    water[person] + amount
    > GOAL_ML
  ) {

    amount =
      GOAL_ML - water[person];
  }

  water[person] += amount;

  // SAVE HISTORY

  if (
    historyCount[person]
    < MAX_HISTORY
  ) {

    history[person]
           [historyCount[person]]
      = amount;

    historyCount[person]++;
  }

  beep(100);

  showScreen();

  Serial.print("Person ");
  Serial.print(person + 1);

  Serial.print(" added ");
  Serial.print(amount);

  Serial.print(" mL -> ");

  Serial.print(
    water[person]
  );

  Serial.print(" mL (");

  Serial.print(
    getGlasses(person)
  );

  Serial.println("/8)");

  // SEND TO WEBSITE SERVER

  sendDataToServer();

  // GOAL COMPLETE

  if (
    water[person] >= GOAL_ML
  ) {

    congratulations(
      person + 1
    );
  }
}

// =====================================================
// UNDO
// =====================================================

void undoGlass(
  int person
) {

  if (
    historyCount[person] <= 0
  ) {

    beep(300);

    Serial.print(
      "Person "
    );

    Serial.print(
      person + 1
    );

    Serial.println(
      ": nothing to undo."
    );

    return;
  }

  int previousGlass =
    history[person]
           [historyCount[person] - 1];

  water[person] -=
    previousGlass;

  if (
    water[person] < 0
  ) {

    water[person] = 0;
  }

  historyCount[person]--;

  beep(250);

  showScreen();

  Serial.print("Person ");
  Serial.print(person + 1);

  Serial.print(" undo ");

  Serial.print(previousGlass);

  Serial.print(" mL -> ");

  Serial.print(
    water[person]
  );

  Serial.print(" mL (");

  Serial.print(
    getGlasses(person)
  );

  Serial.println("/8)");

  // SEND UPDATED DATA

  sendDataToServer();
}

// =====================================================
// CHANGE PAIR
// =====================================================

void changePair() {

  secondPair =
    !secondPair;

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(18, 15);

  display.println("PERSON");

  display.setCursor(20, 40);

  if (secondPair) {

    display.println("3 & 4");

  } else {

    display.println("1 & 2");
  }

  display.display();

  beep(150);

  delay(100);

  beep(150);

  delay(1000);

  showScreen();
}

// =====================================================
// PROCESS TOUCH
// =====================================================

void processTouch(
  int buttonNumber,
  unsigned long duration
) {

  int person;

  if (buttonNumber == 1) {

    if (secondPair) {

      person = 2;

    } else {

      person = 0;
    }

  } else {

    if (secondPair) {

      person = 3;

    } else {

      person = 1;
    }
  }

  // SHORT TOUCH = ADD

  if (
    duration < SHORT_LIMIT
  ) {

    addGlass(person);

    return;
  }

  // LONG HOLD = UNDO

  undoGlass(person);
}

// =====================================================
// RESET DATA
// =====================================================

void resetData() {

  for (
    int p = 0;
    p < 4;
    p++
  ) {

    water[p] = 0;

    historyCount[p] = 0;

    for (
      int i = 0;
      i < MAX_HISTORY;
      i++
    ) {

      history[p][i] = 0;
    }
  }

  secondPair = false;
}

// =====================================================
// WIFI CONNECTION
// =====================================================

void connectWiFi() {

  Serial.println();
  Serial.println(
    "Connecting to WiFi..."
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED
    && attempts < 30
  ) {

    delay(500);

    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (
    WiFi.status() == WL_CONNECTED
  ) {

    Serial.println(
      "WiFi connected!"
    );

    Serial.print(
      "ESP32 IP: "
    );

    Serial.println(
      WiFi.localIP()
    );

  } else {

    Serial.println(
      "WiFi connection failed."
    );
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // TOUCH

  pinMode(
    TOUCH1,
    INPUT
  );

  pinMode(
    TOUCH2,
    INPUT
  );

  // BUZZER

  pinMode(
    BUZZER,
    OUTPUT
  );

  digitalWrite(
    BUZZER,
    LOW
  );

  // OLED

  Wire.begin(
    21,
    22
  );

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDR
    )
  ) {

    Serial.println(
      "OLED NOT FOUND"
    );

    while (true) {

      delay(100);
    }
  }

  // RESET

  resetData();

  // WIFI

  connectWiFi();

  // OLED

  showScreen();

  // SERIAL

  Serial.println();
  Serial.println(
    "=============================="
  );

  Serial.println(
    "       WATER TRACKER"
  );

  Serial.println(
    "=============================="
  );

  Serial.println(
    "4 PEOPLE"
  );

  Serial.println(
    "8 GLASSES PER PERSON"
  );

  Serial.println(
    "250 mL PER GLASS"
  );

  Serial.println(
    "2000 mL DAILY GOAL"
  );

  Serial.println();

  Serial.println(
    "Short touch = +250 mL"
  );

  Serial.println(
    "Hold >=2 sec = Undo"
  );

  Serial.println(
    "Both buttons 3 sec = Change pair"
  );

  Serial.println(
    "=============================="
  );
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  bool touch1 =
    digitalRead(TOUCH1);

  bool touch2 =
    digitalRead(TOUCH2);

  unsigned long now =
    millis();

  // ===================================================
  // BOTH BUTTONS
  // ===================================================

  if (
    touch1 && touch2
  ) {

    if (!bothActive) {

      bothActive = true;

      pairChanged = false;

      bothStart = now;

      touch1Start = 0;

      touch2Start = 0;

      lastTimerUpdate = 0;
    }

    if (
      !pairChanged &&
      now - lastTimerUpdate
      >= TIMER_UPDATE
    ) {

      showBothTimer(
        bothStart
      );

      lastTimerUpdate = now;
    }

    if (
      !pairChanged &&
      now - bothStart
      >= BOTH_HOLD_TIME
    ) {

      pairChanged = true;

      changePair();
    }

    previousTouch1 = touch1;

    previousTouch2 = touch2;

    delay(5);

    return;
  }

  // ===================================================
  // BOTH MODE ENDED
  // ===================================================

  if (bothActive) {

    if (
      !touch1 &&
      !touch2
    ) {

      bothActive = false;

      pairChanged = false;

      touch1Start = 0;

      touch2Start = 0;

      previousTouch1 = false;

      previousTouch2 = false;

      showScreen();

      delay(20);

      return;
    }

    if (
      touch1 ||
      touch2
    ) {

      previousTouch1 = touch1;

      previousTouch2 = touch2;

      delay(5);

      return;
    }
  }

  // ===================================================
  // TOUCH 1 START
  // ===================================================

  if (
    touch1 &&
    !previousTouch1
  ) {

    touch1Start = now;

    lastTimerUpdate = 0;
  }

  // ===================================================
  // TOUCH 2 START
  // ===================================================

  if (
    touch2 &&
    !previousTouch2
  ) {

    touch2Start = now;

    lastTimerUpdate = 0;
  }

  // ===================================================
  // TIMER TOUCH 1
  // ===================================================

  if (
    touch1 &&
    !touch2
  ) {

    if (
      now - lastTimerUpdate
      >= TIMER_UPDATE
    ) {

      showHoldTimer(
        touch1Start
      );

      lastTimerUpdate = now;
    }
  }

  // ===================================================
  // TIMER TOUCH 2
  // ===================================================

  if (
    touch2 &&
    !touch1
  ) {

    if (
      now - lastTimerUpdate
      >= TIMER_UPDATE
    ) {

      showHoldTimer(
        touch2Start
      );

      lastTimerUpdate = now;
    }
  }

  // ===================================================
  // TOUCH 1 RELEASE
  // ===================================================

  if (
    !touch1 &&
    previousTouch1 &&
    !touch2
  ) {

    unsigned long duration =
      now - touch1Start;

    touch1Start = 0;

    processTouch(
      1,
      duration
    );

    previousTouch1 = false;

    delay(20);

    return;
  }

  // ===================================================
  // TOUCH 2 RELEASE
  // ===================================================

  if (
    !touch2 &&
    previousTouch2 &&
    !touch1
  ) {

    unsigned long duration =
      now - touch2Start;

    touch2Start = 0;

    processTouch(
      2,
      duration
    );

    previousTouch2 = false;

    delay(20);

    return;
  }

  // ===================================================
  // UPDATE STATES
  // ===================================================

  previousTouch1 = touch1;

  previousTouch2 = touch2;

  delay(5);
}