#ifndef SKYLANDERS_GAMEPAD_H
#define SKYLANDERS_GAMEPAD_H

#include <glib.h>
#include <gio/gio.h>
#include <libevdev/libevdev-uinput.h>
#include <stdint.h>

// --- Constants ---
#define DEVICE_NAME "Skylanders GamePad"
#define CHARACTERISTIC_UUID "533e1541-3abe-f33f-cd00-594e8b0a8ea3"

// --- Structs ---
typedef struct {
    char *device_path;
    guint characteristic_properties_changed_id;
    guint disconnected_id;
} GamepadSubscription;

// --- Global Variables ---
extern GDBusConnection *conn;
extern struct libevdev_uinput *uidev;
extern char *device_path;
extern char *char_path;
extern gboolean device_connected;
extern GMainLoop *main_loop;
extern GHashTable *subscriptions;

// --- Gamepad I/O ---
void write_event(unsigned int type, unsigned int code, int value);
void setup_virtual_gamepad(void);
void cleanup_virtual_gamepad(void);
void process_gamepad_data(const guchar *data);
// Gamepad Subscriptions
GamepadSubscription *subscribe_gamepad(const char *device_path);
void gamepad_subscription_free(GamepadSubscription *sub);

// --- BlueZ Helpers ---
char *find_gamepad_device_path(void);
char *find_characteristic_path(const char *device_path, const char *uuid);

// --- Device Connection ---
void handle_device_connection_change(gboolean connected);
void check_initial_connection_state(void);

// --- D-Bus Signal Handlers ---
void on_characteristic_properties_changed(GDBusConnection *connection,
                                          const gchar *sender_name,
                                          const gchar *object_path,
                                          const gchar *interface_name,
                                          const gchar *signal_name,
                                          GVariant *parameters,
                                          gpointer user_data);

void on_device_properties_changed(GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data);

void on_bluez_properties_changed(GDBusConnection *connection,
                                 const gchar *sender_name,
                                 const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *signal_name,
                                 GVariant *parameters,
                                 gpointer user_data);

// --- Signal Handler ---
void signal_handler(int sig);

#endif // SKYLANDERS_GAMEPAD_H
