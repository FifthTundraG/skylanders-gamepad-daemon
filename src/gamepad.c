#include "gamepad.h"
#include <stdio.h>
#include <libevdev/libevdev-uinput.h>
#include <stdint.h>

struct libevdev_uinput *uidev = NULL;

static void write_event(const unsigned int type, const unsigned int code, const int value) {
    if (uidev) {
        libevdev_uinput_write_event(uidev, type, code, value);
    }
}

void setup_virtual_gamepad(void) {
    if (uidev) {
        g_warning("Cannot setup virtual gamepad: Already exists\n");
        return;
    }

    struct libevdev *dev = libevdev_new();
    if (!dev) {
        g_error("Failed to allocate libevdev\n");
        return;
    }

    libevdev_set_name(dev, "Skylanders GamePad");

    // Enable button events
    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_KEY, BTN_A, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_B, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_X, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_Y, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_TL, NULL);      // L1
    libevdev_enable_event_code(dev, EV_KEY, BTN_TR, NULL);      // R1
    libevdev_enable_event_code(dev, EV_KEY, BTN_TL2, NULL);     // L2
    libevdev_enable_event_code(dev, EV_KEY, BTN_TR2, NULL);     // R2
    libevdev_enable_event_code(dev, EV_KEY, BTN_DPAD_UP, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_DPAD_DOWN, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_DPAD_LEFT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_DPAD_RIGHT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_START, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_SELECT, NULL);

    // Enable analog stick events
    libevdev_enable_event_type(dev, EV_ABS);
    struct input_absinfo abs = { .minimum = -128, .maximum = 127 };
    libevdev_enable_event_code(dev, EV_ABS, ABS_X, &abs);      // Left stick X
    libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &abs);      // Left stick Y
    libevdev_enable_event_code(dev, EV_ABS, ABS_RX, &abs);     // Right stick X
    libevdev_enable_event_code(dev, EV_ABS, ABS_RY, &abs);     // Right stick Y

    if (libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev) < 0) {
        g_error("Failed to create uinput device\n");
        libevdev_free(dev);
        return;
    }

    libevdev_free(dev);
    g_message("Virtual gamepad created at %s\n", libevdev_uinput_get_devnode(uidev));
}

void cleanup_virtual_gamepad(void) {
    if (uidev) {
        g_message("Removing virtual gamepad\n");
        libevdev_uinput_destroy(uidev);
        uidev = NULL;
    }
}

// Parse gamepad data and emit events
void process_gamepad_data(const guchar *data) {    
    uint16_t buttons = data[8];
    int16_t shoulders_and_pause = data[9]; // shoulders and pause button use the same byte because why not
    int16_t trigger_l = data[10]; // pressed when 0xFF
    int16_t trigger_r = data[11]; // ^
    int8_t right_x = (int8_t)data[12];
    int8_t right_y = (int8_t)data[13];
    int8_t left_x = (int8_t)data[14];
    int8_t left_y = (int8_t)data[15];
    
    static uint16_t prev_buttons = 0;
    uint16_t changed = buttons ^ prev_buttons;
    
    if (changed & 0x10) write_event(EV_KEY, BTN_A, (buttons & 0x10) ? 1 : 0);
    if (changed & 0x20) write_event(EV_KEY, BTN_B, (buttons & 0x20) ? 1 : 0);
    if (changed & 0x40) write_event(EV_KEY, BTN_X, (buttons & 0x40) ? 1 : 0);
    if (changed & 0x80) write_event(EV_KEY, BTN_Y, (buttons & 0x80) ? 1 : 0);
    if (changed & 0x01) write_event(EV_KEY, BTN_DPAD_UP, (buttons & 0x01) ? 1 : 0);
    if (changed & 0x02) write_event(EV_KEY, BTN_DPAD_DOWN, (buttons & 0x02) ? 1 : 0);
    if (changed & 0x04) write_event(EV_KEY, BTN_DPAD_LEFT, (buttons & 0x04) ? 1 : 0);
    if (changed & 0x08) write_event(EV_KEY, BTN_DPAD_RIGHT, (buttons & 0x08) ? 1 : 0);

    // Shoulders and pause
    static int16_t prev_shoulders = 0;
    int16_t shoulders_changed = shoulders_and_pause ^ prev_shoulders;
    if (shoulders_changed & 0x04) write_event(EV_KEY, BTN_START, (shoulders_and_pause & 04) ? 1 : 0); // let's have the pause button be our start button, this may change at some point
    if (shoulders_changed & 0x10) write_event(EV_KEY, BTN_TL, (shoulders_and_pause & 0x10) ? 1 : 0);
    if (shoulders_changed & 0x20) write_event(EV_KEY, BTN_TR, (shoulders_and_pause & 0x20) ? 1 : 0);
    
    // Triggers (L2/R2)
    static int16_t prev_trigger_l = 0, prev_trigger_r = 0;
    if (trigger_l != prev_trigger_l) {
        write_event(EV_KEY, BTN_TL2, (trigger_l == 0xFF) ? 1 : 0);
    }
    if (trigger_r != prev_trigger_r) {
        write_event(EV_KEY, BTN_TR2, (trigger_r == 0xFF) ? 1 : 0);
    }
    
    // Analog sticks
    write_event(EV_ABS, ABS_X, left_x);
    write_event(EV_ABS, ABS_Y, -left_y);  // Invert Y axis
    write_event(EV_ABS, ABS_RX, right_x);
    write_event(EV_ABS, ABS_RY, -right_y);
    
    // Send sync event
    write_event(EV_SYN, SYN_REPORT, 0);
    
    prev_buttons = buttons;
    prev_shoulders = shoulders_and_pause;
    prev_trigger_l = trigger_l;
    prev_trigger_r = trigger_r;
}