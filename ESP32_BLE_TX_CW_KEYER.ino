// **************************************************************
// ESP32 Wroom (30 Pins), Low latency (5-7ms)
// Bluetooth MorseUs CW Paddle firmware Code v1.0
// Designed for MorseUs iOS and MorseDx Mac applications
//
//  Created by Mehmet SIMSEK on 14.03.2026.
//  Copyright © 2020 Mehmet SIMSEK. All rights reserved.
//
// April 2026 by tuaren@yahoo.com (73's TA1AYH)
// **************************************************************

// Code Start s
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <WiFi.h>

//////////////////////////////
// Pins
//////////////////////////////
#define DIT_PIN 17
#define DAH_PIN 16
#define Onboard_LED 2  // Genelde GPIO2

//////////////////////////////
// BLE UUID (Nordic UART)
//////////////////////////////
#define SERVICE_UUID "E20A39F4-73F5-4BC4-A12F-17D1AD05A961"
#define TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // Notify
#define RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Write
//////////////////////////////
// Globals
//////////////////////////////
volatile uint8_t paddleState = 0;  // 0x01 dit, 0x02 dah
bool deviceConnected = false;
bool oldDeviceConnected = false;
BLEServer *pServer;
BLECharacteristic *notifyChar;

//////////////////////////////
// ISR – ultra-fast
//////////////////////////////
void IRAM_ATTR handleDIT() {
  if (!digitalRead(DIT_PIN))
    paddleState |= 0x01;
  else
    paddleState &= ~0x01;
}

void IRAM_ATTR handleDAH() {
  if (!digitalRead(DAH_PIN))
    paddleState |= 0x02;
  else
    paddleState &= ~0x02;
}

//////////////////////////////
// BLE Server Callbacks
//////////////////////////////
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    if (deviceConnected) {
      // zaten biri bağlı → yeni bağlantıyı kes
      pServer->disconnect(pServer->getConnId());
      return;
    }
    deviceConnected = true;
    oldDeviceConnected = true;  // Tekrar bağlanınca bu bayrağı güncelleyin
    digitalWrite(Onboard_LED, HIGH);
    BLEDevice::stopAdvertising();
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    // Burada start() çağırmanıza gerek yok, loop halledecek
  }
};

//////////////////////////////
// Notify Task
//////////////////////////////
void notifyTask(void *parameter) {
  uint8_t lastState = 0xFF;
  while (true) {
    uint8_t current = paddleState;
    if (deviceConnected && current != lastState) {
      notifyChar->setValue(&current, 1);
      notifyChar->notify();
      //Serial.print("📤 ESP -> iOS: ");
      //Serial.println(current, HEX);
      lastState = current;

      if (current == 0x00) {
        vTaskDelay(1);  // 1 tick
        notifyChar->setValue(&current, 1);
        notifyChar->notify();
        //Serial.print("📤 ESP -> iOS: ");
        //Serial.println(current, HEX);
      }
    }
    vTaskDelay(1);  // 1 tick
  }
}

//////////////////////////////
// Setup
//////////////////////////////
void setup() {
  //Serial.begin(115200);
  // Serial.println("MorseUs CW Paddle starting...");

  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true);

  pinMode(Onboard_LED, OUTPUT);
  digitalWrite(Onboard_LED, LOW);
  pinMode(DIT_PIN, INPUT_PULLUP);
  pinMode(DAH_PIN, INPUT_PULLUP);

  attachInterrupt(DIT_PIN, handleDIT, CHANGE);
  attachInterrupt(DAH_PIN, handleDAH, CHANGE);

  // BLE init
  BLEDevice::init("MorseUs CW Paddle");
  // if you do not want to loose iOS device Auto Connect feature.
  // do not change the "MorseUs CW Paddle" name

  pServer = BLEDevice::createServer();  // 'server' yerine global 'pServer'ı kullanın
  pServer->setCallbacks(new ServerCallbacks());


  BLEService *service = pServer->createService(SERVICE_UUID);
  notifyChar = service->createCharacteristic(
    TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY);
  notifyChar->addDescriptor(new BLE2902());
  service->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);

  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  pAdvertising->setMinInterval(0x20);
  pAdvertising->setMaxInterval(0x40);
  BLEDevice::startAdvertising();

  // Notify Task
  xTaskCreate(
    notifyTask,
    "NotifyTask",
    2048,
    NULL,
    2,
    NULL);
}

//////////////////////////////
// Loop
//////////////////////////////
void loop() {
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);  // Kısa bir bekleme
    pServer->startAdvertising();
    //pServer->getAdvertising()->start();
    oldDeviceConnected = false;
  }

  if (!deviceConnected) {
    digitalWrite(Onboard_LED, HIGH);
    delay(100);
    digitalWrite(Onboard_LED, LOW);
    delay(100);
  }
}

// Code End
// *************************************************************