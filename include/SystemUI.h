#pragma once

#include <Arduino.h>
#include "DecisionTreeModel.h"

enum class SystemStatus {
    READY,
    DETECTING,
    CLASSIFIED,
    ERROR
};

struct VehicleCounters {
    uint32_t total = 0;
    uint32_t car = 0;
    uint32_t motorcycle = 0;
    uint32_t truck = 0;
};

class SystemUI
{
public:
    void begin();

    void setStatus(SystemStatus status);

    void showReady(const VehicleCounters& counters);

    void showDetecting(const VehicleCounters& counters);

    void showClassification(VehicleClass vehicleClass,const VehicleCounters& counters);
        
    void showError(const VehicleCounters& counters);

private:
    void setRgb(bool red, bool green, bool blue);

    void drawScreen(const char* status,const char* vehicle,const VehicleCounters& counters);
         
};