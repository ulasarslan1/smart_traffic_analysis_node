#pragma once

#include <Arduino.h>
#include "DecisionTreeModel.h"

class FeatureExtractor
{
public:
    void reset(uint32_t startMs);
    void addSample(float distanceCm);
    bool extract(uint32_t endMs, PassageFeatures& features) const;

private:
    static constexpr uint16_t MAX_SAMPLES = 64;

    float samples[MAX_SAMPLES];

    uint16_t sampleCount = 0;
    uint32_t passageStartMs = 0;
};