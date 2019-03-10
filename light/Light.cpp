/*
 * Copyright (C) 2018 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LightService"

#include <log/log.h>

#include "Light.h"

#include <fstream>

#define LCD_LED         "/sys/class/leds/lcd-backlight/brightness"

#define BREATH_SOURCE_NOTIFICATION	0x01
#define BREATH_SOURCE_BATTERY		0x02
#define BREATH_SOURCE_BUTTONS		0x04
#define BREATH_SOURCE_ATTENTION		0x08
#define BREATH_SOURCE_NONE			0x00

#define MAX_LED_BRIGHTNESS    255
#define MAX_LCD_BRIGHTNESS    255

namespace {
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::light::V2_0::Flash;
using ::android::hardware::light::V2_0::ILight;
using ::android::hardware::light::V2_0::LightState;
using ::android::hardware::light::V2_0::Status;
using ::android::hardware::light::V2_0::Type;

/*
 * Write value to path and close file.
 */
static void set(std::string path, std::string value) {
    std::ofstream file(path);

    if (!file.is_open()) {
        ALOGW("failed to write %s to %s", value.c_str(), path.c_str());
        return;
    }

    file << value;
}

static int readStr(std::string path, char *buffer, size_t size)
{

    std::ifstream file(path);

    if (!file.is_open()) {
        ALOGW("failed to read %s", path.c_str());
        return -1;
    }

    file.read(buffer, size);
    file.close();
    return 1;
}

static int set(std::string path, char *buffer, size_t size)
{

    std::ofstream file(path);

    if (!file.is_open()) {
        ALOGW("failed to write %s", path.c_str());
        return -1;
    }

    file.write(buffer, size);
    file.close();
    return 1;
}

static void set(std::string path, int value) {
    set(path, std::to_string(value));
}

static uint32_t getBrightness(const LightState& state) {
    uint32_t alpha, red, green, blue;

    /*
     * Extract brightness from AARRGGBB.
     */
    alpha = (state.color >> 24) & 0xFF;
    red = (state.color >> 16) & 0xFF;
    green = (state.color >> 8) & 0xFF;
    blue = state.color & 0xFF;

    /*
     * Scale RGB brightness if Alpha brightness is not 0xFF.
     */
    if (alpha != 0xFF) {
        red = red * alpha / 0xFF;
        green = green * alpha / 0xFF;
        blue = blue * alpha / 0xFF;
    }

    return (77 * red + 150 * green + 29 * blue) >> 8;
}

static void handleBacklight(const LightState& state) {
    uint32_t brightness = getBrightness(state);
    set(LCD_LED, brightness);
}

static inline bool isLit(const LightState& state) {
    return state.color & 0x00ffffff;
}

int getBatteryStatus(const LightState& state)
{
    int err;

    char status_str[16];
    int capacity;

    err = readStr(BATTERY_STATUS_FILE, status_str, sizeof(status_str));
    if (err <= 0) {
        ALOGI("failed to read battery status: %d", err);
        return BATTERY_UNKNOWN;
    }

    ALOGI("battery status: %d, %s", err, status_str);

    capacity = (state.color >> 24) & 0xff;
    if (capacity > 100) {
        capacity = 100;
    }

    ALOGI("battery capacity: %d", capacity);

    if (0 == strncmp(status_str, BATTERY_STATUS_CHARGING, 8)) {
        if (capacity < 90) {
            return BATTERY_CHARGING;
        } else {
            return BATTERY_FULL;
        }
    } else {
        if (capacity < 10) {
            return BATTERY_LOW;
        } else {
            return BATTERY_FREE;
        }
    }
}

