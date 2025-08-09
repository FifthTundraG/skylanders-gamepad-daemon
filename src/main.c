// Skylanders GamePad Daemon

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libevdev/libevdev-uinput.h>
#include <gio/gio.h>
#include <stdint.h>
#include "main.h"
#include "gamepad.h"

GDBusConnection *conn = NULL;
char *device_path = NULL;
char *char_path = NULL;
gboolean device_connected = FALSE;
GMainLoop *main_loop = NULL;

GHashTable *subscriptions; // key: device_path, value: GamepadSubscription*

GamepadSubscription *subscribe_gamepad(const char *device_path) {
    GamepadSubscription *subscription = g_new0(GamepadSubscription, 1);
    subscription->device_path = g_strdup(device_path);

    // Subscribe to characteristic property changes
    subscription->characteristic_properties_changed_id = g_dbus_connection_signal_subscribe(
        conn,
        "org.bluez",
        "org.freedesktop.DBus.Properties", 
        "PropertiesChanged",
        char_path,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_characteristic_properties_changed,
        NULL,
        NULL);

    // Subscribe to device property changes
    subscription->disconnected_id = g_dbus_connection_signal_subscribe(
        conn,
        "org.bluez",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged", 
        device_path,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_device_properties_changed,
        NULL,
        NULL);

    return subscription;
}

void gamepad_subscription_free(GamepadSubscription *sub) {
    if (sub == NULL)
        return;
    
    g_dbus_connection_signal_unsubscribe(conn, sub->characteristic_properties_changed_id);
    g_dbus_connection_signal_unsubscribe(conn, sub->disconnected_id);
    g_free(sub->device_path);
    g_free(sub);
}

// Find the gamepad device path by scanning BlueZ managed objects for a device with the name DEVICE_NAME and status connected
char *find_gamepad_device_path(void) {
    g_message("Searching for gamepad device...\n");

    if (device_path != NULL) {
        //? should this warn?
        g_message("A device path is already set, a device must already be connected. Ignoring search and returning existing device_path.\n");
        return device_path;
    }

    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(
        conn,
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects",
        NULL,
        G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (!result) {
        g_error("Failed to get managed objects: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }

    GVariantIter *objects = NULL;
    g_variant_get(result, "(a{oa{sa{sv}}})", &objects);

    const char *object_path;
    GVariantIter *interfaces;

    while (g_variant_iter_loop(objects, "{oa{sa{sv}}}", &object_path, &interfaces)) {
        const char *interface_name;
        GVariantIter *properties;

        while (g_variant_iter_loop(interfaces, "{sa{sv}}", &interface_name, &properties)) {
            if (strcmp(interface_name, "org.bluez.Device1") == 0) {
                const char *prop_name;
                GVariant *prop_value;
                const char *name = NULL;
                gboolean connected = FALSE;

                while (g_variant_iter_loop(properties, "{sv}", &prop_name, &prop_value)) {
                    if (strcmp(prop_name, "Name") == 0 || strcmp(prop_name, "Alias") == 0) {
                        name = g_variant_get_string(prop_value, NULL);
                    }
                    if (strcmp(prop_name, "Connected") == 0) {
                        connected = g_variant_get_boolean(prop_value);
                    }
                }

                if (name && strcmp(name, DEVICE_NAME) == 0 && connected) {
                    g_free(device_path);
                    device_path = g_strdup(object_path);
                    g_variant_iter_free(objects);
                    g_variant_unref(result);
                    return device_path;
                }
            }
        }
    }

    g_variant_iter_free(objects);
    g_variant_unref(result);
    return NULL;
}

// Find the characteristic path by UUID and check its properties
char *find_characteristic_path(const char *device_path, const char *uuid) {
    GError *error = NULL;
    GVariant *result = NULL;
    static char char_path[200];
    
    g_message("Searching for characteristic %s...\n", uuid);
    
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
        g_error("Failed to get managed objects: %s\n", error->message);
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
    
    g_warning("Characteristic not found! Found %d total characteristics.\n", characteristics_found);
    g_variant_unref(result);
    return NULL;
}

// Handle GATT characteristic notifications/property changes
void on_characteristic_properties_changed(GDBusConnection *connection,
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
            const guchar *data = g_variant_get_data(property_value);
            process_gamepad_data(data);
        }
    }
}

// Monitor device connection state changes
void on_device_properties_changed(GDBusConnection *connection,
                                         const gchar *sender_name,
                                         const gchar *object_path,
                                         const gchar *interface_name,
                                         const gchar *signal_name,
                                         GVariant *parameters,
                                         gpointer user_data) {
    (void)connection; (void)sender_name; (void)user_data;

    if (strcmp(object_path, device_path) != 0 ||
        strcmp(interface_name, "org.freedesktop.DBus.Properties") != 0 ||
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
            g_message("Device connection state changed: %s\n", connected ? "connected" : "disconnected");
            handle_device_connection_change(connected);
        }
    }
}

