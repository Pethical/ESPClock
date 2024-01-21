#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "font.h"
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

#define MAX_DEVICES 4
#define CS_PIN 21

#define SSID ""
#define PASSWORD ""

hw_timer_t *My_timer = nullptr;

MD_Parola Display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
volatile int hour = -1;
volatile int minute = -1;
volatile int second = -1;
volatile int lastUpdate = 0;

void OTA() {
    ArduinoOTA
            .onStart([]() {
                String type;
                if (ArduinoOTA.getCommand() == U_FLASH)
                    type = "sketch";
                else
                    type = "filesystem";
                Serial.println("Start updating " + type);
            })
            .onEnd([]() {
                Serial.println("\nEnd");
            })
            .onProgress([](unsigned int progress, unsigned int total) {
                Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
            })
            .onError([](ota_error_t error) {
                Serial.printf("Error[%u]: ", error);
                if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
                else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
                else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
                else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
                else if (error == OTA_END_ERROR) Serial.println("End Failed");
            });
    mdns_init();
    mdns_hostname_set("espclock");
    mdns_instance_name_set("espclock");
    ArduinoOTA.setHostname("espclock");
    ArduinoOTA.begin();
}

bool initWiFi() {
    if(WiFi.status() == WL_CONNECTED) return true;
    bool x = false;
    int probe = 0;
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        probe++;
        delay(100);
        Display.print(String(x?" ":"..."));
        x=!x;
        if(probe>100) return false;
    }
    return true;
}

void updateNTP() {
    if(WiFi.status() == WL_CONNECTED) {
        timeClient.update();
        hour = timeClient.getHours() + 1;
        minute = timeClient.getMinutes();
        second = timeClient.getSeconds();
    }
}

void incrementTime();

void IRAM_ATTR onTimer() {
    incrementTime();
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("Disconnected from WiFi access point");
    Serial.print("WiFi lost connection. Reason: ");
    Serial.println(info.wifi_sta_disconnected.reason);
    Serial.println("Trying to Reconnect");
    WiFi.begin(SSID, PASSWORD);
}

void initTimer() {
    My_timer = timerBegin(1, 80, true);
    timerAttachInterrupt(My_timer, &onTimer, true);
    timerAlarmWrite(My_timer, 1000000, true);
    timerAlarmEnable(My_timer);
}

void initDisplay() {
    Display.begin();
    Display.setFont(newFont);
    Display.setIntensity(0);
    Display.displayClear();
    Display.setTextAlignment(PA_LEFT);
}

void setup() {
    Serial.begin(115200);
    initDisplay();
    initWiFi();
    WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    timeClient.begin();
    delay(100);
    OTA();
    initTimer();
}

bool updateDisplay = false;

void incrementTime() {
    second++;
    lastUpdate++;
    if(second > 59) {
        second = 0;
        minute++;
    }
    if(minute > 59) {
        minute = 0;
        hour++;
    }
    if(hour > 23) {
        hour = 0;
    }
    updateDisplay = true;
}

void loop() {
    if((hour < 0) || (lastUpdate > 3600)) {
        updateNTP();
        lastUpdate = 0;
    }
    ArduinoOTA.handle();
    if(updateDisplay) {
        updateDisplay = false;
        Display.print(" " + String(hour / 10) + String(hour % 10) + (second % 2 ? ":" : " ") + String(minute / 10) +String(minute % 10));
        Serial.println(" " + String(hour / 10) + String(hour % 10) + (second % 2 ? ":" : " ") + String(minute / 10) +String(minute % 10));
    }
}