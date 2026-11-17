#include "wokwi-api.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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


// Display (side view of the road under the sensor)
// The display only reads the simulation state. It never changes what the
// sensor measures, so ECHO timing is identical with or without it.

#define FRAME_PERIOD_US         50000U
#define FB_MAX_PIXELS           (160U * 120U)

#define SCENE_SENSOR_X          64
#define SCENE_SENSOR_Y          32      // beam origin (bottom of the sensor)
#define SCENE_ROAD_Y            82      // road surface, 200 cm below the sensor
#define SCENE_PX_PER_CM         0.25f
#define SCENE_PX_PER_MS         0.018f  // vehicle length on screen per ms of passage

// Outside the measured passage the vehicle moves faster so it can enter and
// leave the screen inside one 9 s cycle. Only the drawing uses this.
#define SCENE_APPROACH_ACCEL    0.000008f
#define SCENE_LEAVE_ACCEL       0.00003f


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

    buffer_t framebuffer;
    uint32_t fb_width;
    uint32_t fb_height;
    timer_t frame_timer;
} chip_state_t;


// Per-cycle random parameters (pure function of the cycle number)

typedef struct {
    vehicle_type_t type;
    float depth_scale;
    float time_scale;
} cycle_params_t;


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

static cycle_params_t get_cycle_params(uint32_t cycle)
{
    cycle_params_t params;

    params.type = select_vehicle_type(cycle);

    const uint32_t seed =
        cycle * 31U
        + (uint32_t)params.type * 997U
        + 12345U;

    const float r1 = random_unit(seed);
    const float r2 = random_unit(seed + 77U);

    params.depth_scale = DEPTH_SCALE_MIN + r1 * DEPTH_SCALE_RANGE;
    params.time_scale = TIME_SCALE_MIN + r2 * TIME_SCALE_RANGE;

    return params;
}

static void prepare_cycle(chip_state_t *chip, uint32_t cycle)
{
    const cycle_params_t params = get_cycle_params(cycle);

    chip->current_cycle = cycle;
    chip->current_vehicle_type = params.type;

    update_ground_truth_pins(chip, chip->current_vehicle_type);

    chip->depth_scale = params.depth_scale;
    chip->time_scale = params.time_scale;
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


// ============================================================
// Display rendering
// ============================================================

// Freestanding build (plain clang --target=wasm32, no libc): the compiler may
// emit calls to these. wasi-sdk builds (wokwi-cli chip compile) use libc's.
#ifndef __wasi__
void *memset(void *dst, int value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    while (len--) *d++ = (uint8_t)value;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (len--) *d++ = *s++;
    return dst;
}
#endif

#define RGB(r, g, b) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | 0xFF000000U)

#define COLOR_SKY_TOP       RGB(240, 249, 255)
#define COLOR_SKY_BOTTOM    RGB(207, 232, 250)
#define COLOR_ROAD          RGB(100, 116, 139)
#define COLOR_ROAD_EDGE     RGB(71, 85, 105)
#define COLOR_LANE          RGB(248, 250, 252)
#define COLOR_GANTRY        RGB(71, 85, 105)
#define COLOR_PCB           RGB(29, 78, 216)
#define COLOR_EYE           RGB(226, 232, 240)
#define COLOR_EYE_CORE      RGB(51, 65, 85)
#define COLOR_TEXT          RGB(15, 23, 42)
#define COLOR_TEXT_DIM      RGB(71, 85, 105)
#define COLOR_TEXT_DARK     RGB(15, 23, 42)
#define COLOR_BEAM_FREE     RGB(2, 132, 199)
#define COLOR_BEAM_HIT      RGB(217, 119, 6)
#define COLOR_TIRE          RGB(30, 41, 59)
#define COLOR_HUB           RGB(203, 213, 225)
#define COLOR_AUTO          RGB(74, 222, 128)
#define COLOR_MANUAL        RGB(203, 213, 225)
#define COLOR_OBJECT        RGB(148, 163, 184)

static const uint32_t VEHICLE_COLORS[VEHICLE_CLASS_COUNT] = {
    [VEHICLE_MOTORCYCLE] = RGB(251, 191, 36),
    [VEHICLE_CAR]        = RGB(56, 189, 248),
    [VEHICLE_TRUCK]      = RGB(248, 113, 113),
};

static const uint32_t VEHICLE_ROOF_COLORS[VEHICLE_CLASS_COUNT] = {
    [VEHICLE_MOTORCYCLE] = RGB(180, 83, 9),
    [VEHICLE_CAR]        = RGB(3, 105, 161),
    [VEHICLE_TRUCK]      = RGB(185, 28, 28),
};

static const char *const VEHICLE_NAMES[VEHICLE_CLASS_COUNT] = {
    [VEHICLE_MOTORCYCLE] = "MOTO",
    [VEHICLE_CAR]        = "CAR",
    [VEHICLE_TRUCK]      = "TRUCK",
};

static uint32_t pixels[FB_MAX_PIXELS];
static int fb_w;
static int fb_h;


// 5x7 font, one byte per row, bit 4 = leftmost column

typedef struct {
    char ch;
    uint8_t rows[7];
} glyph_t;

static const glyph_t FONT[] = {
    { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } },
    { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } },
    { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } },
    { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
    { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } },
    { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } },
    { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
    { 'A', { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } },
    { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } },
    { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } },
    { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
    { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } },
    { 'N', { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 } },
    { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
    { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
    { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
    { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
    { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
    { 'Y', { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 } },
    { 'c', { 0x00, 0x00, 0x0E, 0x10, 0x10, 0x11, 0x0E } },
    { 'm', { 0x00, 0x00, 0x1A, 0x15, 0x15, 0x11, 0x11 } },
    { '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } },
};

static void put_pixel(int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= fb_w || y >= fb_h) return;
    pixels[y * fb_w + x] = color;
}

static void fill_rect(int x, int y, int w, int h, uint32_t color)
{
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) {
            put_pixel(xx, yy, color);
        }
    }
}

static void fill_circle(int cx, int cy, int r, uint32_t color)
{
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy <= r * r) put_pixel(cx + dx, cy + dy, color);
        }
    }
}

