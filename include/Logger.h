#pragma once

#include <Arduino.h>
#include "DecisionTreeModel.h"

namespace Logger {

void begin();

void system(const char* message);

void vehicle(uint32_t id,VehicleClass actual,VehicleClass predicted);
    
void error(const char* message);

}