#include "Passage.h"

void Passage::start(uint32_t id, uint32_t startMs, float firstDistance)
{
    this->id = id;
    this->startMs = startMs;
    active = true;

    featureExtractor.reset(startMs);
    featureExtractor.addSample(firstDistance);
}

void Passage::addSample(float distance)
{
    if (!active) {
        return;
    }

    featureExtractor.addSample(distance);
}

void Passage::cancel()
{
    active = false;
    id = 0;
    startMs = 0;
}

bool Passage::complete(uint32_t endMs, PassageFeatures& features)
{
    if (!active) {
        return false;
    }

    const bool success = featureExtractor.extract(endMs, features);

    active = false;

    return success;
}

bool Passage::isActive() const
{
    return active;
}

uint32_t Passage::getId() const
{
    return id;
}

uint32_t Passage::getStartMs() const
{
    return startMs;
}

uint32_t Passage::getDuration(uint32_t now) const
{
    return now - startMs;
}