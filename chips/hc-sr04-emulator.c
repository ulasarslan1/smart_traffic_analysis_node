#include "wokwi-api.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// HC-SR04 configuration

#define SOUND_SPEED_CM_PER_US   0.0343f
#define ECHO_START_DELAY_US     100

#define MIN_DISTANCE_CM         2.0f
#define MAX_DISTANCE_CM         400.0f

#define EMPTY_ROAD_CM           200.0f
#define BASELINE_CM             100.0f
#define PROFILE_START_CM        95.0f


// Traffic timing


#define EMPTY_ROAD_BEFORE_MS    2000U
#define CYCLE_LENGTH_MS         9000U


// Profile variation


#define DEPTH_SCALE_MIN         0.90f
#define DEPTH_SCALE_RANGE       0.20f

#define TIME_SCALE_MIN          0.85f
#define TIME_SCALE_RANGE        0.30f


// Internal constants

#define INVALID_CYCLE           UINT32_MAX
#define VEHICLE_CLASS_COUNT     3U


// Vehicle types

typedef enum {
    VEHICLE_MOTORCYCLE = 0,
    VEHICLE_CAR        = 1,
    VEHICLE_TRUCK      = 2
} vehicle_type_t;


// Vehicle profile structures

typedef struct {
    float p_end;
    float d_end;
} keyframe_t;

typedef struct {
    const keyframe_t *keyframes;
    int count;
    float base_duration_ms;
} vehicle_profile_t;


// Chip state

typedef struct {
    pin_t trig;
    pin_t echo;

    pin_t label0;
    pin_t label1;

    timer_t echo_start_timer;
    timer_t echo_end_timer;

    uint32_t distance_attr;
    uint32_t auto_attr;

    uint32_t echo_duration_us;

    bool previous_auto_mode;
    uint64_t auto_start_us;

    uint32_t current_cycle;
    uint32_t prepared_cycle;

    vehicle_type_t current_vehicle_type;

    float depth_scale;
    float time_scale;
} chip_state_t;


// Vehicle profiles

static const keyframe_t MOTORCYCLE_KEYS[] = {
    { 0.20f, 72.0f },
    { 0.45f, 58.0f },
    { 0.65f, 68.0f },
    { 1.00f, 95.0f },
};

static const keyframe_t CAR_KEYS[] = {
    { 0.20f, 68.0f },
    { 0.45f, 48.0f },
    { 0.70f, 60.0f },
    { 1.00f, 95.0f },
};

static const keyframe_t TRUCK_KEYS[] = {
    { 0.15f, 62.0f },
    { 0.35f, 43.0f },
    { 0.70f, 52.0f },
    { 1.00f, 95.0f },
};

#define KEY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const vehicle_profile_t PROFILES[] = {
    [VEHICLE_MOTORCYCLE] = { MOTORCYCLE_KEYS, KEY_COUNT(MOTORCYCLE_KEYS), 3600.0f },
    [VEHICLE_CAR]        = { CAR_KEYS, KEY_COUNT(CAR_KEYS), 4300.0f },
    [VEHICLE_TRUCK]      = { TRUCK_KEYS, KEY_COUNT(TRUCK_KEYS), 5000.0f }
};


// Utility functions

static float clampf(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;

    return value;
}

static float interpolate(float a, float b, float t)
{
    return a + (b - a) * t;
}

static uint32_t mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;

    return x;
}

static float random_unit(uint32_t seed)
{
    return (float)(mix32(seed) % 10000U) / 10000.0f;
}


static vehicle_type_t select_vehicle_type(uint32_t cycle)
{
    const uint32_t group = cycle / VEHICLE_CLASS_COUNT;
    const uint32_t position = cycle % VEHICLE_CLASS_COUNT;

    vehicle_type_t order[VEHICLE_CLASS_COUNT] = {
        VEHICLE_MOTORCYCLE,
        VEHICLE_CAR,
        VEHICLE_TRUCK
    };

    uint32_t seed = mix32(group * 2654435761U + 0x9E3779B9U);

    for (int i = 2; i > 0; --i) {
        seed = mix32(seed + (uint32_t)i * 0x85EBCA6BU);

        const uint32_t j = seed % (uint32_t)(i + 1);

        const vehicle_type_t temp = order[i];
        order[i] = order[j];
        order[j] = temp;
    }

    return order[position];
}


static void update_ground_truth_pins(chip_state_t *chip, vehicle_type_t vehicle_type)
{
    switch (vehicle_type) {
        case VEHICLE_MOTORCYCLE:
            pin_write(chip->label0, LOW);
            pin_write(chip->label1, LOW);
            break;

        case VEHICLE_CAR:
            pin_write(chip->label0, HIGH);
            pin_write(chip->label1, LOW);
            break;

        case VEHICLE_TRUCK:
            pin_write(chip->label0, LOW);
            pin_write(chip->label1, HIGH);
            break;
    }
}

// Cycle preparation

static void prepare_cycle(chip_state_t *chip, uint32_t cycle)
{
    chip->current_cycle = cycle;
    chip->current_vehicle_type = select_vehicle_type(cycle);

    update_ground_truth_pins(chip, chip->current_vehicle_type);

    const uint32_t seed =
        cycle * 31U
        + (uint32_t)chip->current_vehicle_type * 997U
        + 12345U;

    const float r1 = random_unit(seed);
    const float r2 = random_unit(seed + 77U);

    chip->depth_scale = DEPTH_SCALE_MIN + r1 * DEPTH_SCALE_RANGE;
    chip->time_scale = TIME_SCALE_MIN + r2 * TIME_SCALE_RANGE;
    chip->prepared_cycle = cycle;
}

