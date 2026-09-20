#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>
#include <BluetoothSerial.h>

// =====================================================
// BLUETOOTH CLASSIC
// =====================================================
BluetoothSerial SerialBT;

// =====================================================
// MOTOR PINS - L298N
// =====================================================
#define IN1 25
#define IN2 26
#define IN3 27
#define IN4 14

// =====================================================
// ULTRASONIC PINS
// =====================================================
#define F_TRIG 32
#define F_ECHO 33

#define L_TRIG 2
#define L_ECHO 35

#define R_TRIG 18
#define R_ECHO 19

#define B_TRIG 21
#define B_ECHO 22

// =====================================================
// BLE UUID
// =====================================================
#define SERVICE_UUID \
"4fafc201-1fb5-459e-8fcc-c5c9c331914b"

#define CHARACTERISTIC_UUID \
"beb5483e-36e1-4688-b7f5-ea07361b26a8"

const char* BEACON_NAME = "SARFARAZ_BEACON";

// =====================================================
// SETTINGS
// =====================================================
const int STOP_RSSI = -80;
const int MIN_DIFF = 4;

bool chairActive = false;
bool manualMode = false;
bool beaconConnected = false;

BLEClient* client = nullptr;
BLERemoteCharacteristic* remoteCharacteristic = nullptr;

// =====================================================
// MOTOR FUNCTIONS
// =====================================================

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void forwardMotor() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backwardMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeft() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// =====================================================
// ULTRASONIC
// =====================================================

long getDistance(int trigPin, int echoPin) {

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    return 400;
  }

  long distance = duration * 0.034 / 2;

  if (distance <= 0 || distance > 400) {
    return 400;
  }

  return distance;
}

// =====================================================
// AUTO OBSTACLE AVOIDANCE
// =====================================================

void checkObstacle() {

  long front = getDistance(F_TRIG, F_ECHO);
  long left  = getDistance(L_TRIG, L_ECHO);
  long right = getDistance(R_TRIG, R_ECHO);
  long back  = getDistance(B_TRIG, B_ECHO);

  Serial.print("F:");
  Serial.print(front);

  Serial.print(" L:");
  Serial.print(left);

  Serial.print(" R:");
  Serial.print(right);

  Serial.print(" B:");
  Serial.println(back);

  // ---------------- FRONT ----------------

  if (front < 30) {

    Serial.println("FRONT OBSTACLE!");

    stopMotor();
    delay(150);

    if (left > right && left > 40) {

      Serial.println("AUTO: TURN LEFT");

      turnLeft();
      delay(500);

    }
    else if (right > left && right > 40) {

      Serial.println("AUTO: TURN RIGHT");

      turnRight();
      delay(500);

    }
    else if (back > 40) {

      Serial.println("AUTO: BACKWARD");

      backwardMotor();
      delay(500);

    }
    else {

      Serial.println("AUTO: NO SAFE DIRECTION");

      stopMotor();
    }

    return;
  }

  // ---------------- LEFT ----------------

  if (left < 20) {

    Serial.println("LEFT OBSTACLE!");

    turnRight();
    delay(300);

    return;
  }

  // ---------------- RIGHT ----------------

  if (right < 20) {

    Serial.println("RIGHT OBSTACLE!");

    turnLeft();
    delay(300);

    return;
  }

  // ---------------- CLEAR ----------------

  forwardMotor();
}

// =====================================================
// BLE CALLBACK
// =====================================================

class MyClientCallback : public BLEClientCallbacks {

  void onConnect(BLEClient* pclient) {

    beaconConnected = true;

    Serial.println("BEACON CONNECTED!");
  }

  void onDisconnect(BLEClient* pclient) {

    beaconConnected = false;

    Serial.println("BEACON DISCONNECTED!");

    if (!manualMode) {
      stopMotor();
    }
  }
};

// =====================================================
// CALL MESSAGE CALLBACK
// =====================================================