static const uint8_t *find_glyph(char ch)
{
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); ++i) {
        if (FONT[i].ch == ch) return FONT[i].rows;
    }
    return NULL;
}

static int text_width(const char *text, int scale)
{
    int n = 0;
    while (text[n]) ++n;
    return n > 0 ? (n * 6 - 1) * scale : 0;
}

static void draw_text(int x, int y, const char *text, uint32_t color, int scale)
{
    for (; *text; ++text, x += 6 * scale) {
        const uint8_t *rows = find_glyph(*text);
        if (!rows) continue;

        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row] & (0x10 >> col)) {
                    fill_rect(x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
    }
}

static void format_cm(char *out, int value)
{
    char digits[8];
    int n = 0;

    if (value <= 0) digits[n++] = '0';
    while (value > 0 && n < 7) {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    }

    int i = 0;
    while (n > 0) out[i++] = digits[--n];
    out[i++] = 'c';
    out[i++] = 'm';
    out[i] = '\0';
}

static uint32_t blend(uint32_t a, uint32_t b, float t)
{
    const int ar = a & 0xFF, ag = (a >> 8) & 0xFF, ab = (a >> 16) & 0xFF;
    const int br = b & 0xFF, bg = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF;

    return RGB(ar + (int)((br - ar) * t), ag + (int)((bg - ag) * t), ab + (int)((bb - ab) * t));
}

static int distance_to_y(float distance_cm)
{
    const int y = SCENE_SENSOR_Y + (int)(distance_cm * SCENE_PX_PER_CM + 0.5f);
    return y > SCENE_ROAD_Y ? SCENE_ROAD_Y : y;
}

static void draw_background(void)
{
    for (int y = 0; y < SCENE_ROAD_Y; ++y) {
        const uint32_t color = blend(COLOR_SKY_TOP, COLOR_SKY_BOTTOM, (float)y / SCENE_ROAD_Y);
        fill_rect(0, y, fb_w, 1, color);
    }

    fill_rect(0, SCENE_ROAD_Y, fb_w, fb_h - SCENE_ROAD_Y, COLOR_ROAD);
    fill_rect(0, SCENE_ROAD_Y, fb_w, 1, COLOR_ROAD_EDGE);

    for (int x = 2; x < fb_w; x += 14) {
        fill_rect(x, SCENE_ROAD_Y + 7, 7, 1, COLOR_LANE);
    }
}

