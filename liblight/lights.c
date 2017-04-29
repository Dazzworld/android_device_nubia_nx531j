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


// #define LOG_NDEBUG 0

#include <cutils/log.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

#include "lights.h"
#include "battery.h"
#include "utils.h"

/******************************************************************************/

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static struct light_state_t g_notification;
static struct light_state_t g_buttons;
static struct light_state_t g_attention;

static int g_ongoing = ONGOING_NONE;
static int g_battery = BATTERY_UNKNOWN;

/**
 * device methods
 */

void init_globals(void)
{
    // init the mutex
    pthread_mutex_init(&g_lock, NULL);
}

static int
set_light_backlight(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);
    if (!dev) {
        return -1;
    }
    pthread_mutex_lock(&g_lock);
    err = write_int(LCD_FILE, brightness);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int
set_nubia_led_locked(struct light_device_t* dev,
        struct light_state_t const* state, int source)
{
    int err;

    int buttons;
    int mode;

    char grade[16];
    size_t grade_len = 0;

    char fade[16];
    size_t fade_len = 0;

    if (!dev) {
        return -1;
    }

    ALOGI("setting led %d: %08x, %d, %d", source,
        state->color, state->flashOnMS, state->flashOffMS);

    if (rgb_to_brightness(state) > 0) {
        g_ongoing |= source;
        ALOGI("ongoing +: %d = %d", source, g_ongoing);
    } else {
        g_ongoing &= ~source;
        ALOGI("ongoing -: %d = %d", source, g_ongoing);
    }

    buttons = rgb_to_brightness(&g_buttons) / 20;

    /* set side buttons */

    write_int(NUBIA_CHANNEL_FILE, SILDE_CHANNEL);
    if (g_ongoing & ONGOING_BUTTONS) {
        write_int(NUBIA_GRADE_FILE, buttons);
        write_int(NUBIA_MODE_FILE, RGB_LED_MODE_CONSTANT_ON);
    } else {
        write_int(NUBIA_MODE_FILE, RGB_LED_MODE_OFF);
    }

    /* set middle ring */

    if (g_ongoing & ONGOING_NOTIFICATION) {
        mode = RGB_LED_MODE_AUTO_BLINK;
        grade_len = sprintf(grade, "%d %d\n",
            buttons,
            buttons + rgb_to_brightness(&g_notification) / 20);
    }
    else if (g_ongoing & ONGOING_ATTENTION) {
        mode = RGB_LED_MODE_AUTO_BLINK;
        grade_len = sprintf(grade, "%d %d\n",
            buttons,
            buttons + rgb_to_brightness(&g_attention) / 20);
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

    if (state->flashMode == LIGHT_FLASH_TIMED) {
        fade_len = sprintf(fade, "%d %d %d\n",
            1, state->flashOnMS / 400, state->flashOffMS / 400);
    }

    write_int(NUBIA_CHANNEL_FILE, MIDDLE_CHANNEL);

    if (grade_len > 0) {
        write_str(NUBIA_GRADE_FILE, grade, grade_len);
    }

    if (fade_len > 0) {
        write_str(NUBIA_FADE_FILE, fade, fade_len);
    }

    write_int(NUBIA_MODE_FILE, mode);

    return 0;
}

static int
set_light_battery(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    if (!dev) {
        return -1;
    }

    pthread_mutex_lock(&g_lock);
    g_battery = get_battery_status(state);
    err = set_nubia_led_locked(dev, state, 0);
    pthread_mutex_unlock(&g_lock);

    ALOGI("battery status changed: %d", g_battery);

    return err;
}

static int
set_light_notifications(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    if (!dev) {
        return -1;
    }

    pthread_mutex_lock(&g_lock);
    g_notification = *state;
    err = set_nubia_led_locked(dev, state, ONGOING_NOTIFICATION);
    pthread_mutex_unlock(&g_lock);

    return err;
}

static int
set_light_attention(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    if (!dev) {
        return -1;
    }

    pthread_mutex_lock(&g_lock);
    g_attention = *state;
    err = set_nubia_led_locked(dev, state, ONGOING_ATTENTION);
    pthread_mutex_unlock(&g_lock);

    return err;
}

static int
set_light_buttons(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    if (!dev) {
        return -1;
    }

    pthread_mutex_lock(&g_lock);
    g_buttons = *state;
    err = set_nubia_led_locked(dev, state, ONGOING_BUTTONS);
    pthread_mutex_unlock(&g_lock);

    return err;
}

/** Close the lights device */
static int
close_lights(struct light_device_t *dev)
{
    if (dev) {
        free(dev);
    }
    return 0;
}


/******************************************************************************/

/**
 * module methods
 */

/** Open a new instance of a lights device using name */
static int open_lights(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
    int (*set_light)(struct light_device_t* dev,
            struct light_state_t const* state);

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name))
        set_light = set_light_backlight;
    else if (0 == strcmp(LIGHT_ID_BATTERY, name))
        set_light = set_light_battery;
    else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name))
        set_light = set_light_notifications;
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name))
        set_light = set_light_buttons;
    else if (0 == strcmp(LIGHT_ID_ATTENTION, name))
        set_light = set_light_attention;
    else
        return -EINVAL;

    pthread_once(&g_init, init_globals);

    struct light_device_t *dev = malloc(sizeof(struct light_device_t));

    if (!dev)
        return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t*)dev;
    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

/*
 * The lights Module
 */
struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "lights Module",
    .author = "Google, Inc., XiNGRZ",
    .methods = &lights_module_methods,
};