static void notifyCallback(
  BLERemoteCharacteristic* characteristic,
  uint8_t* data,
  size_t length,
  bool isNotify) {

  String message = "";

  for (size_t i = 0; i < length; i++) {
    message += (char)data[i];
  }

  Serial.print("MESSAGE RECEIVED: ");
  Serial.println(message);

  if (message == "CALL") {

    Serial.println("CALL RECEIVED!");

    chairActive = true;
    manualMode = false;

    stopMotor();

    Serial.println("CHAIR ACTIVATED");
    Serial.println("AUTO MODE ACTIVATED");
  }
}

// =====================================================
// CONNECT TO BEACON
// =====================================================

bool connectToBeacon(BLEAdvertisedDevice device) {

  client = BLEDevice::createClient();

  client->setClientCallbacks(new MyClientCallback());

  Serial.println("Connecting to beacon...");

  if (!client->connect(&device)) {

    Serial.println("BEACON CONNECTION FAILED!");

    return false;
  }

  Serial.println("BEACON CONNECTED!");

  BLERemoteService* service =
      client->getService(SERVICE_UUID);

  if (service == nullptr) {

    Serial.println("SERVICE NOT FOUND!");

    client->disconnect();

    return false;
  }

  remoteCharacteristic =
      service->getCharacteristic(CHARACTERISTIC_UUID);

  if (remoteCharacteristic == nullptr) {

    Serial.println("CHARACTERISTIC NOT FOUND!");

    client->disconnect();

    return false;
  }

  if (remoteCharacteristic->canNotify()) {

    remoteCharacteristic->registerForNotify(notifyCallback);
  }

  beaconConnected = true;

  return true;
}

// =====================================================
// FIND BEACON
// =====================================================

bool findBeacon() {

  Serial.println("SCANNING FOR BEACON...");

  BLEScan* scan = BLEDevice::getScan();

  scan->setActiveScan(true);

  BLEScanResults* results =
      scan->start(5, false);

  for (int i = 0; i < results->getCount(); i++) {

    BLEAdvertisedDevice device =
        results->getDevice(i);

    if (device.haveName()) {

      String name = device.getName().c_str();

      if (name == BEACON_NAME) {

        Serial.println("BEACON FOUND!");

        return connectToBeacon(device);
      }
    }
  }

  Serial.println("BEACON NOT FOUND!");

  return false;
}

// =====================================================
// GET AVERAGE RSSI
// =====================================================

int getAverageRSSI() {

  if (!beaconConnected || client == nullptr) {
    return -120;
  }

  int total = 0;

  const int samples = 5;

  for (int i = 0; i < samples; i++) {

    int rssi = client->getRssi();

    Serial.print("RSSI: ");
    Serial.println(rssi);

    total += rssi;

    delay(100);
  }

  int average = total / samples;

  Serial.print("AVERAGE RSSI: ");
  Serial.println(average);

  return average;
}

// =====================================================
// RSSI NAVIGATION
// =====================================================

