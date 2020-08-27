#include <Arduino.h>

#if defined(ESP32)
#include <SPIFFS.h>
#define FLASHFS SPIFFS
#else
#include <LittleFS.h>
#define FLASHFS LittleFS
#endif

#include "LocalDNS.h"
#include "MyMatrix.h"
#include "EffectsManager.h"
#include "Settings.h"
#include "TimeClient.h"

#include "GyverButton.h"
#include "LampWebServer.h"

#include "effects/Effect.h"

#include "Spectrometer.h"
#include "MqttClient.h"

namespace  {

uint16_t webServerPort = 80;

#if defined(ESP32)
const uint8_t btnPin = 15;
const GButton::PullType btnType = GButton::PullTypeLow;
TaskHandle_t processMatrixTaskHandle = 0;
TaskHandle_t userTaskHandle = 0;
#elif defined(SONOFF)
const uint8_t btnPin = 0;
const GButton::PullType btnType = GButton::PullTypeHigh;
const uint8_t relayPin = 12;
const uint8_t miniLedPin = 13;
#else
const uint8_t btnPin = D2;
const GButton::PullType btnType = GButton::PullTypeLow;
#endif
const GButton::DefaultState btnState = GButton::DefaultStateOpen;

GButton *button = nullptr;

int stepDirection = 1;
bool isHolding = false;

uint32_t logTimer = 0;

bool setupMode = false;
bool connectFinished = false;

void processMatrix()
{
    if (mySettings->generalSettings.working) {
        effectsManager->loop();
    } else {
        myMatrix->clear(true);
    }
}

#if defined(ESP32)
void processMatrix32()
{
    if (userTaskHandle == 0) {
        noInterrupts();
        userTaskHandle = xTaskGetCurrentTaskHandle();
        xTaskNotifyGive(processMatrixTaskHandle);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));
        interrupts();
        userTaskHandle = 0;
    }
}

void processMatrixTask(void *)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        processMatrix();
        xTaskNotifyGive(userTaskHandle);
    }
}
#endif

void printFlashInfo()
{
    uint32_t ideSize = ESP.getFlashChipSize();
    FlashMode_t ideMode = ESP.getFlashChipMode();

    Serial.printf_P(PSTR("Flash ide  size: %u bytes\n"), ideSize);
    Serial.printf_P(PSTR("Flash ide speed: %u Hz\n"), ESP.getFlashChipSpeed());
    Serial.print(F("Flash ide mode:  "));
    Serial.println((ideMode == FM_QIO ? F("QIO") : ideMode == FM_QOUT ? F("QOUT") : ideMode == FM_DIO ? F("DIO") : ideMode == FM_DOUT ? F("DOUT") : F("UNKNOWN")));

#if defined(ESP8266)
    uint32_t realSize = ESP.getFlashChipRealSize();
    Serial.printf_P(PSTR("Flash real id:   %08X\n"), ESP.getFlashChipId());
    Serial.printf_P(PSTR("Flash real size: %u bytes\n\n"), realSize);
    if (ideSize != realSize) {
      Serial.println(F("Flash Chip configuration wrong!"));
    } else {
      Serial.println(F("Flash Chip configuration ok."));
    }
#endif

    Serial.print(F("Sketch size: "));
    Serial.println(ESP.getSketchSize());
    Serial.print(F("Sketch free: "));
    Serial.println(ESP.getFreeSketchSpace());

#if defined(ESP32)
    Serial.print(F("Total heap: "));
    Serial.println(ESP.getHeapSize());
    Serial.print(F("Min free heap: "));
    Serial.println(ESP.getMinFreeHeap());
    Serial.print(F("Max alloc heap: "));
    Serial.println(ESP.getMaxAllocHeap());
#endif
}

void printFreeHeap()
{
    Serial.print(F("FreeHeap: "));
    Serial.println(ESP.getFreeHeap());
}

void processButton()
{
    if (mySettings->buttonSettings.pin == 0) {
        return;
    }
    button->tick();
    if (button->isSingle()) {
        Serial.println(F("Single button"));
        mySettings->generalSettings.working = !mySettings->generalSettings.working;
        mySettings->saveLater();
    }
    if (!mySettings->generalSettings.working) {
        return;
    }
    if (button->isDouble()) {
        Serial.println(F("Double button"));
        effectsManager->next();
        mySettings->saveLater();
    }
    if (button->isTriple()) {
        Serial.println(F("Triple button"));
        effectsManager->previous();
        mySettings->saveLater();
    }
    if (button->isHolded()) {
        Serial.println(F("Holded button"));
        isHolding = true;
        const uint8_t brightness = effectsManager->activeEffect()->settings.brightness;
        if (brightness <= 1) {
            stepDirection = 1;
        } else if (brightness == 255) {
            stepDirection = -1;
        }
    }
    if (isHolding && button->isStep()) {
        uint8_t brightness = effectsManager->activeEffect()->settings.brightness;
        if (stepDirection < 0 && brightness == 1) {
            return;
        }
        if (stepDirection > 0 && brightness == 255) {
            return;
        }
        brightness += stepDirection;
        Serial.printf_P(PSTR("Step button %d. brightness: %u\n"), stepDirection, brightness);
        effectsManager->activeEffect()->settings.brightness = brightness;
        myMatrix->setBrightness(brightness);
        mySettings->saveLater();
    }
    if (button->isRelease() && isHolding) {
        Serial.println(F("Release button"));
        mySettings->saveLater();
        isHolding = false;
    }
}

