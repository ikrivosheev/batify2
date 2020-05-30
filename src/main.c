#include <glib.h>
#include <libnotify/notify.h>
#include <stdio.h>
#include <locale.h>
#include <libintl.h>

#include "battery.h"

#define PROGRAM_NAME "batify"
#define DEFAULT_INTERVAL 5
#define DEFAULT_LOW_LEVEL 20
#define DEFAULT_CRITICAL_LEVEL 10
#define DEFAULT_FULL_CAPACITY 98
#define SYSFS_BASE_PATH "/sys/class/power_supply/"
#define LOG_IF


typedef enum 
{
    LOW_LEVEL,
    CRITICAL_LEVEL,
} BATTERY_LEVEL;

static struct config
{
    gint interval;
    gint timeout;
    gint low_level;
    gint critical_level;
    gint full_capacity;
} config = {
    DEFAULT_INTERVAL,
    NOTIFY_EXPIRES_DEFAULT,
    DEFAULT_LOW_LEVEL,
    DEFAULT_CRITICAL_LEVEL,
    DEFAULT_FULL_CAPACITY
};

static struct _MainContext 
{
    gchar* sysfs_battery_path;
    BATTERY_STATUS prev_status;
    gboolean low_level_notified;
    gboolean critical_level_notified;
};

typedef struct _MainContext MainContext;


static GOptionEntry option_entries[] =
{
    {"interval", 'i', 0, G_OPTION_ARG_INT, &config.interval, "Update interval in seconds", NULL},
    {"timeout", 't', 0, G_OPTION_ARG_INT, &config.timeout, "Notification timeout", NULL},
    {"low-level", 'l', 0, G_OPTION_ARG_INT, &config.low_level, "Low battery level in percent", NULL},
    {"critical-level", 'c', 0, G_OPTION_ARG_INT, &config.critical_level, "Critical battery level in percent", NULL},
    {"full-capacity", 'f', 0, G_OPTION_ARG_INT, &config, "Full capacity for battery", NULL},
    {NULL}
};

static void notify_message(
    NotifyNotification** notification, 
    const gchar* summary, 
    const gchar* body,
    NotifyUrgency urgency,
    GdkPixbuf* pixbuf,
    gint timeout)
{
    *notification = notify_notification_new(summary, body, NULL);
    notify_notification_set_timeout(*notification, timeout);
    notify_notification_set_urgency(*notification, urgency);
    if (pixbuf != NULL)
    {
        notify_notification_set_image_from_pixbuf(*notification, pixbuf);
    }

    notify_notification_show(*notification, NULL);
}


static gchar* get_battery_status_summery_string(BATTERY_STATUS status)
{
    static gchar string[255];
    switch(status)
    {
        case UNKNOWN_STATUS:
            g_strlcpy(string, "Battery is unknown!", 255);
            break;
        case CHARGING_STATUS:
            g_strlcpy(string, "Battery is charging", 255);
            break;
        case DISCHARGING_STATUS:
            g_strlcpy(string, "Battery is discharging", 255);
            break;
        case NOT_CHARGING_STATUS:
            g_strlcpy(string, "Battery is not charging", 255);
            break;
        case CHARGED_STATUS:
            g_strlcpy(string, "Battery is charged", 255);
            break;
        default:
            string[0] = '\0';
            break;
    }
    return string;
}

static gchar* get_battery_level_summery_string(BATTERY_LEVEL level)
{
    static gchar string[255];
    switch(level)
    {
        case LOW_LEVEL:
            g_strlcpy(string, "Battery level is low!", 255);
            break;
        case CRITICAL_LEVEL:
            g_strlcpy(string, "Battery level is critical!", 255);
            break;
    }
    return string;
}

static gchar* get_battery_body_string(guint64 percentage, guint64 seconds)
{
    static gchar body_string[255];
    if (seconds == 0)
    {
        g_sprintf(body_string, "%d%% of 100%%", percentage);
        return body_string;
    }

    guint hours, minutes;
    minutes = seconds / 60;
    hours = minutes / 60; 
    minutes = minutes - hours * 60;
    if (hours > 0)
    {
        g_sprintf(body_string, 
                  "%d%% %02d:%02d remaining", 
                  percentage,
                  hours,
                  minutes);
    }
    else
    {
        g_sprintf(body_string, 
                  "%d%% 00:%02d remaining", 
                  percentage, 
                  minutes);
    }
    return body_string;

}

static void battery_status_notification(
    NotifyNotification* notification, 
    BATTERY_STATUS status,
    guint64 percentage,
    guint64 seconds)

{
    notify_message(
        &notification, 
        get_battery_status_summery_string(status),
        get_battery_body_string(percentage, seconds),
        NOTIFY_URGENCY_NORMAL,
        NULL,
        config.timeout);
}

static void battery_level_notification(
    NotifyNotification* notification,
    BATTERY_LEVEL level,
    guint64 percentage,
    guint64 seconds)
{
    NotifyUrgency urgency;
    switch(level)
    {
        case LOW_LEVEL:
            urgency = NOTIFY_URGENCY_NORMAL;
            break;
        case CRITICAL_LEVEL:
            urgency = NOTIFY_URGENCY_CRITICAL;
            break;
    }

    notify_message(
        &notification,
        get_battery_level_summery_string(level),
        get_battery_body_string(percentage, seconds),
        urgency,
        NULL, 
        config.timeout
    );
}

