// **************************************************************
// ESP32 Wroom (30 Pins), Low latency (5-7ms)
// Bluetooth MorseUs CW Paddle ESP32 firmware Code v1.1
// Designed for MorseUs iOS and MorseDx Mac applications
//
//  Created by TA1AYH on 13.11.2026
//
// 2026 by tuaren@yahoo.com (73's TA1AYH)
// **************************************************************

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <WiFi.h>
#include "soc/gpio_struct.h"

#define DIT_PIN 17
#define DAH_PIN 16
#define Onboard_LED 2

#define SERVICE_UUID "E20A39F4-73F5-4BC4-A12F-17D1AD05A961"
#define TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

volatile uint8_t paddleState = 0;
bool deviceConnected = false;
bool oldDeviceConnected = false;
BLEServer *pServer;
BLECharacteristic *notifyChar;
TaskHandle_t notifyTaskHandle = NULL; 

void IRAM_ATTR handlePaddle() {
  uint32_t gpio_in = GPIO.in;
  uint8_t newState = 0;
  
  if (!(gpio_in & (1 << DIT_PIN))) newState |= 0x01;
  if (!(gpio_in & (1 << DAH_PIN))) newState |= 0x02;
  
  paddleState = newState;


  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (notifyTaskHandle != NULL) {
    vTaskNotifyGiveFromISR(notifyTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) override {
    if (deviceConnected) { 
      pServer->disconnect(pServer->getConnId());
      return;
    }
    deviceConnected = true;
    oldDeviceConnected = true;
    digitalWrite(Onboard_LED, HIGH);
    BLEDevice::stopAdvertising();

    pServer->updateConnParams(param->connect.remote_bda, 0x06, 0x0C, 0, 100);
  }

  void onDisconnect(BLEServer *pServer) override {
    deviceConnected = false; 
  }
};

void notifyTask(void *parameter) {
  uint8_t lastState = 0xFF;
    
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    uint8_t current = paddleState;
    if (deviceConnected && current != lastState) {
      notifyChar->setValue(&current, 1);
      notifyChar->notify();
      lastState = current;

      if (current == 0x00) {
        ets_delay_us(500);
        notifyChar->setValue(&current, 1);
        notifyChar->notify();
      }
    }
  }
}

void setup() {
  WiFi.mode(WIFI_OFF);
  btStop(); 

  pinMode(Onboard_LED, OUTPUT);
  digitalWrite(Onboard_LED, LOW);
  pinMode(DIT_PIN, INPUT_PULLUP);
  pinMode(DAH_PIN, INPUT_PULLUP);

  xTaskCreatePinnedToCore(
    notifyTask,
    "NotifyTask",
    2048,
    NULL,
    configMAX_PRIORITIES - 1,
    &notifyTaskHandle,
    1
  );

  attachInterrupt(DIT_PIN, handlePaddle, CHANGE);
  attachInterrupt(DAH_PIN, handlePaddle, CHANGE);

  BLEDevice::init("MorseUs CW Paddle");
  pServer = BLEDevice::createServer();
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
  BLEDevice::startAdvertising();
}

void loop() {
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    oldDeviceConnected = false;
  }

  if (!deviceConnected) {
    digitalWrite(Onboard_LED, !digitalRead(Onboard_LED));
    delay(150);
  } else {
    delay(1000); 
  }
}
