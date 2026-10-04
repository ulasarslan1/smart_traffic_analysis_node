#pragma once

// ============================================================
// AUTO-GENERATED FILE
//
// Generated from:
// models/decision_tree.joblib
//
// Do not edit the decision rules manually.
// Regenerate this file using:
// python scripts/export_model.py
// ============================================================

enum class VehicleClass {
    CAR,
    MOTORCYCLE,
    TRUCK
};

struct PassageFeatures {
    float duration_ms;
    float min_cm;
    float max_cm;
    float mean_cm;
    float std_cm;
    float range_cm;
    float mean_delta_cm;
    float valid_samples;
};

inline VehicleClass predictVehicle(const PassageFeatures& features)
{
    if (features.min_cm <= 55.2749996185f) {
        if (features.mean_cm <= 64.2507133484f) {
            if (features.std_cm <= 13.8170175552f) {
                return VehicleClass::CAR;
            } else {
                return VehicleClass::TRUCK;
            }
        } else {
            if (features.mean_cm <= 71.8567619324f) {
                if (features.mean_cm <= 64.3826675415f) {
                    if (features.range_cm <= 43.3450012207f) {
                        return VehicleClass::CAR;
                    } else {
                        return VehicleClass::TRUCK;
                    }
                } else {
                    return VehicleClass::CAR;
                }
            } else {
                return VehicleClass::MOTORCYCLE;
            }
        }
    } else {
        return VehicleClass::MOTORCYCLE;
    }
}

inline const char* vehicleClassName(VehicleClass vehicleClass)
{
    switch (vehicleClass) {
        case VehicleClass::CAR:
            return "CAR";

        case VehicleClass::MOTORCYCLE:
            return "MOTORCYCLE";

        case VehicleClass::TRUCK:
            return "TRUCK";

        default:
            return "UNKNOWN";
    }
}
