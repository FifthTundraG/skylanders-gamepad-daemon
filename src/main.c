#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <libevdev/libevdev-uinput.h>
#include <gio/gio.h>
#include <stdint.h>

#define DEVICE_MAC "D2:C6:F7:00:76:A8" // temp: will dynamically find this later
#define CHARACTERISTIC_UUID "533e1541-3abe-f33f-cd00-594e8b0a8ea3"

static GDBusConnection *conn = NULL;
static struct libevdev_uinput *uidev = NULL;
static char *device_path = NULL;
static char *char_path = NULL;
static gboolean device_connected = FALSE;
static GMainLoop *main_loop = NULL;

// Convert MAC address format for BlueZ D-Bus paths
static char *get_device_path(void) {
    static char path[100];
    char mac_copy[32];
    
    strcpy(mac_copy, DEVICE_MAC);
    for (char *p = mac_copy; *p; ++p) {
        if (*p == ':') *p = '_';
    }
    
    snprintf(path, sizeof(path), "/org/bluez/hci0/dev_%s", mac_copy);
    return path;
}

static void write_event(unsigned int type, unsigned int code, int value) {
    if (uidev) {
        libevdev_uinput_write_event(uidev, type, code, value);
    }
}

static void setup_virtual_gamepad(void) {
    if (uidev) {
        printf("Virtual gamepad already exists\n");
        return;
    }

    struct libevdev *dev = libevdev_new();
    if (!dev) {
        fprintf(stderr, "Failed to allocate libevdev\n");
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
        fprintf(stderr, "Failed to create uinput device\n");
        libevdev_free(dev);
        return;
    }

    libevdev_free(dev);
    printf("Virtual gamepad created at %s\n", libevdev_uinput_get_devnode(uidev));
}

static void cleanup_virtual_gamepad(void) {
    if (uidev) {
        printf("Removing virtual gamepad\n");
        libevdev_uinput_destroy(uidev);
        uidev = NULL;
    }
}

