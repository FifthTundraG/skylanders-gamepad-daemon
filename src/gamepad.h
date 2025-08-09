#include <glib.h>
#include <libevdev/libevdev-uinput.h>

// Button masks
#define DPAD_UP_MASK 0x01
#define DPAD_DOWN_MASK 0x02
#define DPAD_LEFT_MASK 0x04
#define DPAD_RIGHT_MASK 0x08
#define BUTTON_A_MASK 0x10
#define BUTTON_B_MASK 0x20
#define BUTTON_X_MASK 0x40
#define BUTTON_Y_MASK 0x80

// Shoulders and pause masks
#define PAUSE_MASK 0x04
#define SHOULDER_LEFT_MASK 0x10
#define SHOULDER_RIGHT_MASK 0x20

// Both triggers (L/R) will be 0xFF when pressed, 0x00 when not
#define TRIGGER_DOWN 0xFF

extern struct libevdev_uinput *uidev;

void setup_virtual_gamepad(void);
void cleanup_virtual_gamepad(void);
void process_gamepad_data(const guchar *data);