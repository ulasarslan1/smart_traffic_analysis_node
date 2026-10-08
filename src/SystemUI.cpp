#include "SystemUI.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


constexpr uint8_t OLED_SDA_PIN = 8;
constexpr uint8_t OLED_SCL_PIN = 9;

constexpr uint8_t RGB_RED_PIN = 12;
constexpr uint8_t RGB_GREEN_PIN = 13;
constexpr uint8_t RGB_BLUE_PIN = 14;

constexpr uint8_t OLED_ADDRESS = 0x3C;

constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;

bool displayReady = false;

Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire, -1);
    
    
void SystemUI::begin(){

    pinMode(RGB_RED_PIN, OUTPUT);
    pinMode(RGB_GREEN_PIN, OUTPUT);
    pinMode(RGB_BLUE_PIN, OUTPUT);

    setRgb(false, false, false);

    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC,OLED_ADDRESS,false,false)) {
        
        Serial.println("[UI] OLED initialization failed");
        setStatus(SystemStatus::ERROR);
        return;
    }

    displayReady = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    display.setCursor(18, 20);
    display.println("SMART TRAFFIC");

    display.setCursor(36, 35);
    display.println("NODE");

    display.display();

    setStatus(SystemStatus::READY);


    delay(1000);
}

void SystemUI::setRgb(bool red, bool green, bool blue){

    if (red) {
        digitalWrite(RGB_RED_PIN, HIGH);
    } else {
        digitalWrite(RGB_RED_PIN, LOW);
    }

    if (green) {
        digitalWrite(RGB_GREEN_PIN, HIGH);
    } else {
        digitalWrite(RGB_GREEN_PIN, LOW);
    }

    if (blue) {
        digitalWrite(RGB_BLUE_PIN, HIGH);
    } else {
        digitalWrite(RGB_BLUE_PIN, LOW);
    }
}


void SystemUI::setStatus(SystemStatus status){

    switch (status) {

        case SystemStatus::READY:
            // Green
            setRgb(false, true, false);
            break;

        case SystemStatus::DETECTING:
            // Blue
            setRgb(false, false, true);
            break;

        case SystemStatus::CLASSIFIED:
            // Yellow = Red + Green
            setRgb(true, true, false);
            break;

        case SystemStatus::ERROR:
            // Red
            setRgb(true, false, false);
            break;
    }
}


void SystemUI::drawScreen(const char* status, const char* vehicle,
    const VehicleCounters& counters){

    if (!displayReady) return;

    display.clearDisplay();

    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.println("SMART TRAFFIC NODE");

    display.drawLine(0, 10, SCREEN_WIDTH - 1, 10, SSD1306_WHITE);

    display.setCursor(0, 15);
    display.print("Status: ");
    display.println(status);

    display.setCursor(0, 27);
    display.print("Vehicle: ");
    display.println(vehicle);

    display.setCursor(0, 39);
    display.print("Total: ");
    display.println(counters.total);

    display.setCursor(0, 51);
    display.print("C:");
    display.print(counters.car);

    display.print(" M:");
    display.print(counters.motorcycle);

    display.print(" T:");
    display.print(counters.truck);

    display.display();
}




void SystemUI::showReady(const VehicleCounters& counters){

    setStatus(SystemStatus::READY);
    drawScreen("READY", "-", counters);
}

void SystemUI::showDetecting(const VehicleCounters& counters){

    setStatus(SystemStatus::DETECTING);
    drawScreen("DETECTING", "...", counters);
}


void SystemUI::showClassification(VehicleClass vehicleClass,
    const VehicleCounters& counters){

    setStatus(SystemStatus::CLASSIFIED);
    drawScreen("CLASSIFIED",vehicleClassName(vehicleClass),counters);
              
}

void SystemUI::showError(const VehicleCounters& counters){

    setStatus(SystemStatus::ERROR);
    drawScreen("ERROR","UNKNOWN",counters);
          
}