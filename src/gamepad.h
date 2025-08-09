#include <glib.h>
#include <libevdev/libevdev-uinput.h>

extern struct libevdev_uinput *uidev;

void setup_virtual_gamepad(void);
void cleanup_virtual_gamepad(void);
void process_gamepad_data(const guchar *data);