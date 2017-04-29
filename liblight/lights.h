/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2014 The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 The CyanogenMod Project
 * Copyright (C) 2017 The MoKee Open Source Project
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

#ifndef NX531J_LIGHTS_IMPL_H
#define NX531J_LIGHTS_IMPL_H


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


#endif  // NX531J_LIGHTS_IMPL_H
