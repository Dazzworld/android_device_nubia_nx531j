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

#ifndef ANDROID_HARDWARE_LIGHT_V2_0_LIGHT_H
#define ANDROID_HARDWARE_LIGHT_V2_0_LIGHT_H

#include <android/hardware/light/2.0/ILight.h>
#include <hardware/lights.h>
#include <hidl/Status.h>
#include <map>
#include <mutex>
#include <vector>

/**
 * led defs
 */

#define LCD_FILE \
        "/sys/class/leds/lcd-backlight/brightness"

#define NUBIA_LED_FILE \
        "/sys/class/leds/nubia_led/brightness"

#define NUBIA_CHANNEL_FILE \
        "/sys/class/leds/nubia_led/outn"

#define NUBIA_GRADE_FILE \
        "/sys/class/leds/nubia_led/grade_parameter"

#define NUBIA_FADE_FILE \
        "/sys/class/leds/nubia_led/fade_parameter"

#define NUBIA_MODE_FILE \
        "/sys/class/leds/nubia_led/blink_mode"

#define ONGOING_NONE             0
#define ONGOING_NOTIFICATION    (1 << 0)
#define ONGOING_BUTTONS         (1 << 1)
#define ONGOING_ATTENTION       (1 << 2)

enum led_control_mode {
    RGB_LED_MODE_CLOSED = 0,
    RGB_LED_MODE_CONSTANT_ON,
    RGB_LED_MODE_OFF,
    RGB_LED_MODE_AUTO_BLINK,
    RGB_LED_MODE_POWER_ON,
    RGB_LED_MODE_POWER_OFF,
    RGB_LED_MODE_ONCE_BLINK,
};

#define MIDDLE_CHANNEL   0x10
#define SILDE_CHANNEL    0x08


/**
 * battery defs
 */

enum battery_status {
    BATTERY_UNKNOWN = 0,
    BATTERY_LOW,
    BATTERY_FREE,
    BATTERY_CHARGING,
    BATTERY_FULL,
};

#define BRIGHTNESS_BATTERY_LOW          100
#define BRIGHTNESS_BATTERY_CHARGING     200
#define BRIGHTNESS_BATTERY_FULL         100

#define BATTERY_STATUS_FILE \
        "/sys/class/power_supply/battery/status"

#define BATTERY_STATUS_DISCHARGING  "Discharging"
#define BATTERY_STATUS_NOT_CHARGING "Not charging"
#define BATTERY_STATUS_CHARGING     "Charging"


static int g_ongoing = ONGOING_NONE;
static int g_battery = BATTERY_UNKNOWN;

using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::light::V2_0::Flash;
using ::android::hardware::light::V2_0::ILight;
using ::android::hardware::light::V2_0::LightState;
using ::android::hardware::light::V2_0::Status;
using ::android::hardware::light::V2_0::Type;

typedef void (*LightStateHandler)(const LightState&);

struct LightBackend {
    Type type;
    LightState state;
    LightStateHandler handler;

    LightBackend(Type type, LightStateHandler handler) : type(type), handler(handler) {
        this->state.color = 0xff000000;
    }
};


namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

class Light : public ILight {
  public:
    Return<Status> setLight(Type type, const LightState& state) override;
    Return<void> getSupportedTypes(getSupportedTypes_cb _hidl_cb) override;

  private:
    std::mutex globalLock;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_LIGHT_V2_0_LIGHT_H