// When BlueZ properties change (at /org/bluez/hci0), check if our device appears. if it does, update device_path and monitor it.
void on_bluez_properties_changed(GDBusConnection *connection,
                                        const gchar *sender_name,
                                        const gchar *object_path,
                                        const gchar *interface_name,
                                        const gchar *signal_name,
                                        GVariant *parameters,
                                        gpointer user_data) {
    (void)connection; (void)sender_name; (void)user_data; (void)parameters;
    // let's make sure this is the right signal
    if (strcmp(object_path, "/org/bluez/hci0") != 0 ||
        strcmp(interface_name, "org.freedesktop.DBus.Properties") != 0 ||
        strcmp(signal_name, "PropertiesChanged") != 0) {
        return;
    }

    device_path = find_gamepad_device_path();

    if (device_path != NULL) {
        g_message("Found gamepad device at %s\n", device_path);

        handle_device_connection_change(TRUE);
    } else {
        handle_device_connection_change(FALSE);
    }
}

// Handle device connection/disconnection
void handle_device_connection_change(gboolean connected) {
    if (connected == device_connected) {
        //* i don't want to print below (at least by default), i think it makes the user think something is wrong when in reality it usually just means a device was connected via bluez and it wasn't the gamepad
        //g_warning("Ignoring connection change call, already in state: %s\n", connected ? "connected" : "disconnected");
        return; // No change
    }
    
    device_connected = connected;
    
    if (connected) {
        g_message("Skylanders gamepad connected!\n");
        
        // Wait a bit for services to resolve
        g_usleep(2000000); // 2 seconds
        
        // Find the characteristic
        char_path = find_characteristic_path(device_path, CHARACTERISTIC_UUID);
        if (!char_path) {
            g_error("Could not find characteristic %s\n", CHARACTERISTIC_UUID);
            return;
        }
        
        g_message("Found characteristic at %s\n", char_path);
        
        // Set up virtual gamepad
        setup_virtual_gamepad();
        
        GamepadSubscription *sub = subscribe_gamepad(device_path);
        g_hash_table_insert(subscriptions, g_strdup(device_path), sub);

        GError *error = NULL;
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
            g_error("StartNotify failed: %s\n", error->message);
            g_error_free(error);
            return;
        }
        g_variant_unref(result);
        
        g_message("Skylanders gamepad ready!\n");
    } else {
        g_message("Skylanders gamepad disconnected\n");
        cleanup_virtual_gamepad();
        g_hash_table_remove(subscriptions, device_path);
        device_path = NULL;
        char_path = NULL;
    }
}

// Check initial connection state
void check_initial_connection_state(void) {
    device_path = find_gamepad_device_path();
    if (device_path == NULL) {
        g_message("Could not find device path in initial connection check.\n");
        return;
    }

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
        g_message("Device not found in BlueZ. Waiting for connection...\n");
        if (error) g_error_free(error);
        return;
    }
    
    GVariant *value;
    g_variant_get(result, "(v)", &value);
    gboolean connected = g_variant_get_boolean(value);
    g_variant_unref(value);
    g_variant_unref(result);
    
    if (connected) {
        g_message("Device already connected at startup\n");
        handle_device_connection_change(TRUE);
    } else {
        g_message("Device not connected at startup. Waiting...\n");
    }
}

// Signal handler for clean shutdown
void signal_handler(int sig) {
    (void)sig;
    g_message("Shutting down daemon...\n");
    cleanup_virtual_gamepad();
    if (main_loop) {
        g_main_loop_quit(main_loop);
    }
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
    g_message("Starting Skylanders GamePad Daemon\n");

    // make our subscriptions hashtable exist
    subscriptions = g_hash_table_new_full(
        g_str_hash, g_str_equal,
        g_free,
        (GDestroyNotify)gamepad_subscription_free
    );
    
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
    
    g_message("Connected to D-Bus\n");

    g_message("Checking if device is already connected...\n");
    // Check if device is already connected
    check_initial_connection_state();
        
    g_dbus_connection_signal_subscribe(conn,
        "org.bluez",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged", 
        "/org/bluez/hci0", // monitor all objects
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_bluez_properties_changed,
        NULL,
        NULL);
    g_message("Monitoring for new devices.\n");
    
    // Run daemon
    main_loop = g_main_loop_new(NULL, FALSE);
    
    g_message("Daemon running.\n");
    
    g_main_loop_run(main_loop);
    
    // Cleanup
    cleanup_virtual_gamepad();
    if (conn) {
        g_object_unref(conn);
    }
    g_main_loop_unref(main_loop);
    
    g_message("Daemon stopped\n");
    return 0;
}