static void draw_gantry(void)
{
    fill_rect(4, 22, 2, SCENE_ROAD_Y - 22, COLOR_GANTRY);
    fill_rect(4, 22, SCENE_SENSOR_X + 6, 2, COLOR_GANTRY);

    fill_rect(SCENE_SENSOR_X - 10, 24, 21, 8, COLOR_PCB);
    fill_circle(SCENE_SENSOR_X - 5, 28, 3, COLOR_EYE);
    fill_circle(SCENE_SENSOR_X + 5, 28, 3, COLOR_EYE);
    fill_circle(SCENE_SENSOR_X - 5, 28, 1, COLOR_EYE_CORE);
    fill_circle(SCENE_SENSOR_X + 5, 28, 1, COLOR_EYE_CORE);
}


// Vehicle sprites
// Shapes are given in vehicle coordinates: u runs from the front bumper (0) to
// the rear (1), h is height above the road in cm. The outlines follow the
// emulator's distance profiles, and heights above 100 cm are scaled by the
// same depth factor as the profile, so the beam ends on the drawn vehicle.

typedef struct {
    float front_x;
    float length_px;
    float depth_scale;
} vehicle_frame_t;

typedef struct {
    float u;
    float h;
} vpt_t;

#define COLOR_GLASS         RGB(191, 219, 254)
#define COLOR_DARK_PART     RGB(55, 65, 81)
#define COLOR_CHROME        RGB(148, 163, 184)
#define COLOR_HEADLIGHT     RGB(253, 224, 71)
#define COLOR_TAILLIGHT     RGB(220, 38, 38)
#define COLOR_RIM           RGB(203, 213, 225)
#define COLOR_JACKET        RGB(30, 41, 59)
#define COLOR_HELMET        RGB(248, 250, 252)
#define COLOR_CARGO         RGB(241, 245, 249)
#define COLOR_CARGO_EDGE    RGB(148, 163, 184)

static int ifloor(float v)
{
    const int i = (int)v;
    return (v < (float)i) ? i - 1 : i;
}

static float vx(const vehicle_frame_t *vf, float u)
{
    return vf->front_x - u * vf->length_px;
}

static float vy(const vehicle_frame_t *vf, float h)
{
    if (h > 100.0f) h = 100.0f + (h - 100.0f) * vf->depth_scale;
    return (float)SCENE_ROAD_Y - h * SCENE_PX_PER_CM;
}

// Scanline fill of a simple polygon (screen coordinates).
static void fill_poly_xy(const float *xs, const float *ys, int n, uint32_t color)
{
    float min_y = ys[0], max_y = ys[0];
    for (int i = 1; i < n; ++i) {
        if (ys[i] < min_y) min_y = ys[i];
        if (ys[i] > max_y) max_y = ys[i];
    }

    for (int y = ifloor(min_y); y <= ifloor(max_y) + 1; ++y) {
        const float yc = (float)y + 0.5f;
        float cross[16];
        int k = 0;

        for (int i = 0; i < n && k < 16; ++i) {
            const int j = (i + 1) % n;
            const float y0 = ys[i], y1 = ys[j];
            if ((y0 <= yc && y1 > yc) || (y1 <= yc && y0 > yc)) {
                cross[k++] = xs[i] + (yc - y0) * (xs[j] - xs[i]) / (y1 - y0);
            }
        }

        for (int a = 1; a < k; ++a) {
            const float v = cross[a];
            int b = a - 1;
            while (b >= 0 && cross[b] > v) { cross[b + 1] = cross[b]; --b; }
            cross[b + 1] = v;
        }

        for (int a = 0; a + 1 < k; a += 2) {
            for (int x = ifloor(cross[a] + 0.5f); (float)x + 0.5f < cross[a + 1]; ++x) {
                put_pixel(x, y, color);
            }
        }
    }
}

static void vpoly(const vehicle_frame_t *vf, const vpt_t *pts, int n, uint32_t color)
{
    float xs[16], ys[16];
    if (n > 16) n = 16;
    for (int i = 0; i < n; ++i) {
        xs[i] = vx(vf, pts[i].u);
        ys[i] = vy(vf, pts[i].h);
    }
    fill_poly_xy(xs, ys, n, color);
}

#define VPOLY(vf, color, ...) do { \
    const vpt_t pts_[] = { __VA_ARGS__ }; \
    vpoly((vf), pts_, (int)(sizeof(pts_) / sizeof(pts_[0])), (color)); \
} while (0)