// Vehicle profile

static float profile_distance(const vehicle_profile_t *profile, float p, float scale)
{
    float prev_p = 0.0f;
    float prev_d = PROFILE_START_CM;
    float distance;

    for (int i = 0; i < profile->count - 1; ++i) {
        const keyframe_t *key = &profile->keyframes[i];

        if (p < key->p_end) {
            distance = interpolate(prev_d, key->d_end, (p - prev_p) / (key->p_end - prev_p));

            return BASELINE_CM - (BASELINE_CM - distance) * scale;
        }

        prev_p = key->p_end;
        prev_d = key->d_end;
    }

    const keyframe_t *last = &profile->keyframes[profile->count - 1];

    distance = interpolate(prev_d, last->d_end, (p - prev_p) / (last->p_end - prev_p));

    return BASELINE_CM - (BASELINE_CM - distance) * scale;
}


// Automatic traffic distance


static float get_auto_distance(chip_state_t *chip)
{
    const uint64_t now_us = get_sim_nanos() / 1000;
    const uint64_t elapsed_ms = (now_us - chip->auto_start_us) / 1000;

    const uint32_t cycle = (uint32_t)(elapsed_ms / CYCLE_LENGTH_MS);
    const uint32_t cycle_time = (uint32_t)(elapsed_ms % CYCLE_LENGTH_MS);

    if (chip->prepared_cycle != cycle) {
        prepare_cycle(chip, cycle);
    }

    const vehicle_profile_t *profile = &PROFILES[chip->current_vehicle_type];

    if (cycle_time < EMPTY_ROAD_BEFORE_MS) {
        return EMPTY_ROAD_CM;
    }

    const float passage_duration_ms = profile->base_duration_ms * chip->time_scale;
    const float passage_time_ms = (float)(cycle_time - EMPTY_ROAD_BEFORE_MS);

    if (passage_time_ms >= passage_duration_ms) {
        return EMPTY_ROAD_CM;
    }

    const float progress = clampf(passage_time_ms / passage_duration_ms, 0.0f, 1.0f);

    return profile_distance(profile, progress, chip->depth_scale);
}


// Echo handling

static void echo_end(void *user_data)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    pin_write(chip->echo, LOW);
}

static void echo_start(void *user_data)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    pin_write(chip->echo, HIGH);

    timer_start(chip->echo_end_timer, chip->echo_duration_us, false);
}


// Auto mode


static void update_auto_mode_state(chip_state_t *chip, bool auto_mode)
{
    if (auto_mode && !chip->previous_auto_mode) {
        chip->auto_start_us = get_sim_nanos() / 1000;
        chip->current_cycle = 0;
        chip->prepared_cycle = INVALID_CYCLE;
    }

    chip->previous_auto_mode = auto_mode;
}


// Distance source


static float read_distance_cm(chip_state_t *chip, bool auto_mode)
{
    float distance_cm;

    if (auto_mode) {
        distance_cm = get_auto_distance(chip);
    }
    else {
        distance_cm = attr_read_float(chip->distance_attr);
    }

    return clampf(distance_cm, MIN_DISTANCE_CM, MAX_DISTANCE_CM);
}


// TRIG handling

static void trig_changed(void *user_data, pin_t pin, uint32_t value)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    if (value != LOW) {
        return;
    }

    const bool auto_mode = attr_read_float(chip->auto_attr) >= 0.5f;

    update_auto_mode_state(chip, auto_mode);

    const float distance_cm = read_distance_cm(chip, auto_mode);

    chip->echo_duration_us = (uint32_t)(distance_cm * 2.0f / SOUND_SPEED_CM_PER_US);

    timer_start(chip->echo_start_timer, ECHO_START_DELAY_US, false);
}


// Chip initialization


void chip_init()
{
    chip_state_t *chip = malloc(sizeof(chip_state_t));

    chip->trig = pin_init("TRIG", INPUT);
    chip->echo = pin_init("ECHO", OUTPUT_LOW);

    chip->label0 = pin_init("LABEL0", OUTPUT_LOW);
    chip->label1 = pin_init("LABEL1", OUTPUT_LOW);

    chip->distance_attr = attr_init_float("distance", 200.0f);
    chip->auto_attr = attr_init_float("auto", 0.0f);

    chip->echo_duration_us = 0;

    chip->previous_auto_mode = false;
    chip->auto_start_us = get_sim_nanos() / 1000;

    chip->current_cycle = 0;
    chip->prepared_cycle = INVALID_CYCLE;

    chip->current_vehicle_type = VEHICLE_CAR;

    chip->depth_scale = 1.0f;
    chip->time_scale = 1.0f;

    update_ground_truth_pins(chip, chip->current_vehicle_type);

    const timer_config_t echo_start_config = {
        .callback = echo_start,
        .user_data = chip
    };

    chip->echo_start_timer = timer_init(&echo_start_config);

    const timer_config_t echo_end_config = {
        .callback = echo_end,
        .user_data = chip
    };

    chip->echo_end_timer = timer_init(&echo_end_config);

    const pin_watch_config_t trig_watch = {
        .edge = FALLING,
        .pin_change = trig_changed,
        .user_data = chip
    };

    pin_watch(chip->trig, &trig_watch);
}