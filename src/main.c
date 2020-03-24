#include <glib.h>
#include <libnotify/notify.h>
#include <stdio.h>
#include <locale.h>
#include <signal.h>
#include <libintl.h>
#include <sys/inotify.h>

#include "battery.h"

#define PROGRAM_NAME "batify"
#define DEFAULT_INTERVAL 5
#define DEFAULT_LOW_LEVEL 20
#define DEFAULT_CRITICAL_LEVEL 10
#define SYSFS_BASE_PATH "/sys/class/power_supply/"
#define LOG_ERROR(string, ...) g_printerr(PROGRAM_NAME ": " #string "\n", ##__VA_ARGS__)
#define EXIT_ON_FALSE(condition, exit_code) \
{ \
    if (condition == FALSE) \
        return 1; \
}
#define EXIT_ON_FALSE_MESSAGE(condition, exit_code, error_message, ...) \
{ \
    if (condition == FALSE) \
    { \
        LOG_ERROR(error_message, ##__VA_ARGS__); \
        return exit_code; \
    } \
}
#define GOTO_ON_FALSE(condition, label) \
{ \
    if (condition == FALSE) \
    { \
        goto label; \
    } \
}


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
} config = {
    DEFAULT_INTERVAL,
    NOTIFY_EXPIRES_DEFAULT,
    DEFAULT_LOW_LEVEL,
    DEFAULT_CRITICAL_LEVEL,
};

static gboolean shutdown = FALSE;


static GOptionEntry option_entries[] =
{
    {"interval", 'i', 0, G_OPTION_ARG_INT, &config.interval, "Update interval in seconds", NULL},
    {"timeout", 't', 0, G_OPTION_ARG_INT, &config.timeout, "Notification timeout", NULL},
    {"low-level", 'l', 0, G_OPTION_ARG_INT, &config.low_level, "Low battery level in percent", NULL},
    {"critical-level", 'c', 0, G_OPTION_ARG_INT, &config.critical_level, "Critical battery level in percent", NULL},
    {NULL}
};

static void signal_handler(int signum)
{
    shutdown = TRUE;
}

static void notify_message(
    NotifyNotification** notification, 
    const gchar* summary, 
    const gchar* body,
    NotifyUrgency urgency,
    gint timeout)
{
    *notification = notify_notification_new(summary, body, NULL);
    
    notify_notification_set_timeout(*notification, timeout);
    notify_notification_set_urgency(*notification, urgency);
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
        config.timeout
    );
}

static inline gboolean battery_remaining(BATTERY_STATUS status)
{
    switch (status)
    {
        case DISCHARGING_STATUS:
        case NOT_CHARGING_STATUS:
            return TRUE;
        case CHARGING_STATUS:
        case CHARGED_STATUS:
            return FALSE;
    }
    return FALSE;
}


static gboolean options_init(int argc, char* argv[])
{
    GError* error = NULL;
    GOptionContext* option_context;
    option_context = g_option_context_new("[BATTERY ID]");
    g_option_context_add_main_entries(option_context, option_entries, PROGRAM_NAME);

    if (g_option_context_parse(option_context, &argc, &argv, &error) == FALSE) 
    {
        LOG_ERROR("Cannot parse command line arguments: %s", error->message);
        g_error_free(error); 
        error = NULL;
        return FALSE;
    }
    g_option_context_free(option_context);

    if (argc < 2) 
    {
        LOG_ERROR("[BATTERY ID] didn't set");
        return FALSE;
    }
    if (config.low_level < 0 || config.low_level > 100)
    {
        LOG_ERROR("Invalid low level! Low level should be greater then 0, less then 100");
        return FALSE;
    }
    if (config.critical_level < 0 || config.critical_level > 100)
    {   
        LOG_ERROR("Invalid critical level!, Critical level should be greater then 0, less then 100");
        return FALSE;
    }

    if (config.low_level < config.critical_level)
    {
        LOG_ERROR("Low level should be less then critical level");
        return FALSE;
    }
    return TRUE;
}