void setupSerial()
{
    Serial.begin(115200);
    Serial.println(F("\nHappy debugging!"));
    Serial.flush();
}

}

void setup() {
#if defined(ESP8266)
    ESP.wdtDisable();
    ESP.wdtEnable(0);
#endif

    setupSerial();
    printFlashInfo();
    printFreeHeap();

    if(!FLASHFS.begin()) {
        Serial.println(F("An Error has occurred while mounting FLASHFS"));
        return;
    }

    Settings::Initialize();
// default values for button
    mySettings->buttonSettings.pin = btnPin;
    mySettings->buttonSettings.type = btnType;
    mySettings->buttonSettings.state = btnState;
    if (!mySettings->readSettings()) {
        mySettings->buttonSettings.pin = 0;
    }

    Serial.printf_P(PSTR("Button pin: %d\n"), mySettings->buttonSettings.pin);

    EffectsManager::Initialize();
    mySettings->readEffects();
    MyMatrix::Initialize();

#if defined(SONOFF)
    pinMode(relayPin, OUTPUT);
    pinMode(miniLedPin, OUTPUT);
#endif

    button = new GButton(mySettings->buttonSettings.pin,
                         mySettings->buttonSettings.type,
                         mySettings->buttonSettings.state);
    button->setTickMode(false);
    button->setStepTimeout(20);

    myMatrix->matrixTest();

    button->tick();
    if (button->state()) {
        Serial.println(F("Setup mode entered. No effects!"));
        myMatrix->setBrightness(80);
        myMatrix->fill(CRGB(0, 20, 0), true);
        setupMode = true;
        myMatrix->clear(true);
        return;
    }

    LampWebServer::Initialize(webServerPort);

    Serial.println(F("AutoConnect started"));
    lampWebServer->onConnected([](bool isConnected) {
        Serial.println(F("AutoConnect finished"));
        if (isConnected) {
            LocalDNS::Initialize();
            if (localDNS->begin()) {
                localDNS->addService(F("http"), F("tcp"), webServerPort);
            } else {
                Serial.println(F("An Error has occurred while initializing mDNS"));
            }
            TimeClient::Initialize();
            MqttClient::Initialize();
        } else if (mySettings->buttonSettings.pin > 0) {
            button->tick();
            if (button->state()) {
                Serial.println(F("Setup mode entered. No effects!"));
                myMatrix->setBrightness(80);
                myMatrix->fill(CRGB(0, 20, 0), true);
                setupMode = true;
                myMatrix->clear(true);
                return;
            }
        }

//    if (mySettings->generalSettings.soundControl) {
//        Spectrometer::Initialize();
//    }
        if (!setupMode) {
            effectsManager->activateEffect(mySettings->generalSettings.activeEffect);
        }
        connectFinished = true;
    });
    lampWebServer->autoConnect();

#if defined(ESP32)
    xTaskCreatePinnedToCore(processMatrixTask, "FastLEDshowTask", 10000, NULL, 2, &processMatrixTaskHandle, 0);
#endif
}

void loop() {
#if defined(ESP8266)
    ESP.wdtFeed();
#endif

    lampWebServer->loop();

    if (!connectFinished) {
        return;
    }

    if (lampWebServer->isUpdating()) {
        return;
    }

    localDNS->loop();
    if (lampWebServer->isConnected()) {
        timeClient->loop();
    } else if (setupMode) {
        return;
    }
    processButton();
#if defined(SONOFF)
    digitalWrite(relayPin, mySettings->generalSettings.working);
    digitalWrite(miniLedPin, mySettings->generalSettings.working);
#endif

//    if (mySettings->generalSettings.soundControl) {
//        mySpectrometer->loop();
//    }

#if defined(ESP32)
    processMatrix32();
#else
    processMatrix();
#endif

    mySettings->loop();

    if (mySettings->generalSettings.logInterval > 0 && millis() - logTimer > mySettings->generalSettings.logInterval) {
        printFreeHeap();
        logTimer = millis();
    }
}