static void vline(const vehicle_frame_t *vf, float u0, float h0, float u1, float h1, int radius, uint32_t color)
{
    const float x0 = vx(vf, u0), y0 = vy(vf, h0);
    const float x1 = vx(vf, u1), y1 = vy(vf, h1);
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    const int steps = (int)(len * 2.0f) + 1;

    for (int i = 0; i <= steps; ++i) {
        const float t = (float)i / (float)steps;
        fill_circle(ifloor(x0 + dx * t + 0.5f), ifloor(y0 + dy * t + 0.5f), radius, color);
    }
}

static void vcircle(const vehicle_frame_t *vf, float u, float h, int radius, uint32_t color)
{
    fill_circle(ifloor(vx(vf, u) + 0.5f), ifloor(vy(vf, h) + 0.5f), radius, color);
}

static void vwheel(const vehicle_frame_t *vf, float u, int radius)
{
    const int cx = ifloor(vx(vf, u) + 0.5f);
    const int cy = SCENE_ROAD_Y - radius;

    fill_circle(cx, cy, radius, COLOR_TIRE);
    fill_circle(cx, cy, radius - 2, COLOR_RIM);
    fill_circle(cx, cy, 1, COLOR_DARK_PART);
}

static void draw_motorcycle(const vehicle_frame_t *vf, uint32_t body)
{
    // Exhaust and engine
    vline(vf, 0.55f, 40.0f, 0.92f, 50.0f, 1, COLOR_CHROME);
    VPOLY(vf, COLOR_DARK_PART, { 0.36f, 34.0f }, { 0.62f, 34.0f }, { 0.64f, 62.0f }, { 0.38f, 66.0f });

    // Front fork and handlebar
    vline(vf, 0.13f, 30.0f, 0.24f, 104.0f, 1, COLOR_CHROME);
    vline(vf, 0.24f, 104.0f, 0.30f, 108.0f, 1, COLOR_DARK_PART);

    // Tank, seat and tail
    VPOLY(vf, body, { 0.25f, 80.0f }, { 0.30f, 96.0f }, { 0.44f, 100.0f }, { 0.50f, 92.0f },
                    { 0.74f, 94.0f }, { 0.92f, 90.0f }, { 0.88f, 78.0f }, { 0.60f, 70.0f }, { 0.34f, 68.0f });

    // Windscreen and headlight
    VPOLY(vf, COLOR_GLASS, { 0.19f, 100.0f }, { 0.22f, 126.0f }, { 0.27f, 124.0f }, { 0.27f, 102.0f });
    vcircle(vf, 0.19f, 92.0f, 2, COLOR_HEADLIGHT);

    // Courier top box
    VPOLY(vf, COLOR_DARK_PART, { 0.66f, 94.0f }, { 0.66f, 128.0f }, { 0.88f, 128.0f }, { 0.90f, 94.0f });
    VPOLY(vf, body, { 0.68f, 106.0f }, { 0.68f, 112.0f }, { 0.87f, 112.0f }, { 0.87f, 106.0f });
    vcircle(vf, 0.91f, 86.0f, 1, COLOR_TAILLIGHT);

    // Rider
    vline(vf, 0.58f, 98.0f, 0.42f, 88.0f, 2, COLOR_JACKET);
    vline(vf, 0.42f, 88.0f, 0.46f, 56.0f, 1, COLOR_JACKET);
    vline(vf, 0.58f, 100.0f, 0.46f, 126.0f, 3, COLOR_JACKET);
    vline(vf, 0.46f, 124.0f, 0.28f, 106.0f, 1, COLOR_JACKET);
    vcircle(vf, 0.44f, 136.0f, 4, COLOR_HELMET);
    vline(vf, 0.40f, 136.0f, 0.42f, 136.0f, 1, COLOR_DARK_PART);

    vwheel(vf, 0.13f, 7);
    vwheel(vf, 0.85f, 7);
}