static gboolean battery_handler_check(MainContext* context)
{
    guint64 capacity;
    guint64 seconds;
    BATTERY_STATUS status;
    NotifyNotification* notification;

    g_debug("Get battery status");
    g_return_val_if_fail(get_battery_status(context->sysfs_battery_path, &status, NULL), FALSE);

    switch(status)
    {
        case UNKNOWN_STATUS:
            g_debug("Got UNKNOWN_STATUS");
            context->low_level_notified = FALSE;
            context->critical_level_notified = FALSE;
            
            if (context->prev_status == status)
                break;
            
            g_debug("Get battery capacity");
            g_return_val_if_fail(get_battery_capacity(context->sysfs_battery_path, &capacity, NULL), G_SOURCE_CONTINUE);
            if (capacity >= config.full_capacity)
            {
                g_debug("Capacity is greater then full capacity: %d", config.full_capacity);
                battery_status_notification(notification, CHARGED_STATUS, capacity, 0);
            }
            break;
        case CHARGED_STATUS:
            g_debug("Got CHARGED_STATUS");
            context->low_level_notified = FALSE;
            context->critical_level_notified = FALSE;
            if (context->prev_status != status)
                battery_status_notification(notification, status, 100, 0);
            break;
        case CHARGING_STATUS:
            g_debug("Got CHARGING_STATUS");
            context->low_level_notified = FALSE;
            context->critical_level_notified = FALSE;
            if (context->prev_status == status)
                break;
            
            g_debug("Get battery capacity");
            g_return_val_if_fail(get_battery_capacity(context->sysfs_battery_path, &capacity, NULL), G_SOURCE_CONTINUE);

            g_debug("Get battery time");
            g_return_val_if_fail(get_battery_time(context->sysfs_battery_path, status, &seconds, NULL), G_SOURCE_CONTINUE);

            battery_status_notification(notification, status, capacity, seconds);
            break;
        case DISCHARGING_STATUS:
            g_debug("Got DISCHARGING_STATUS");
        case NOT_CHARGING_STATUS:
            g_debug("Got NOT_CHARGING_STATUS");

            g_debug("Get battery capacity");
            g_return_val_if_fail(get_battery_capacity(context->sysfs_battery_path, &capacity, NULL), G_SOURCE_CONTINUE);

            g_debug("Get battery time");
            g_return_val_if_fail(get_battery_time(context->sysfs_battery_path, status, &seconds, NULL), G_SOURCE_CONTINUE);

            if (context->prev_status != status)
            {
                battery_status_notification(notification, status, capacity, seconds);
            }
            if ((context->critical_level_notified == FALSE) 
                && (capacity <= config.critical_level))
            {
                context->low_level_notified = FALSE;
                context->critical_level_notified = TRUE;
                battery_level_notification(notification, CRITICAL_LEVEL, capacity, seconds);
            }
            if  ((context->low_level_notified == FALSE) 
                 && (capacity > config.critical_level) 
                 && (capacity <= config.low_level))
            {
                context->low_level_notified = TRUE;
                context->critical_level_notified = FALSE;
                battery_level_notification(notification, LOW_LEVEL, capacity, seconds);
            }
            break;
    }
    context->prev_status = status;
}


static gboolean options_init(int argc, char* argv[])
{
    GError* error = NULL;
    GOptionContext* option_context;
    option_context = g_option_context_new("[BATTERY ID]");
    g_option_context_add_main_entries(option_context, option_entries, PROGRAM_NAME);

    if (g_option_context_parse(option_context, &argc, &argv, &error) == FALSE) 
    {
        g_prefix_error(&error, "Cannot parse command line arguments: ");
        g_warning(error->message);
        g_error_free(error); 
        return FALSE;
    }
    g_option_context_free(option_context);

    if (argc < 2) 
    {
        g_warning("[BATTERY ID] did'n set");
        return FALSE;
    }
    if (config.low_level < 0 || config.low_level > 100)
    {
        g_warning("Invalid low level! Low level should be greater then 0, less then 100");
        return FALSE;
    }
    if (config.critical_level < 0 || config.critical_level > 100)
    {   
        g_warning("Invalid critical level! Critical level should be greater then 0, less then 100");
        return FALSE;
    }
    if (config.full_capacity < 0 || config.full_capacity > 100)
    {
        g_warning("Invalid full capacity! Full capacity should be greater then 0, less then 100");
        return FALSE;
    }

    if (config.low_level < config.critical_level)
    {
        g_warning("Low level should be less then critical level");
        return FALSE;
    }
    if (config.full_capacity < config.critical_level)
    {
        g_warning("Full capacity should be greater then critical level");
        return FALSE;
    }
    
    return TRUE;
}

int main(int argc, char* argv[]) 
{
    GMainLoop* loop;
    MainContext main_context;
    
    setlocale (LC_ALL, "");
    g_return_val_if_fail(options_init(argc, argv), 1);
    g_debug("Options have been initialized");

    g_return_val_if_fail(notify_init(PROGRAM_NAME), 1);
    g_debug("Notify have been initialized");

    main_context.sysfs_battery_path = g_strjoin("", SYSFS_BASE_PATH, "BAT", argv[1], NULL);
    main_context.low_level_notified = FALSE;
    main_context.critical_level_notified = FALSE;
    
    loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add_seconds(
        config.interval, 
        (GSourceFunc)battery_handler_check, 
        (gpointer)&main_context
    );
    
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    notify_uninit();
    g_free(main_context.sysfs_battery_path);
    
    return 0;
}
