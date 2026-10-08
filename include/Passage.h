#pragma once

#include <Arduino.h>
#include "FeatureExtractor.h"

class Passage
{
public:
    void start(uint32_t id, uint32_t startMs, float firstDistance);
    void addSample(float distance);
    void cancel();
    bool complete(uint32_t endMs, PassageFeatures& features);
    bool isActive() const;
    uint32_t getId() const;
    uint32_t getStartMs() const;
    uint32_t getDuration(uint32_t now) const;

private:
    bool active = false;
    uint32_t id = 0;
    uint32_t startMs = 0;
    FeatureExtractor featureExtractor;
};