void checkBeaconDirection() {

  if (!beaconConnected) {

    Serial.println("BEACON SIGNAL NOT SEEN!");

    stopMotor();

    findBeacon();

    return;
  }

  int centerRSSI = getAverageRSSI();

  Serial.print("CENTER RSSI = ");
  Serial.println(centerRSSI);

  // =================================================
  // BEACON REACHED
  // =================================================

  if (centerRSSI >= STOP_RSSI) {

    stopMotor();

    chairActive = false;

    manualMode = true;

    Serial.println("BEACON REACHED!");
    Serial.println("MOTOR STOPPED");
    Serial.println("AUTO COMPLETE");
    Serial.println("MANUAL MODE READY");
    Serial.println("F/B/L/R/S");

    SerialBT.println("AUTO COMPLETE");
    SerialBT.println("MANUAL MODE READY");
    SerialBT.println("F/B/L/R/S");

    return;
  }

  // =================================================
  // CHECK LEFT
  // =================================================

  turnLeft();

  delay(350);

  stopMotor();

  int leftRSSI = getAverageRSSI();

  Serial.print("LEFT RSSI = ");
  Serial.println(leftRSSI);

  // =================================================
  // CENTER AGAIN
  // =================================================

  turnRight();

  delay(350);

  stopMotor();

  delay(100);

  // =================================================
  // CHECK RIGHT
  // =================================================

  turnRight();

  delay(700);

  stopMotor();

  int rightRSSI = getAverageRSSI();

  Serial.print("RIGHT RSSI = ");
  Serial.println(rightRSSI);

  // =================================================
  // CENTER AGAIN
  // =================================================

  turnLeft();

  delay(700);

  stopMotor();

  delay(100);

  // =================================================
  // DIRECTION DECISION
  // =================================================

  Serial.print("LEFT = ");
  Serial.print(leftRSSI);

  Serial.print(" | RIGHT = ");
  Serial.println(rightRSSI);

  if (leftRSSI > rightRSSI + MIN_DIFF) {

    Serial.println("RSSI: GO LEFT");

    turnLeft();
    delay(450);

  }
  else if (rightRSSI > leftRSSI + MIN_DIFF) {

    Serial.println("RSSI: GO RIGHT");

    turnRight();
    delay(450);

  }
  else {

    Serial.println("RSSI: GO FORWARD");

    forwardMotor();
    delay(500);
  }

  stopMotor();
}

// =====================================================
// MANUAL BLUETOOTH CONTROL
// =====================================================

void manualControl() {

  if (!SerialBT.available()) {
    return;
  }

  char command = SerialBT.read();

  // Ignore ENTER / SPACE
  if (command == '\r' ||
      command == '\n' ||
      command == ' ') {

    return;
  }

  command = toupper(command);

  Serial.print("MANUAL COMMAND: ");
  Serial.println(command);

  switch (command) {

    case 'F':

      forwardMotor();

      Serial.println("MANUAL: FORWARD");

      break;

    case 'B':

      backwardMotor();

      Serial.println("MANUAL: BACKWARD");

      break;

    case 'L':

      turnLeft();

      Serial.println("MANUAL: LEFT");

      break;

    case 'R':

      turnRight();

      Serial.println("MANUAL: RIGHT");

      break;

    case 'S':

      stopMotor();

      Serial.println("MANUAL: STOP");

      break;

    default:

      Serial.println("INVALID COMMAND");

      break;
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // ---------------- MOTOR ----------------

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotor();

  // ---------------- ULTRASONIC ----------------

  pinMode(F_TRIG, OUTPUT);
  pinMode(F_ECHO, INPUT);

  pinMode(L_TRIG, OUTPUT);
  pinMode(L_ECHO, INPUT);

  pinMode(R_TRIG, OUTPUT);
  pinMode(R_ECHO, INPUT);

  pinMode(B_TRIG, OUTPUT);
  pinMode(B_ECHO, INPUT);

  // ---------------- BLUETOOTH ----------------

  SerialBT.begin("SMART_WHEEL_CHAIR");

  // ---------------- BLE ----------------

  BLEDevice::init("CHAIR_CONTROLLER");

  Serial.println();
  Serial.println("================================");
  Serial.println("SMART WHEEL CHAIR");
  Serial.println("CHAIR ESP32 READY");
  Serial.println("================================");

  Serial.println("Bluetooth Name:");
  Serial.println("SMART_WHEEL_CHAIR");

  Serial.println("Waiting for CALL...");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // =================================================
  // MANUAL MODE
  // =================================================

  if (manualMode) {

    // IMPORTANT:
    // Manual mode me obstacle checking nahi hai.
    // Sirf Bluetooth command chalega.

    manualControl();

    delay(10);

    return;
  }

  // =================================================
  // AUTO MODE
  // =================================================

  if (chairActive) {

    checkObstacle();

    checkBeaconDirection();

    delay(100);

    return;
  }

  // =================================================
  // IDLE
  // =================================================

  stopMotor();

  // Beacon connected nahi hai to search karo
  if (!beaconConnected) {

    findBeacon();

    delay(1000);
  }
}
