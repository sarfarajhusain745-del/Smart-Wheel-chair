#include <WiFi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// ================= WiFi =================
const char* ssid = "Sarfaraj";
const char* password = "12345678";

// ================= Telegram =================
// Yahan apna NAYA Telegram Bot Token paste karo
const char* BOT_TOKEN = "8523112760:AAHJZYFrvYtXAW53FNCtmANeCnFzcHCjSeo";

// Chat ID
const char* CHAT_ID = "7233875973";

// ================= Buttons =================
#define CALL_BUTTON 4
#define EMERGENCY_BUTTON 5

// ================= BLE =================
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic;

// ================= Telegram Function =================
void sendTelegramMessage(String message) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi NOT connected!");
    return;
  }

  HTTPClient http;

  String url = "https://api.telegram.org/bot";
  url += BOT_TOKEN;
  url += "/sendMessage?chat_id=";
  url += CHAT_ID;
  url += "&text=";

  message.replace(" ", "%20");
  url += message;

  http.begin(url);

  int httpCode = http.GET();

  Serial.print("Telegram HTTP Code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    Serial.println("Telegram message sent!");
  } else {
    Serial.println("Telegram message failed!");
  }

  http.end();
}

// ================= BLE Setup =================
void setupBLE() {

  BLEDevice::init("SARFARAZ_BEACON");

  BLEServer *pServer = BLEDevice::createServer();

  BLEService *pService =
      pServer->createService(SERVICE_UUID);

  pCharacteristic =
      pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
      );

  pCharacteristic->setValue("READY");

  pService->start();

  BLEAdvertising *pAdvertising =
      BLEDevice::getAdvertising();

  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);

  BLEDevice::startAdvertising();

  Serial.println("BLE BEACON STARTED");
  Serial.println("Name: SARFARAZ_BEACON");
}

// ================= Setup =================
void setup() {

  Serial.begin(115200);

  pinMode(CALL_BUTTON, INPUT_PULLUP);
  pinMode(EMERGENCY_BUTTON, INPUT_PULLUP);

  // WiFi
  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  int count = 0;

  while (WiFi.status() != WL_CONNECTED && count < 30) {
    delay(500);
    Serial.print(".");
    count++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi CONNECTED");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi NOT CONNECTED");
  }

  // BLE
  setupBLE();

  Serial.println("--------------------------------");
  Serial.println("LOCATION / BEACON READY");
  Serial.println("CALL Button = GPIO 4");
  Serial.println("Emergency Button = GPIO 5");
  Serial.println("--------------------------------");
}

// ================= Loop =================
void loop() {

  // -------- CALL BUTTON --------
  if (digitalRead(CALL_BUTTON) == LOW) {

    Serial.println();
    Serial.println("================================");
    Serial.println("CALL BUTTON PRESSED");
    Serial.println("CALL SENT");
    Serial.println("================================");

    // BLE CALL message
    pCharacteristic->setValue("CALL");
    pCharacteristic->notify();

    delay(100);

    // Also write value
    pCharacteristic->setValue("CALL");

    // Button debounce
    while (digitalRead(CALL_BUTTON) == LOW) {
      delay(10);
    }

    delay(1000);
  }

  // -------- EMERGENCY BUTTON --------
  if (digitalRead(EMERGENCY_BUTTON) == LOW) {

    Serial.println();
    Serial.println("================================");
    Serial.println("EMERGENCY BUTTON PRESSED");
    Serial.println("Sending Telegram Alert...");
    Serial.println("================================");

    sendTelegramMessage(
      "EMERGENCY ALERT! Please check immediately."
    );

    // Wait until button released
    while (digitalRead(EMERGENCY_BUTTON) == LOW) {
      delay(10);
    }

    delay(2000);
  }

  delay(20);
}
