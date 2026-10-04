#include "FeatureExtractor.h"

#include <math.h>


void FeatureExtractor::reset(uint32_t startMs)
{
    passageStartMs = startMs;
    sampleCount = 0;
}


void FeatureExtractor::addSample(float distanceCm)
{
    if (sampleCount >= MAX_SAMPLES) {
        return;
    }

    samples[sampleCount] = distanceCm;
    ++sampleCount;
}


bool FeatureExtractor::extract(
    uint32_t endMs,
    PassageFeatures& features
) const
{
    if (sampleCount < 3) {
        return false;
    }

    float minCm = samples[0];
    float maxCm = samples[0];
    float sum = 0.0f;
    float deltaSum = 0.0f;

    for (uint16_t i = 0; i < sampleCount; ++i) {
        const float value = samples[i];

        sum += value;

        if (value < minCm) {
            minCm = value;
        }

        if (value > maxCm) {
            maxCm = value;
        }

        if (i > 0) {
            deltaSum += fabsf(samples[i] - samples[i - 1]);
        }
    }

    const float meanCm = sum / static_cast<float>(sampleCount);

    float squaredDifferenceSum = 0.0f;

    for (uint16_t i = 0; i < sampleCount; ++i) {
        const float difference = samples[i] - meanCm;
        squaredDifferenceSum += difference * difference;
    }

    const float stdCm = sqrtf(
        squaredDifferenceSum / static_cast<float>(sampleCount)
    );

    const float meanDeltaCm =
        deltaSum / static_cast<float>(sampleCount - 1);

    features.duration_ms =
        static_cast<float>(endMs - passageStartMs);

    features.min_cm = minCm;
    features.max_cm = maxCm;
    features.mean_cm = meanCm;
    features.std_cm = stdCm;
    features.range_cm = maxCm - minCm;
    features.mean_delta_cm = meanDeltaCm;
    features.valid_samples =
        static_cast<float>(sampleCount);

    return true;
}