static void draw_car(const vehicle_frame_t *vf, uint32_t body, uint32_t trim)
{
    // Lower body and cabin
    VPOLY(vf, body, { 0.00f, 32.0f }, { 0.00f, 86.0f }, { 0.03f, 100.0f }, { 0.15f, 106.0f },
                    { 0.86f, 110.0f }, { 0.98f, 104.0f }, { 1.00f, 88.0f }, { 1.00f, 32.0f });
    VPOLY(vf, body, { 0.15f, 105.0f }, { 0.33f, 150.0f }, { 0.64f, 150.0f }, { 0.86f, 109.0f });

    // Windows
    VPOLY(vf, COLOR_GLASS, { 0.20f, 110.0f }, { 0.345f, 144.0f }, { 0.475f, 144.0f }, { 0.475f, 110.0f });
    VPOLY(vf, COLOR_GLASS, { 0.505f, 110.0f }, { 0.505f, 144.0f }, { 0.625f, 144.0f }, { 0.79f, 110.0f });

    // Door line, handle, sill
    vline(vf, 0.49f, 42.0f, 0.49f, 108.0f, 0, trim);
    vline(vf, 0.40f, 92.0f, 0.43f, 92.0f, 0, trim);
    vline(vf, 0.04f, 38.0f, 0.97f, 38.0f, 0, trim);

    // Lights
    VPOLY(vf, COLOR_HEADLIGHT, { 0.00f, 76.0f }, { 0.00f, 86.0f }, { 0.04f, 86.0f }, { 0.04f, 76.0f });
    VPOLY(vf, COLOR_TAILLIGHT, { 0.97f, 84.0f }, { 0.97f, 96.0f }, { 1.00f, 94.0f }, { 1.00f, 84.0f });

    vwheel(vf, 0.17f, 7);
    vwheel(vf, 0.80f, 7);
}

static void draw_truck(const vehicle_frame_t *vf, uint32_t body, uint32_t trim)
{
    // Chassis
    VPOLY(vf, COLOR_DARK_PART, { 0.02f, 24.0f }, { 0.99f, 24.0f }, { 0.99f, 40.0f }, { 0.02f, 40.0f });

    // Cargo box with a company stripe
    VPOLY(vf, COLOR_CARGO_EDGE, { 0.24f, 38.0f }, { 0.24f, 155.0f }, { 0.99f, 155.0f }, { 0.99f, 38.0f });
    VPOLY(vf, COLOR_CARGO, { 0.25f, 42.0f }, { 0.25f, 151.0f }, { 0.98f, 151.0f }, { 0.98f, 42.0f });
    VPOLY(vf, body, { 0.25f, 88.0f }, { 0.25f, 100.0f }, { 0.98f, 100.0f }, { 0.98f, 88.0f });

    // Cab
    VPOLY(vf, body, { 0.00f, 30.0f }, { 0.00f, 98.0f }, { 0.02f, 128.0f }, { 0.06f, 142.0f },
                    { 0.22f, 142.0f }, { 0.22f, 30.0f });
    VPOLY(vf, COLOR_GLASS, { 0.03f, 104.0f }, { 0.045f, 127.0f }, { 0.08f, 136.0f }, { 0.17f, 136.0f }, { 0.17f, 104.0f });
    vline(vf, 0.19f, 40.0f, 0.19f, 132.0f, 0, trim);
    VPOLY(vf, COLOR_HEADLIGHT, { 0.00f, 62.0f }, { 0.00f, 74.0f }, { 0.02f, 74.0f }, { 0.02f, 62.0f });
    VPOLY(vf, COLOR_TAILLIGHT, { 0.985f, 44.0f }, { 0.985f, 54.0f }, { 1.00f, 54.0f }, { 1.00f, 44.0f });

    vwheel(vf, 0.11f, 8);
    vwheel(vf, 0.69f, 8);
    vwheel(vf, 0.84f, 8);
}

static void draw_vehicle(vehicle_type_t type, const cycle_params_t *params, float front_x, float length_px)
{
    const vehicle_frame_t vf = { front_x, length_px, params->depth_scale };
    const uint32_t body = VEHICLE_COLORS[type];
    const uint32_t trim = VEHICLE_ROOF_COLORS[type];

    switch (type) {
        case VEHICLE_MOTORCYCLE: draw_motorcycle(&vf, body); break;
        case VEHICLE_CAR:        draw_car(&vf, body, trim); break;
        case VEHICLE_TRUCK:      draw_truck(&vf, body, trim); break;
    }

    // Ground-truth class tag in the top bar
    const char *name = VEHICLE_NAMES[type];
    const int tag_w = text_width(name, 1);
    const int tag_x = SCENE_SENSOR_X + 6 - tag_w / 2;
    fill_rect(tag_x - 3, 1, tag_w + 6, 9, body);
    draw_text(tag_x, 2, name, COLOR_TEXT_DARK, 1);
}