static void handleNubiaLed(const LightState& state, int source)
{

    int buttons;
    int mode;

    char grade[16];
    size_t grade_len = 0;

    char fade[16];
    size_t fade_len = 0;

    ALOGI("setting led %d: %08x, %d, %d", source,
        state.color, state.flashOnMs, state.flashOffMs);

    if (getBrightness(state) > 0) {
        g_ongoing |= source;
        ALOGI("ongoing +: %d = %d", source, g_ongoing);
    } else {
        g_ongoing &= ~source;
        ALOGI("ongoing -: %d = %d", source, g_ongoing);
    }

    buttons = getBrightness(state) / 20;

    /* set side buttons */

    set(NUBIA_CHANNEL_FILE, SILDE_CHANNEL);
    if (g_ongoing & ONGOING_BUTTONS) {
        set(NUBIA_GRADE_FILE, buttons);
        set(NUBIA_MODE_FILE, RGB_LED_MODE_CONSTANT_ON);
    } else {
        set(NUBIA_MODE_FILE, RGB_LED_MODE_OFF);
    }

    /* set middle ring */

    if (g_ongoing & ONGOING_NOTIFICATION) {
        mode = RGB_LED_MODE_AUTO_BLINK;
        grade_len = sprintf(grade, "%d %d\n",
            buttons,
            buttons + getBrightness(state) / 20);
    }
    else if (g_ongoing & ONGOING_ATTENTION) {
        mode = RGB_LED_MODE_AUTO_BLINK;
        grade_len = sprintf(grade, "%d %d\n",
            buttons,
            buttons + getBrightness(state) / 20);
    }
    else if (g_battery == BATTERY_CHARGING) {
        mode = RGB_LED_MODE_AUTO_BLINK;
        grade_len = sprintf(grade, "%d %d\n",
            buttons + BRIGHTNESS_BATTERY_FULL / 20,
            buttons + BRIGHTNESS_BATTERY_CHARGING / 20);
    }
    else if (g_battery == BATTERY_FULL) {
        mode = RGB_LED_MODE_CONSTANT_ON;
        grade_len = sprintf(grade, "%d\n",
            buttons + BRIGHTNESS_BATTERY_FULL / 20);
    }
    else if (g_battery == BATTERY_LOW) {
        mode = RGB_LED_MODE_AUTO_BLINK;
        grade_len = sprintf(grade, "%d %d\n",
            buttons, buttons + BRIGHTNESS_BATTERY_LOW / 20);
    }
    else if (g_ongoing & ONGOING_BUTTONS) {
        mode = RGB_LED_MODE_CONSTANT_ON;
        grade_len = sprintf(grade, "%d\n", buttons);
    }
    else {
        mode = RGB_LED_MODE_OFF;
    }

    if (state.flashMode == Flash::TIMED) {
        fade_len = sprintf(fade, "%d %d %d\n",
            1, state.flashOnMs / 400, state.flashOffMs / 400);
    }

    set(NUBIA_CHANNEL_FILE, MIDDLE_CHANNEL);

    if (grade_len > 0) {
        set(NUBIA_GRADE_FILE, grade, grade_len);
    }

    if (fade_len > 0) {
        set(NUBIA_FADE_FILE, fade, fade_len);
    }

    set(NUBIA_MODE_FILE, mode);

}

static void handleButtons(const LightState& state) {
	handleNubiaLed(state, ONGOING_BUTTONS);
}

static void handleNotification(const LightState& state) {
	handleNubiaLed(state, ONGOING_NOTIFICATION);
}

static void handleBattery(const LightState& state){
	g_battery = getBatteryStatus(state);
	handleNubiaLed(state, 0);
}

static void handleAttention(const LightState& state){
	handleNubiaLed(state, ONGOING_ATTENTION);
}

/* Keep sorted in the order of importance. */
static std::vector<LightBackend> backends = {
    { Type::ATTENTION, handleAttention },
    { Type::NOTIFICATIONS, handleNotification },
    { Type::BATTERY, handleBattery },
    { Type::BACKLIGHT, handleBacklight },
    { Type::BUTTONS, handleButtons },
};

}  // anonymous namespace

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

Return<Status> Light::setLight(Type type, const LightState& state) {
    LightStateHandler handler;
    bool handled = false;

    /* Lock global mutex until light state is updated. */
    std::lock_guard<std::mutex> lock(globalLock);

    /* Update the cached state value for the current type. */
    for (LightBackend& backend : backends) {
        if (backend.type == type) {
            backend.state = state;
            handler = backend.handler;
        }
    }

    /* If no handler has been found, then the type is not supported. */
    if (!handler) {
        return Status::LIGHT_NOT_SUPPORTED;
    }

    /* Light up the type with the highest priority that matches the current handler. */
    for (LightBackend& backend : backends) {
        if (handler == backend.handler && isLit(backend.state)) {
            handler(backend.state);
            handled = true;
            break;
        }
    }

    /* If no type has been lit up, then turn off the hardware. */
    if (!handled) {
        handler(state);
    }

    return Status::SUCCESS;
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb) {
    std::vector<Type> types;

    for (const LightBackend& backend : backends) {
        types.push_back(backend.type);
    }

    _hidl_cb(types);

    return Void();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android
