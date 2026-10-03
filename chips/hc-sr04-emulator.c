#include "wokwi-api.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


typedef struct {
    pin_t trig;
    pin_t echo;

    timer_t echo_start_timer;
    timer_t echo_end_timer;

    uint32_t distance_attr;
    uint32_t auto_attr;
    uint32_t vehicle_type_attr;

    uint32_t echo_duration_us;

    bool previous_auto_mode;

    uint64_t auto_start_us;

    uint32_t current_cycle;
    uint32_t prepared_cycle;

    float depth_scale;
    float time_scale;

} chip_state_t;


/* ---------------------------------------------------------
   Small deterministic pseudo-random generator

   We intentionally keep the simulation reproducible.
   --------------------------------------------------------- */

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
    return
        (float)(mix32(seed) % 10000U)
        / 10000.0f;
}


/* ---------------------------------------------------------
   Prepare variation for one vehicle passage
   --------------------------------------------------------- */

static void prepare_cycle(
    chip_state_t *chip,
    uint32_t cycle,
    int vehicle_type
)
{
    const uint32_t seed =
        cycle * 31U
        + (uint32_t)vehicle_type * 997U
        + 12345U;

    const float r1 =
        random_unit(seed);

    const float r2 =
        random_unit(seed + 77U);

    /*
       depth_scale:
       0.90 ... 1.10

       time_scale:
       0.85 ... 1.15
    */

    chip->depth_scale =
        0.90f + r1 * 0.20f;

    chip->time_scale =
        0.85f + r2 * 0.30f;

    chip->prepared_cycle = cycle;
}


/* ---------------------------------------------------------
   Linear interpolation
   --------------------------------------------------------- */

static float interpolate(
    float a,
    float b,
    float t
)
{
    return a + (b - a) * t;
}


/* ---------------------------------------------------------
   Vehicle profiles

   p = normalized passage progress 0 ... 1

   These profiles intentionally overlap.
   Vehicle type is not represented by one single feature.
   --------------------------------------------------------- */

static float motorcycle_profile(
    float p,
    float scale
)
{
    float distance;

    if (p < 0.20f) {

        distance =
            interpolate(
                95.0f,
                72.0f,
                p / 0.20f
            );

    } else if (p < 0.45f) {

        distance =
            interpolate(
                72.0f,
                58.0f,
                (p - 0.20f) / 0.25f
            );

    } else if (p < 0.65f) {

        distance =
            interpolate(
                58.0f,
                68.0f,
                (p - 0.45f) / 0.20f
            );

    } else {

        distance =
            interpolate(
                68.0f,
                95.0f,
                (p - 0.65f) / 0.35f
            );
    }

    return 100.0f -
        (100.0f - distance) * scale;
}


static float car_profile(
    float p,
    float scale
)
{
    float distance;

    if (p < 0.20f) {

        distance =
            interpolate(
                95.0f,
                68.0f,
                p / 0.20f
            );

    } else if (p < 0.45f) {

        distance =
            interpolate(
                68.0f,
                48.0f,
                (p - 0.20f) / 0.25f
            );

    } else if (p < 0.70f) {

        distance =
            interpolate(
                48.0f,
                60.0f,
                (p - 0.45f) / 0.25f
            );

    } else {

        distance =
            interpolate(
                60.0f,
                95.0f,
                (p - 0.70f) / 0.30f
            );
    }

    return 100.0f -
        (100.0f - distance) * scale;
}


static float truck_profile(
    float p,
    float scale
)
{
    float distance;

    if (p < 0.15f) {

        distance =
            interpolate(
                95.0f,
                62.0f,
                p / 0.15f
            );

    } else if (p < 0.35f) {

        distance =
            interpolate(
                62.0f,
                43.0f,
                (p - 0.15f) / 0.20f
            );

    } else if (p < 0.70f) {

        distance =
            interpolate(
                43.0f,
                52.0f,
                (p - 0.35f) / 0.35f
            );

    } else {

        distance =
            interpolate(
                52.0f,
                95.0f,
                (p - 0.70f) / 0.30f
            );
    }

    return 100.0f -
        (100.0f - distance) * scale;
}


/* ---------------------------------------------------------
   Automatic simulation
   --------------------------------------------------------- */