static gboolean open_inotify_fd(
    const gchar* sysfs_battery_path, 
    int* inotify_fd, 
    int* inotify_wd)
{
    *inotify_fd = inotify_init();
    if (*inotify_fd == -1)
    {
        LOG_ERROR("Cannot init inotify");
        return FALSE;
    }
    *inotify_wd = inotify_add_watch(*inotify_fd, sysfs_battery_path, IN_MODIFY);
    if (*inotify_wd == -1)
    {
        close(*inotify_fd);
        LOG_ERROR("Cannot add watch to inotify by path: %s", sysfs_battery_path);
        return FALSE;
    }
    return TRUE;
}

int main(int argc, char* argv[]) 
{
    int return_code = 0;
    int inotify_fd, inotify_wd, inotify_read;
    char inotify_buffer;
    gchar* sysfs_battery_path;
    guint64 seconds, percentage;
    NotifyNotification* notification;
    BATTERY_STATUS prev_status, next_status; 
    gboolean battery_level_low = FALSE;
    gboolean battery_level_critical = FALSE;
    
    setlocale (LC_ALL, "");
    EXIT_ON_FALSE(options_init(argc, argv), 1);
    sysfs_battery_path = g_strjoin("", SYSFS_BASE_PATH, "BAT", argv[1], NULL);
    
    EXIT_ON_FALSE_MESSAGE(notify_init(PROGRAM_NAME), 1, "Cannot init libnotify");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    GOTO_ON_FALSE(open_inotify_fd(sysfs_battery_path, &inotify_fd, &inotify_wd), finish);

    while (shutdown == FALSE) 
    {
        if (get_battery_status(sysfs_battery_path, &next_status) == FALSE)
        {
            LOG_ERROR("Cannot get battery status");
            return_code = 1;
            goto finish_inotify;
        }
        if (get_battery_capacity(sysfs_battery_path, &percentage) == FALSE)
        {
            LOG_ERROR("Cannot get battery capacity");
            return_code = 1;
            goto finish_inotify;
        }
        if (get_battery_time(sysfs_battery_path, battery_remaining(next_status), &seconds) == FALSE)
        {
            LOG_ERROR("Cannot get battery time");
            return_code = 1;
            goto finish_inotify;
        }
        switch(next_status)
        {
            case UNKNOWN_STATUS:
                battery_level_low = FALSE;
                battery_level_critical = FALSE;
                break;
            case CHARGED_STATUS:
                battery_level_low = FALSE;
                battery_level_critical = FALSE;
                if (prev_status == next_status)
                    break;
                battery_status_notification(notification, next_status, percentage, seconds);
                break;
            case CHARGING_STATUS:
                battery_level_low = FALSE;
                battery_level_critical = FALSE;
                if (prev_status == next_status)
                    break;
                battery_status_notification(notification, next_status, percentage, seconds);
                break;
            case DISCHARGING_STATUS:
            case NOT_CHARGING_STATUS:
                if (prev_status != next_status)
                {
                    battery_status_notification(notification, next_status, percentage, seconds);
                }
                if ((battery_level_critical == FALSE) 
                    && (percentage <= config.critical_level))
                {
                    battery_level_low = FALSE;
                    battery_level_critical = TRUE;
                    battery_level_notification(notification, CRITICAL_LEVEL, percentage, seconds);
                }
                if  ((battery_level_low == FALSE) 
                     && (percentage > config.critical_level) 
                     && (percentage <= config.low_level))
                {
                    battery_level_low = TRUE;
                    battery_level_critical = FALSE;
                    battery_level_notification(notification, LOW_LEVEL, percentage, seconds);
                }
                break;
        }

        prev_status = next_status;
        g_usleep(config.interval * G_USEC_PER_SEC);
    }

finish_inotify:
    inotify_rm_watch(inotify_fd, inotify_wd);
    close(inotify_fd);
finish:
    notify_uninit();
    g_free(sysfs_battery_path);
    return return_code;
}
