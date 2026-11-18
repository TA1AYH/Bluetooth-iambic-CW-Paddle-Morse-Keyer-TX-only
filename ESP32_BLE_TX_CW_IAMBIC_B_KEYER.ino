// **************************************************************
// ESP32 Wroom (30 Pins), Low latency (5-7ms)
// Bluetooth MorseUs CW iambic-B Paddle firmware Code v1.2 
// Designed for MorseUs iOS and MorseDx Mac applications
//
//  Created by TA1AYH on 13.11.2026
//
// April 2026 by tuaren@yahoo.com (73's TA1AYH)
// **************************************************************

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <WiFi.h>
#include "soc/gpio_struct.h"
//#include <stdio.h>

#define DIT_PIN 17
#define DAH_PIN 16
#define BUZZER_PIN 18
#define Onboard_LED 2

#define SERVICE_UUID "E20A39F4-73F5-4BC4-A12F-17D1AD05A961"
#define TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// constexpr uint16_t BUZZER_FREQ = 700;
constexpr uint32_t DIT_MS = 1200UL / 23UL; // 23 WPM dit length ~52 ms
constexpr uint32_t DAH_MS = DIT_MS * 3UL;

volatile uint8_t paddleState = 0;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool tek = false;
bool playing = false;
bool iambicB = false;
BLEServer *pServer;
BLECharacteristic *notifyChar;
TaskHandle_t notifyTaskHandle = NULL;

// Ultra hızlı Register bazlı ISR
void IRAM_ATTR handlePaddle()
{
  uint32_t gpio_in = GPIO.in;
  uint8_t newState = 0;
  if (!(gpio_in & (1 << DIT_PIN)))
  {
    newState |= 0x01;
  }
  if (!(gpio_in & (1 << DAH_PIN)))
  {
    newState |= 0x02;
  }

  paddleState = newState;

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (notifyTaskHandle != NULL)
  {
    vTaskNotifyGiveFromISR(notifyTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) override
  {
    if (deviceConnected)
    {
      pServer->disconnect(pServer->getConnId());
      return;
    }
    deviceConnected = true;
    oldDeviceConnected = true;
    digitalWrite(Onboard_LED, HIGH);
    BLEDevice::stopAdvertising();

    if (!pServer->requestConnParams(param->connect.remote_bda, 0x06, 0x0C, 0, 100))
    {
      log_e("Failed to request BLE connection parameters");
    }
  }

  void onDisconnect(BLEServer *pServer) override
  {
    deviceConnected = false;
  }
};

void notifyTask(void *parameter)
{
  uint8_t lastState = 0xFF;

  while (true)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    uint8_t current = paddleState;
    if (deviceConnected && current != lastState)
    {
      notifyChar->setValue(&current, 1);
      notifyChar->notify();
      lastState = current;

      if (current == 0x00)
      {
        ets_delay_us(500);
        notifyChar->setValue(&current, 1);
        notifyChar->notify();
      }
    }
    else if (!deviceConnected && current != lastState)
    {
      lastState = current;

      // printf("paddleState: %1x\n current: 0x%02X\n lastState: 0x%02X\n", paddleState,current, lastState);

      if (paddleState == 0x03)
      {
        if (!iambicB)
        {
          iambicB = true;
          //printf("iambicB: %d\n", iambicB);
        }
      }
      else
      {
        //printf("paddleState: %1X\n iambicB: %d\n", paddleState, iambicB);
      }
      if (current == 0x00)
      {
        ets_delay_us(500);
      }
    }
  }
}

void setup()
{

  WiFi.mode(WIFI_OFF);
  btStop();

  pinMode(Onboard_LED, OUTPUT);
  digitalWrite(Onboard_LED, LOW);
  pinMode(DIT_PIN, INPUT_PULLUP);
  pinMode(DAH_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  xTaskCreatePinnedToCore(
      notifyTask,
      "NotifyTask",
      2048,
      NULL,
      configMAX_PRIORITIES - 1,
      &notifyTaskHandle,
      1);

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

void playDit()
{
  if (!playing)
  {
    playing = true;
    tek = true;
    digitalWrite(BUZZER_PIN, HIGH);
    delay(DIT_MS);
    digitalWrite(BUZZER_PIN, LOW);
    delay(DIT_MS);
    playing = false;
  }
}
void playDah()
{
  if (!playing)
  {
    playing = true;
    tek = false;
    digitalWrite(BUZZER_PIN, HIGH);
    delay(DAH_MS);
    digitalWrite(BUZZER_PIN, LOW);
    delay(DIT_MS);
    playing = false;
  }
}

void loop()
{
  if (!deviceConnected)
  {
    if (iambicB)
    {
      //printf("iambicB playing: %d\n", iambicB);

      if (iambicB && !playing)
      {
        iambicB = false;
        if (tek)
        {
          playDah();
        }
        else
        {
          playDit();
        }
        //printf("iambicB played: %d\n", iambicB);
      }
    }
    else if (paddleState == 0x03)
    {
      if (tek)
      {
        playDah();
      }
      else
      {
        playDit();
      }
    }
    else if (paddleState == 0x02)
    {
      playDah();
    }
    else if (paddleState == 0x01)
    {
      playDit();
    }
    if (oldDeviceConnected)
    {
      delay(500);
      pServer->startAdvertising();
      oldDeviceConnected = false;
    }
  }
  else
  {
    delay(1000);
  }
}
