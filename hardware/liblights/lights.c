/*
 * Copyright (C) 2009 The Android Open Source Project
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

#define LOG_TAG "lights"

#define LOG_NDEBUG 0
#include <cutils/log.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>
#include <cutils/properties.h>


/******************************************************************************/

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static struct light_state_t g_notification;
static struct light_state_t g_battery;
static int g_backlight = 255;
static int g_buttons = 0;
static int g_show_charging = 0;

struct led_prop {
    const char *filename;
};

struct led {
    struct led_prop trigger;
    struct led_prop brightness;
    struct led_prop delay_on;
    struct led_prop delay_off;
    struct led_prop blank;
};

enum {
    BOTTOM_LED,
    LCD_BACKLIGHT,
    NUM_LEDS,
};

struct led leds[NUM_LEDS] = {
    [BOTTOM_LED] = {
        .brightness = { "/sys/class/leds/led1_B/brightness"},
        .trigger = { "/sys/class/leds/led1_B/trigger"},
        .delay_on = { "/sys/class/leds/led1_B/delay_on"},
        .delay_off = { "/sys/class/leds/led1_B/delay_off"},
    },
    [LCD_BACKLIGHT] = {
        .brightness = { "/sys/class/leds/lcd-backlight/brightness"},
    },
};

struct light_config {
    int brightness;
    int delay_on;
    int delay_off;
};
// define LED behaviour for notification and charging
//                                            /brightness,  on delay,   off delay
struct light_config LED_CFG_NOTIFICATION    = {255,         500,        2000};
struct light_config LED_CFG_CHARGE          = {255,         250,        10};

/**
 * device methods
 */

void init_globals(void)
{
    pthread_mutex_init(&g_lock, NULL);
}

static int
write_string(struct led_prop *prop, const char *value)
{
    char buffer[20];
    int bytes;
    int amt;

    if (!prop->filename)
        return 0;

    int fd = open(prop->filename, O_RDWR);
    if (fd < 0) {
        LOGE("init_prop: %s cannot be opened (%s)\n", prop->filename,
             strerror(errno));
        return -errno;
    }

    if (fd < 0)
        return 0;

    //LOGV("%s %s: 0x%s\n", __func__, prop->filename, value);

    bytes = snprintf(buffer, sizeof(buffer), "%s\n", value);
    while (bytes > 0) {
        amt = write(fd, buffer, bytes);
        if (amt < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        bytes -= amt;
    }

success:
    return 0;
failed:
    if (fd > 0)
        close(fd);
    return -errno;
}

static int
write_int(struct led_prop *prop, int value)
{
    char buffer[20];
    int bytes = snprintf(buffer, sizeof(buffer), "%d", value);
    return  write_string(prop, buffer);
}

static int
is_lit(struct light_state_t const* state)
{
    return state->color & 0x00ffffff;
}


static int
rgb_to_brightness(struct light_state_t const* state)
{
    int color = state->color & 0x00ffffff;
    return ((77*((color>>16)&0x00ff))
            + (150*((color>>8)&0x00ff)) + (29*(color&0x00ff))) >> 8;
}

static int
set_light_backlight(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);
    //LOGD("%s brightness=%d color=0x%08x",__func__,brightness, state->color);
    pthread_mutex_lock(&g_lock);
    g_backlight = brightness;

    err = write_int(&leds[LCD_BACKLIGHT].brightness, brightness);

    pthread_mutex_unlock(&g_lock);
    return err;
}

static int
set_light_keyboard(struct light_device_t* dev,
        struct light_state_t const* state)
{
    return 0;
}

static int
set_light_buttons(struct light_device_t* dev,
        struct light_state_t const* state)
{
    return 0;
}


static int set_led_light_locked(struct light_state_t const* state, struct light_config *led_config )
{
    struct led *bottom_led = &leds[BOTTOM_LED];
    unsigned int colorRGB = state->color & 0xFFFFFF;
        
    LOGV("%s: led=%s mode=%d color=0x%08x On=%d Off=%d\n", __func__, 
        state==&g_battery?"battery":"notification",
        state->flashMode, state->color, state->flashOnMS, state->flashOffMS);

    switch (state->flashMode) {
    case LIGHT_FLASH_TIMED:
        write_string(&bottom_led->trigger, "timer");      
        write_int(&bottom_led->delay_on, led_config->delay_on);
        write_int(&bottom_led->delay_off, led_config->delay_off);
        //falltrough
    case LIGHT_FLASH_HARDWARE:
    case LIGHT_FLASH_NONE:      //alway enable light if color is set
        if (colorRGB == 0) {
            write_int(&bottom_led->brightness, 0);
        }
        else {
            write_int(&bottom_led->brightness, led_config->brightness);

        }
        break;

    default:
        LOGE("set_led_state colorRGB=%08X, unknown mode %d\n",
        state->color, state->flashMode);
    }
    return 0;
}



static void
handle_led_battery_locked(struct light_device_t* dev)
{
    if (is_lit(&g_battery)) {   //charging has priority over notification
        set_led_light_locked( &g_battery, &LED_CFG_CHARGE);
    } else {
        set_led_light_locked( &g_notification, &LED_CFG_NOTIFICATION);
    }
}


static int
set_light_battery(struct light_device_t* dev,
        struct light_state_t const* state)
{
    if (g_show_charging)
    {
        pthread_mutex_lock(&g_lock);
        g_battery = *state;
        handle_led_battery_locked(dev);
        pthread_mutex_unlock(&g_lock);
    }

    return 0;
}

static int
set_light_notifications(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_notification = *state;
    handle_led_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);

    return 0;
}

static int
set_light_attention(struct light_device_t* dev,
        struct light_state_t const* state)
{
	/* nowplus has no attention, bad nowplus */

    return 0;
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
    char value[PROPERTY_VALUE_MAX];
    int (*set_light)(struct light_device_t* dev,
            struct light_state_t const* state);

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name)) {
        set_light = set_light_backlight;
    }
    else if (0 == strcmp(LIGHT_ID_KEYBOARD, name)) {
        set_light = set_light_keyboard;
    }
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name)) {
        set_light = set_light_buttons;
    }
    else if (0 == strcmp(LIGHT_ID_BATTERY, name)) {
        set_light = set_light_battery;
    }
    else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name)) {
        set_light = set_light_notifications;
    }
    else if (0 == strcmp(LIGHT_ID_ATTENTION, name)) {
        set_light = set_light_attention;
    }
    else {
        return -EINVAL;
    }

    property_get("light.battery.show", value, "");
    if (value[0] == '1') g_show_charging = 1;
  
    pthread_once(&g_init, init_globals);

    struct light_device_t *dev = malloc(sizeof(struct light_device_t));
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
const struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Samsung GT-I8320 lights Module",
    .author = "Mephisto",
    .methods = &lights_module_methods,
};