static void draw_beam(float distance_cm)
{
    const int end_y = distance_to_y(distance_cm);
    const bool hit = distance_cm < EMPTY_ROAD_CM - 1.0f;
    const uint32_t color = hit ? COLOR_BEAM_HIT : COLOR_BEAM_FREE;

    for (int y = SCENE_SENSOR_Y; y < end_y; ++y) {
        if (((y - SCENE_SENSOR_Y) / 2) % 2 == 0) put_pixel(SCENE_SENSOR_X, y, color);
    }

    fill_rect(SCENE_SENSOR_X - 3, end_y - 1, 7, 1, color);
}

static void render_frame(void *user_data)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    const bool auto_mode = attr_read_float(chip->auto_attr) >= 0.5f;
    float distance_cm = EMPTY_ROAD_CM;

    draw_background();

    if (auto_mode && chip->previous_auto_mode) {
        const uint64_t now_us = get_sim_nanos() / 1000;
        const uint64_t elapsed_ms = (now_us - chip->auto_start_us) / 1000;
        const uint32_t cycle = (uint32_t)(elapsed_ms / CYCLE_LENGTH_MS);
        const float cycle_time = (float)(elapsed_ms % CYCLE_LENGTH_MS);

        const cycle_params_t params = get_cycle_params(cycle);
        const vehicle_profile_t *profile = &PROFILES[params.type];

        const float duration_ms = profile->base_duration_ms * params.time_scale;
        const float length_px = duration_ms * SCENE_PX_PER_MS;
        const float t = cycle_time - (float)EMPTY_ROAD_BEFORE_MS;

        float front_x;

        if (t < 0.0f) {
            front_x = SCENE_SENSOR_X - (-t * SCENE_PX_PER_MS + SCENE_APPROACH_ACCEL * t * t);
        }
        else if (t <= duration_ms) {
            front_x = SCENE_SENSOR_X + t * SCENE_PX_PER_MS;
            distance_cm = profile_distance(profile, t / duration_ms, params.depth_scale);
        }
        else {
            const float tau = t - duration_ms;
            front_x = SCENE_SENSOR_X + length_px + tau * SCENE_PX_PER_MS + SCENE_LEAVE_ACCEL * tau * tau;
        }

        if (t >= duration_ms) distance_cm = EMPTY_ROAD_CM;

        draw_vehicle(params.type, &params, front_x, length_px);
    }
    else if (!auto_mode) {
        distance_cm = clampf(attr_read_float(chip->distance_attr), MIN_DISTANCE_CM, MAX_DISTANCE_CM);

        if (distance_cm < EMPTY_ROAD_CM) {
            const int top = distance_to_y(distance_cm);
            fill_rect(SCENE_SENSOR_X - 18, top, 37, SCENE_ROAD_Y - top, COLOR_OBJECT);
        }
    }

    draw_gantry();
    draw_beam(distance_cm);

    char text[12];
    format_cm(text, (int)(distance_cm + 0.5f));
    draw_text(SCENE_SENSOR_X + 14, 25, text, COLOR_TEXT, 1);

    draw_text(4, 2, "HC-SR04", COLOR_TEXT_DIM, 1);

    const char *mode = auto_mode ? "AUTO" : "MANUAL";
    const int mode_w = text_width(mode, 1);
    const int mode_x = fb_w - mode_w - 5;
    fill_rect(mode_x - 2, 1, mode_w + 4, 9, auto_mode ? COLOR_AUTO : COLOR_MANUAL);
    draw_text(mode_x, 2, mode, COLOR_TEXT_DARK, 1);

    buffer_write(chip->framebuffer, 0, pixels, (uint32_t)(fb_w * fb_h) * 4U);
}


// Chip initialization

static chip_state_t chip_state;

void chip_init()
{
    chip_state_t *chip = &chip_state;

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

    chip->framebuffer = framebuffer_init(&chip->fb_width, &chip->fb_height);
    fb_w = (int)chip->fb_width;
    fb_h = (int)chip->fb_height;

    if ((uint32_t)(fb_w * fb_h) > FB_MAX_PIXELS) {
        fb_h = (int)(FB_MAX_PIXELS / chip->fb_width);
    }

    const timer_config_t frame_config = {
        .callback = render_frame,
        .user_data = chip
    };

    chip->frame_timer = timer_init(&frame_config);
    render_frame(chip);
    timer_start(chip->frame_timer, FRAME_PERIOD_US, true);
}