static float get_auto_distance(
    chip_state_t *chip,
    int vehicle_type
)
{
    const uint64_t now_us =
        get_sim_nanos() / 1000;

    const uint64_t elapsed_ms =
        (now_us - chip->auto_start_us) / 1000;

    /*
       Each cycle consists of:

       2000 ms empty road
       variable vehicle passage
       2000 ms empty road

       Passage durations intentionally overlap.
    */

    float base_duration_ms;

    switch (vehicle_type) {

        case 0:
            base_duration_ms = 3600.0f;
            break;

        case 2:
            base_duration_ms = 5000.0f;
            break;

        case 1:
        default:
            base_duration_ms = 4300.0f;
            break;
    }


    /*
       Use an approximate maximum cycle length.
       The actual vehicle profile is scaled inside it.
    */

    const uint32_t cycle_length_ms = 9000;

    const uint32_t cycle =
        (uint32_t)(
            elapsed_ms / cycle_length_ms
        );

    const uint32_t cycle_time =
        (uint32_t)(
            elapsed_ms % cycle_length_ms
        );


    if (chip->prepared_cycle != cycle) {

        prepare_cycle(
            chip,
            cycle,
            vehicle_type
        );
    }


    /*
       Empty road before vehicle
    */

    if (cycle_time < 2000) {
        return 200.0f;
    }


    const float passage_duration =
        base_duration_ms *
        chip->time_scale;


    const float passage_time =
        (float)(cycle_time - 2000);


    /*
       Vehicle has left
    */

    if (passage_time >= passage_duration) {
        return 200.0f;
    }


    float progress =
        passage_time /
        passage_duration;


    if (progress < 0.0f) {
        progress = 0.0f;
    }

    if (progress > 1.0f) {
        progress = 1.0f;
    }


    switch (vehicle_type) {

        case 0:
            return motorcycle_profile(
                progress,
                chip->depth_scale
            );

        case 2:
            return truck_profile(
                progress,
                chip->depth_scale
            );

        case 1:
        default:
            return car_profile(
                progress,
                chip->depth_scale
            );
    }
}


/* ---------------------------------------------------------
   ECHO generation
   --------------------------------------------------------- */

static void echo_end(void *user_data)
{
    chip_state_t *chip =
        (chip_state_t *)user_data;

    pin_write(
        chip->echo,
        LOW
    );
}


static void echo_start(void *user_data)
{
    chip_state_t *chip =
        (chip_state_t *)user_data;

    pin_write(
        chip->echo,
        HIGH
    );

    timer_start(
        chip->echo_end_timer,
        chip->echo_duration_us,
        false
    );
}


/* ---------------------------------------------------------
   TRIG handler
   --------------------------------------------------------- */

static void trig_changed(
    void *user_data,
    pin_t pin,
    uint32_t value
)
{
    chip_state_t *chip =
        (chip_state_t *)user_data;


    if (value != LOW) {
        return;
    }


    const bool auto_mode =
        attr_read_float(
            chip->auto_attr
        ) >= 0.5f;


    /*
       Detect Auto Mode rising edge.

       This fixes the previous problem where the profile
       started at simulator startup instead of when Auto
       Mode was enabled.
    */

    if (
        auto_mode &&
        !chip->previous_auto_mode
    ) {

        chip->auto_start_us =
            get_sim_nanos() / 1000;

        chip->current_cycle = 0;

        chip->prepared_cycle =
            UINT32_MAX;
    }


    chip->previous_auto_mode =
        auto_mode;


    float distance_cm;


    if (auto_mode) {

        int vehicle_type =
            (int)(
                attr_read_float(
                    chip->vehicle_type_attr
                ) + 0.5f
            );


        if (vehicle_type < 0) {
            vehicle_type = 0;
        }

        if (vehicle_type > 2) {
            vehicle_type = 2;
        }


        distance_cm =
            get_auto_distance(
                chip,
                vehicle_type
            );

    } else {

        distance_cm =
            attr_read_float(
                chip->distance_attr
            );
    }


    if (distance_cm < 2.0f) {
        distance_cm = 2.0f;
    }

    if (distance_cm > 400.0f) {
        distance_cm = 400.0f;
    }


    chip->echo_duration_us =
        (uint32_t)(
            distance_cm *
            2.0f /
            0.0343f
        );


    timer_start(
        chip->echo_start_timer,
        100,
        false
    );
}


/* ---------------------------------------------------------
   Initialization
   --------------------------------------------------------- */

void chip_init()
{
    chip_state_t *chip =
        malloc(
            sizeof(chip_state_t)
        );


    chip->trig =
        pin_init(
            "TRIG",
            INPUT
        );


    chip->echo =
        pin_init(
            "ECHO",
            OUTPUT_LOW
        );


    chip->distance_attr =
        attr_init_float(
            "distance",
            200.0f
        );


    chip->auto_attr =
        attr_init_float(
            "auto",
            0.0f
        );


    chip->vehicle_type_attr =
    attr_init_float(
        "vehicleType",
        1.0f
    );


    chip->echo_duration_us = 0;

    chip->previous_auto_mode = false;

    chip->auto_start_us =
        get_sim_nanos() / 1000;

    chip->current_cycle = 0;

    chip->prepared_cycle =
        UINT32_MAX;

    chip->depth_scale = 1.0f;

    chip->time_scale = 1.0f;


    const timer_config_t
        echo_start_config = {

        .callback = echo_start,
        .user_data = chip
    };


    chip->echo_start_timer =
        timer_init(
            &echo_start_config
        );


    const timer_config_t
        echo_end_config = {

        .callback = echo_end,
        .user_data = chip
    };


    chip->echo_end_timer =
        timer_init(
            &echo_end_config
        );


    const pin_watch_config_t
        trig_watch = {

        .edge = FALLING,
        .pin_change = trig_changed,
        .user_data = chip
    };


    pin_watch(
        chip->trig,
        &trig_watch
    );
}