// Parse gamepad data and emit events
static void process_gamepad_data(const guchar *data, gsize length) {
    printf("Raw data [%zu bytes]: ", length);
    for (gsize i = 0; i < length; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
    
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

// Handle GATT characteristic notifications/property changes
static void on_properties_changed(GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data) {
    (void)connection; (void)sender_name; (void)user_data; // mark as unused
    
    if (strcmp(interface_name, "org.freedesktop.DBus.Properties") != 0 ||
        strcmp(signal_name, "PropertiesChanged") != 0) {
        return;
    }
    
    // Check if this is our characteristic
    if (!char_path || strcmp(object_path, char_path) != 0) {
        return;
    }
    
    const char *iface;
    GVariantIter *changed_properties;
    GVariantIter *invalidated_properties;
    
    g_variant_get(parameters, "(&sa{sv}as)", &iface, &changed_properties, &invalidated_properties);
    
    // check if the interface being reported is the GATT characteristic interface
    if (strcmp(iface, "org.bluez.GattCharacteristic1") != 0) {
        return;
    }
    
    const char *property_name;
    GVariant *property_value;
    
    while (g_variant_iter_loop(changed_properties, "{&sv}", &property_name, &property_value)) {
        if (strcmp(property_name, "Value") == 0) {
            gsize length;
            const guchar *data = g_variant_get_fixed_array(property_value, &length, sizeof(guchar));
            process_gamepad_data(data, length);
        }
    }
}

// Find the characteristic path by UUID and check its properties
static char *find_characteristic_path(const char *device_path, const char *uuid) {
    GError *error = NULL;
    GVariant *result = NULL;
    static char char_path[200];
    
    printf("Searching for characteristic %s...\n", uuid);
    
    // Get all objects from BlueZ
    result = g_dbus_connection_call_sync(conn,
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects",
        NULL,
        G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);
        
    if (!result) {
        g_printerr("Failed to get managed objects: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }
    
    GVariantIter *objects;
    g_variant_get(result, "(a{oa{sa{sv}}})", &objects);
    
    const char *object_path;
    GVariantIter *interfaces;
    int characteristics_found = 0;
    
    while (g_variant_iter_loop(objects, "{oa{sa{sv}}}", &object_path, &interfaces)) {
        // Check if this object belongs to our device
        if (!g_str_has_prefix(object_path, device_path)) {
            continue;
        }
        
        const char *interface_name;
        GVariantIter *properties;
        
        while (g_variant_iter_loop(interfaces, "{sa{sv}}", &interface_name, &properties)) {
            if (strcmp(interface_name, "org.bluez.GattCharacteristic1") == 0) {
                characteristics_found++;
                const char *prop_name;
                GVariant *prop_value;
                const char *char_uuid = NULL;
                
                // First pass: get UUID and flags
                while (g_variant_iter_loop(properties, "{sv}", &prop_name, &prop_value)) {
                    if (strcmp(prop_name, "UUID") == 0) {
                        char_uuid = g_variant_get_string(prop_value, NULL);
                    } else if (strcmp(prop_name, "Flags") == 0) {
                        GVariantIter *flags_iter;
                        const char *flag;
                        g_variant_get(prop_value, "as", &flags_iter);
                    }
                }
                
                if (char_uuid && g_ascii_strcasecmp(char_uuid, uuid) == 0) {
                    strncpy(char_path, object_path, sizeof(char_path) - 1);
                    char_path[sizeof(char_path) - 1] = '\0';
                    g_variant_unref(result);
                    return char_path;
                }
            }
        }
    }
    
    printf("Characteristic not found! Found %d total characteristics.\n", characteristics_found);
    g_variant_unref(result);
    return NULL;
}

// Handle device connection/disconnection
static void handle_device_connection_change(gboolean connected) {
    if (connected == device_connected) {
        return; // No change
    }
    
    device_connected = connected;
    
    if (connected) {
        printf("Skylanders gamepad connected!\n");
        
        // Wait a bit for services to resolve
        g_usleep(2000000); // 2 seconds
        
        // Find the characteristic
        char_path = find_characteristic_path(device_path, CHARACTERISTIC_UUID);
        if (!char_path) {
            printf("Could not find characteristic %s\n", CHARACTERISTIC_UUID);
            return;
        }
        
        printf("Found characteristic at %s\n", char_path);
        
        // Set up virtual gamepad
        setup_virtual_gamepad();
        
        // Subscribe to property changes
        g_dbus_connection_signal_subscribe(conn,
            "org.bluez",
            "org.freedesktop.DBus.Properties", 
            "PropertiesChanged",
            char_path,
            NULL,
            G_DBUS_SIGNAL_FLAGS_NONE,
            on_properties_changed,
            NULL,
            NULL);

        GError *error = NULL;
        
        // I'm not sure exactly why but this needs to be called
        GVariant *result = g_dbus_connection_call_sync(conn,
            "org.bluez",
            char_path,
            "org.bluez.GattCharacteristic1",
            "StartNotify",
            NULL,
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &error);
            
        if (!result) {
            printf("StartNotify failed: %s\n", error->message);
            g_error_free(error);
            return;
        }
        
        printf("Skylanders gamepad ready!\n");
        
    } else {
        printf("Skylanders gamepad disconnected\n");
        cleanup_virtual_gamepad();
        char_path = NULL;
    }
}

// Monitor device connection state changes
static void on_device_properties_changed(GDBusConnection *connection,
                                         const gchar *sender_name,
                                         const gchar *object_path,
                                         const gchar *interface_name,
                                         const gchar *signal_name,
                                         GVariant *parameters,
                                         gpointer user_data) {
    (void)connection; (void)sender_name; (void)user_data;
    
    if (strcmp(object_path, device_path) != 0 ||
        strcmp(interface_name, "org.bluez.Device1") != 0 ||
        strcmp(signal_name, "PropertiesChanged") != 0) {
        return;
    }
    
    const char *iface;
    GVariantIter *changed_properties;
    GVariantIter *invalidated_properties;
    
    g_variant_get(parameters, "(&sa{sv}as)", &iface, &changed_properties, &invalidated_properties);
    
    const char *property_name;
    GVariant *property_value;
    
    while (g_variant_iter_loop(changed_properties, "{&sv}", &property_name, &property_value)) {
        if (strcmp(property_name, "Connected") == 0) {
            gboolean connected = g_variant_get_boolean(property_value);
            printf("Device connection state changed: %s\n", connected ? "connected" : "disconnected");
            handle_device_connection_change(connected);
        }
    }
}

// Check initial connection state
static void check_initial_connection_state(void) {
    GError *error = NULL;
    
    GVariant *result = g_dbus_connection_call_sync(conn,
        "org.bluez",
        device_path,
        "org.freedesktop.DBus.Properties",
        "Get",
        g_variant_new("(ss)", "org.bluez.Device1", "Connected"),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);
        
    if (!result) {
        printf("Device not found in BlueZ. Waiting for connection...\n");
        if (error) g_error_free(error);
        return;
    }
    
    GVariant *value;
    g_variant_get(result, "(v)", &value);
    gboolean connected = g_variant_get_boolean(value);
    g_variant_unref(value);
    g_variant_unref(result);
    
    if (connected) {
        printf("Device already connected at startup\n");
        handle_device_connection_change(TRUE);
    } else {
        printf("Device not connected at startup. Waiting...\n");
    }
}

// Signal handler for clean shutdown
static void signal_handler(int sig) {
    (void)sig;
    printf("\nShutting down daemon...\n");
    cleanup_virtual_gamepad();
    if (main_loop) {
        g_main_loop_quit(main_loop);
    }
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
    printf("Starting Skylanders GamePad Daemon\n");
    printf("Target device: %s\n", DEVICE_MAC);
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    GError *error = NULL;
    
    // Connect to BlueZ via D-Bus
    conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!conn) {
        g_printerr("Failed to connect to system bus: %s\n", error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        return 1;
    }
    
    printf("Connected to D-Bus\n");
    
    // Set up device path
    device_path = get_device_path();
    printf("Monitoring device at %s\n", device_path);
    
    // Subscribe to device property changes
    g_dbus_connection_signal_subscribe(conn,
        "org.bluez",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged", 
        device_path,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_device_properties_changed,
        NULL,
        NULL);
    
    // Check if device is already connected
    check_initial_connection_state();
    
    // Run daemon
    main_loop = g_main_loop_new(NULL, FALSE);
    
    printf("Daemon running.\n");
    
    g_main_loop_run(main_loop);
    
    // Cleanup
    cleanup_virtual_gamepad();
    if (conn) {
        g_object_unref(conn);
    }
    g_main_loop_unref(main_loop);
    
    printf("Daemon stopped\n");
    return 0;
}