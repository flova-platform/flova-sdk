#include <Arduino.h>
#include <FlovaSDK.h>

const uint8_t TOUCH_PIN = D1;
const uint8_t LED_PIN = D2;
const uint32_t DEBOUNCE_MS = 60;


FlovaEsp8266 device;
auto led = FLOVA_DATASTREAM(device, bool, "LED");
auto touch = FLOVA_DATASTREAM(device, bool, "TOUCH_SENSOR");

bool touchState = false;
bool lastRawTouch = false;
uint32_t rawTouchChangedAt = 0;
uint32_t restartRequestedAt = 0;

void scheduleRestart(void*, FlovaRestartReason) {
  const uint32_t now = millis();
  restartRequestedAt = now ? now : 1;
}

void writeLed(bool enabled) { digitalWrite(LED_PIN, enabled ? HIGH : LOW); }

void pollTouch() {
  const uint32_t now = millis();
  const bool rawTouch = digitalRead(TOUCH_PIN) == HIGH;

  if (rawTouch != lastRawTouch) {
    lastRawTouch = rawTouch;
    rawTouchChangedAt = now;
  }

  if (rawTouch == touchState || now - rawTouchChangedAt < DEBOUNCE_MS)
    return;

  touchState = rawTouch;
  FLOVA_REPORT(touch, touchState, flova::Origin::PhysicalInput);
  if (touchState) FLOVA_WRITE(led, !FLOVA_HAS_VALUE(led) || !FLOVA_READ(led));
}

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  FLOVA_ON_WRITE(led, writeLed);
  led.offline(flova::OfflinePolicy::KeepLatest);
  touch.offline(flova::OfflinePolicy::KeepLatest);

  device.setFirmwareTarget("esp8266-touch-test-ota");
  device.enableOta(true);
  device.setOtaProfile(FlovaOtaStrategy::Ab, "esp8266-staged-copy", false);
  device.setRestartHandler(scheduleRestart);

  if (!device.begin()) Serial.println("[flova] startup failed");
}

void loop() {
  device.run();
  if (restartRequestedAt && millis() - restartRequestedAt >= 1000UL) {
    restartRequestedAt = 0;
    ESP.restart();
  }
  pollTouch();
  yield();
}
