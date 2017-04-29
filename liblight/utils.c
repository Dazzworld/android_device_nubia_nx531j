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

#include <cutils/log.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include "utils.h"

int write_int(char const* path, int value)
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

int read_str(char const* path, char *buffer, size_t size)
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

int rgb_to_brightness(struct light_state_t const* state)
{
    int color = state->color;

    float alpha = (float) ((color >> 24) & 0xff) / 255;

    int brightness = ((77 * ((color >> 16) & 0x00ff))
                    + (150 * ((color >> 8) & 0x00ff))
                    + (29 * (color & 0x00ff))) >> 8;

    brightness = (int) ((float) brightness * alpha);

    return brightness;
}
