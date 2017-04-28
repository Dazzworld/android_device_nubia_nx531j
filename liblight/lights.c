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

#define BATTERY_STATUS_FILE \
        "/sys/class/power_supply/battery/status"

#define BATTERY_STATUS_DISCHARGING  "Discharging"
#define BATTERY_STATUS_NOT_CHARGING "Not charging"
#define BATTERY_STATUS_CHARGING     "Charging"

#define BATTERY_CAPACITY_FILE \
        "/sys/class/power_supply/battery/capacity"

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


/******************************************************************************/

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static struct light_state_t g_notification;
static struct light_state_t g_buttons;
static struct light_state_t g_attention;

static int g_ongoing = ONGOING_NONE;
static int g_last = ONGOING_NONE;

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
write_int(char const* path, int value)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[20];
        int bytes = snprintf(buffer, sizeof(buffer), "%d\n", value);
        ssize_t amt = write(fd, buffer, (size_t)bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int
read_int(char const* path)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        char buffer[20];
        read(fd, &buffer, sizeof(buffer));
        close(fd);
        return atoi(buffer);
    } else {
        if (already_warned == 0) {
            ALOGE("read_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int
read_str(char const* path, char *buffer, size_t size)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        int bytes = read(fd, buffer, size);
        close(fd);
        return bytes;
    } else {
        if (already_warned == 0) {
            ALOGE("read_str failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int
rgb_to_brightness(struct light_state_t const* state)
{
    int color = state->color;

    float alpha = (float) ((color >> 24) & 0xff) / 255;

    int r = (color >> 16) & 0xff;
    int g = (color >> 8) & 0xff;
    int b = (color) & 0xff;

    int brightness = r;
    if (g > brightness) {
        brightness = g;
    } else if (b > brightness) {
        brightness = b;
    }

    brightness = (int) ((float) brightness * alpha);

    return brightness;
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
get_battery_status()
{
    int err;

    char status_str[16];
    int capacity;

    err = read_str(BATTERY_STATUS_FILE, status_str, sizeof(status_str));
    if (err <= 0) {
        ALOGI("failed to read battery status: %d", err);
        return BATTERY_UNKNOWN;
    }

    ALOGI("battery status: %d, %s", err, status_str);

    capacity = read_int(BATTERY_CAPACITY_FILE);
    if (capacity < 0) {
        ALOGI("failed to read battery capacity: %d", capacity);
        return BATTERY_UNKNOWN;
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

static int
set_nubia_led_locked(struct light_device_t* dev,
        struct light_state_t const* state, int source)
{
    int err;

    int brightness, buttons;
    int mode;

    if (!dev) {
        return -1;
    }

    brightness = rgb_to_brightness(state);
    if (brightness > 0) {
        g_ongoing |= source;
        ALOGI("ongoing +: %d = %d", source, g_ongoing);
    } else {
        g_ongoing &= ~source;
        ALOGI("ongoing -: %d = %d", source, g_ongoing);
    }

    buttons = rgb_to_brightness(&g_buttons);

    /* write side button */

    write_int(NUBIA_CHANNEL_FILE, SILDE_CHANNEL);
    if ((g_ongoing & ONGOING_BUTTONS) == 0) {
        write_int(NUBIA_MODE_FILE, RGB_LED_MODE_OFF);
    } else {
        write_int(NUBIA_GRADE_FILE, buttons / 20);
        write_int(NUBIA_MODE_FILE, RGB_LED_MODE_CONSTANT_ON);
    }

    /* write middle ring */

    if (g_ongoing == 0) {
        if (g_battery == BATTERY_CHARGING) {
            mode = RGB_LED_MODE_AUTO_BLINK;
            brightness = BRIGHTNESS_BATTERY_CHARGING;
        } else if (g_battery == BATTERY_FULL) {
            mode = RGB_LED_MODE_CONSTANT_ON;
            brightness = BRIGHTNESS_BATTERY_FULL;
        } else {
            mode = RGB_LED_MODE_OFF;
        }
    } else {
        if (state->flashMode == LIGHT_FLASH_TIMED) {
            mode = RGB_LED_MODE_AUTO_BLINK;
        } else {
            mode = RGB_LED_MODE_CONSTANT_ON;
        }
    }

    // brightness is not always from buttons, sometimes it is 0
    if ((g_ongoing & ONGOING_BUTTONS) != 0 && brightness < buttons) {
        brightness = buttons;
    }

    if (g_battery == BATTERY_LOW) {
        mode = RGB_LED_MODE_AUTO_BLINK;
        brightness = BRIGHTNESS_BATTERY_LOW;
    }

    write_int(NUBIA_CHANNEL_FILE, MIDDLE_CHANNEL);
    write_int(NUBIA_GRADE_FILE, brightness / 10);
    write_int(NUBIA_MODE_FILE, mode);

    g_last = g_ongoing;

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
    g_battery = get_battery_status();
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
