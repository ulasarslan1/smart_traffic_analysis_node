#include "Logger.h"

namespace Logger {

void begin(){Serial.begin(115200);}
    
void system(const char* message){Serial.printf("[SYSTEM] %s\r\n",message);}

void vehicle(uint32_t id,VehicleClass actual,VehicleClass predicted){

    const bool correct = actual == predicted;
        
    Serial.printf(
        "[VEHICLE] #%lu | actual=%s | predicted=%s | %s\r\n",
        static_cast<unsigned long>(id),
        vehicleClassName(actual),
        vehicleClassName(predicted),
        correct ? "CORRECT" : "WRONG"
    );
}

void error(const char* message){Serial.printf("[ERROR] %s\r\n",message);}


}