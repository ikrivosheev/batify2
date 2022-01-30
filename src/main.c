#include <glib.h>
#include <glib/gprintf.h>
#include <libintl.h>
#include <libnotify/notify.h>
#include <locale.h>
#include <stdio.h>

#include "battery.h"

#define PROGRAM_NAME "batify"
#define DEFAULT_INTERVAL 5
#define DEFAULT_LOW_LEVEL 20
#define DEFAULT_CRITICAL_LEVEL 10
#define DEFAULT_FULL_CAPACITY 98
#define DEFAULT_DEBUG FALSE

#define LOG_WARNING_AND_RETURN(val, error, prefix, ...)                                            \
    {                                                                                              \
        if (error != NULL) {                                                                       \
            g_warning(prefix ": %s", ##__VA_ARGS__, error->message);                               \
            g_error_free(error);                                                                   \
        } else                                                                                     \
            g_warning(prefix);                                                                     \
        return val;                                                                                \
    }

GMainLoop* loop;

typedef enum
{
    LOW_LEVEL,
    CRITICAL_LEVEL,
} BATTERY_LEVEL;

static struct config
{
    gint interval;
    gint low_level;
    gint critical_level;
    gint full_capacity;
    gboolean debug;
} config = {
    DEFAULT_INTERVAL,      DEFAULT_LOW_LEVEL, DEFAULT_CRITICAL_LEVEL,
    DEFAULT_FULL_CAPACITY, DEFAULT_DEBUG,
};

struct _Context
{
    Battery* battery;
    BATTERY_STATUS prev_status;
    gboolean low_level_notified;
    gboolean critical_level_notified;
    NotifyNotification* notification;
};

typedef struct _Context Context;

Context*
context_init(Battery* battery)
{
    Context* context = g_new(Context, 1);
    context->battery = battery;
    context->prev_status = 0;
    context->low_level_notified = FALSE;
    context->critical_level_notified = FALSE;
    context->notification = notify_notification_new(NULL, NULL, NULL);
    return context;
}

void
context_free(Context* context)
{
    battery_free(context->battery);
    g_free(context);
}

static GOptionEntry option_entries[] = {
    { "debug", 'd', 0, G_OPTION_ARG_NONE, &config.debug, "Enable/disable debug information", NULL },
    { "interval", 'i', 0, G_OPTION_ARG_INT, &config.interval, "Update interval in seconds", NULL },
    { "low-level",
      'l',
      0,
      G_OPTION_ARG_INT,
      &config.low_level,
      "Low battery level in percent",
      NULL },
    { "critical-level",
      'c',
      0,
      G_OPTION_ARG_INT,
      &config.critical_level,
      "Critical battery level in percent",
      NULL },
    { "full-capacity", 'f', 0, G_OPTION_ARG_INT, &config, "Full capacity for battery", NULL },
    { NULL }
};

static void
notify_message(NotifyNotification* notification,
               const gchar* summary,
               const gchar* body,
               NotifyUrgency urgency,
               gint percent,
               gint timeout)
{
    notify_notification_update(notification, summary, body, NULL);
    notify_notification_set_timeout(notification, timeout);
    notify_notification_set_urgency(notification, urgency);
    if (percent >= 0) {
        GVariant* g_percent = g_variant_new_int32(percent);
        notify_notification_set_hint(notification, "value", g_percent);
    } else {
        notify_notification_set_hint(notification, "value", NULL);
    }

    notify_notification_show(notification, NULL);
}

static gchar*
get_battery_status_summery_string(const Battery* battery, const BATTERY_STATUS status)
{
    static gchar string[255];
    switch (status) {
        case UNKNOWN_STATUS:
            g_sprintf(string, "%s (%s) is unknown", battery->name, battery->technology);
            break;
        case CHARGING_STATUS:
            g_sprintf(string, "%s (%s) is charging", battery->name, battery->technology);
            break;
        case DISCHARGING_STATUS:
            g_sprintf(string, "%s (%s) is discharging", battery->name, battery->technology);
            break;
        case NOT_CHARGING_STATUS:
            g_sprintf(string, "%s (%s) is not charging", battery->name, battery->technology);
            break;
        case CHARGED_STATUS:
            g_sprintf(string, "%s (%s) is charged", battery->name, battery->technology);
            break;
        default:
            string[0] = '\0';
            break;
    }
    return string;
}

static gchar*
get_battery_level_summery_string(const Battery* battery, const BATTERY_LEVEL level)
{
    static gchar string[255];
    switch (level) {
        case LOW_LEVEL:
            g_sprintf(string, "%s (%s) level is low", battery->name, battery->technology);
            break;
        case CRITICAL_LEVEL:
            g_sprintf(string, "%s (%s) level is critical", battery->name, battery->technology);
            break;
        default:
            string[0] = '\0';
            break;
    }
    return string;
}

static gchar*
get_battery_body_string(const guint64 seconds)
{
    if (seconds == 0) {
        return "";
    }

    static gchar body_string[255];
    guint hours, minutes;
    minutes = seconds / 60;
    hours = minutes / 60;
    minutes = minutes - hours * 60;
    if (hours > 0) {
        g_sprintf(body_string, "%02d:%02d remaining", hours, minutes);
    } else {
        g_sprintf(body_string, "00:%02d remaining", minutes);
    }
    return body_string;
}

static void
battery_status_notification(const Battery* battery,
                            const BATTERY_STATUS status,
                            const guint64 percent,
                            const guint64 seconds,
                            NotifyNotification* notification)

{
    notify_message(notification,
                   get_battery_status_summery_string(battery, status),
                   get_battery_body_string(seconds),
                   NOTIFY_URGENCY_NORMAL,
                   percent,
                   NOTIFY_EXPIRES_DEFAULT);
}

static void
battery_level_notification(const Battery* battery,
                           const BATTERY_LEVEL level,
                           const guint64 percent,
                           const guint64 seconds,
                           NotifyNotification* notification)
{
    NotifyUrgency urgency;
    switch (level) {
        case LOW_LEVEL:
            urgency = NOTIFY_URGENCY_NORMAL;
            break;
        case CRITICAL_LEVEL:
            urgency = NOTIFY_URGENCY_CRITICAL;
            break;
    }

    notify_message(notification,
                   get_battery_level_summery_string(battery, level),
                   get_battery_body_string(seconds),
                   urgency,
                   percent,
                   NOTIFY_EXPIRES_DEFAULT);
}

static gboolean
battery_handler(Context* context)
{
    guint64 capacity;
    guint64 seconds;
    BATTERY_STATUS status;
    GError* error = NULL;
    const Battery* battery = context->battery;

    g_debug("Get battery(%s) status", battery->name);
    if (get_battery_status(battery, &status, &error) == FALSE)
        LOG_WARNING_AND_RETURN(
          G_SOURCE_CONTINUE, error, "Cannot get battery(%s) status", battery->name);

    switch (status) {
        case UNKNOWN_STATUS:
            g_debug("Got UNKNOWN_STATUS");
            context->low_level_notified = FALSE;
            context->critical_level_notified = FALSE;

            if (context->prev_status == status)
                break;

            g_debug("Get battery capacity");
            if (get_battery_capacity(battery, &capacity, &error) == FALSE)
                LOG_WARNING_AND_RETURN(
                  G_SOURCE_CONTINUE, error, "Cannot get battery(%s) capacty", battery->name);

            if (capacity >= config.full_capacity) {
                g_debug("Battery(%s) capacity is greater then full capacity: %d",
                        battery->name,
                        config.full_capacity);
                battery_status_notification(
                  battery, CHARGED_STATUS, capacity, 0, context->notification);
            }
            break;
        case CHARGED_STATUS:
            g_debug("Battery(%s) got CHARGED_STATUS", battery->name);
            context->low_level_notified = FALSE;
            context->critical_level_notified = FALSE;
            if (context->prev_status != status)
                battery_status_notification(battery, status, 100, 0, context->notification);
            break;
        case CHARGING_STATUS:
            g_debug("Battery(%s) got CHARGING_STATUS", battery->name);
            context->low_level_notified = FALSE;
            context->critical_level_notified = FALSE;
            if (context->prev_status == status)
                break;

            g_debug("Get battery(%s) capacity", battery->name);
            if (get_battery_capacity(battery, &capacity, &error) == FALSE)
                LOG_WARNING_AND_RETURN(
                  G_SOURCE_CONTINUE, error, "Cannot get battery(%s) capacty", battery->name);

            g_debug("Get battery(%s) time", battery->name);
            if (get_battery_time(battery, status, &seconds, &error) == FALSE) {
                g_warning("Cannot get battery(%s) time", battery->name);
                seconds = 0;
            }
            battery_status_notification(battery, status, capacity, seconds, context->notification);

            break;
        case DISCHARGING_STATUS:
        case NOT_CHARGING_STATUS:
            g_debug("Battery(%s) got NOT_CHARGING_STATUS or DISCHARGING_STATUS", battery->name);

            g_debug("Get battery(%s) capacity", battery->name);
            if (get_battery_capacity(battery, &capacity, &error) == FALSE)
                LOG_WARNING_AND_RETURN(
                  G_SOURCE_CONTINUE, error, "Cannot get battery(%s) capacity", battery->name);

            g_debug("Get battery time");
            if (get_battery_time(battery, status, &seconds, &error) == FALSE) {
                g_warning("Cannot get battery(%s) time", battery->name);
                seconds = 0;
            }

            if (context->prev_status != status) {
                battery_status_notification(
                  battery, status, capacity, seconds, context->notification);
            }
            if ((context->critical_level_notified == FALSE) &&
                (capacity <= config.critical_level)) {
                context->low_level_notified = FALSE;
                context->critical_level_notified = TRUE;
                battery_level_notification(
                  battery, CRITICAL_LEVEL, capacity, seconds, context->notification);
            }
            if ((context->low_level_notified == FALSE) && (capacity > config.critical_level) &&
                (capacity <= config.low_level)) {
                context->low_level_notified = TRUE;
                context->critical_level_notified = FALSE;
                battery_level_notification(
                  battery, LOW_LEVEL, capacity, seconds, context->notification);
            }
            break;
    }
    context->prev_status = status;
    return G_SOURCE_CONTINUE;
}

static gint
battery_compare_by_serial_number(const Battery* battery, const gchar* serial_number)
{
    return g_strcmp0(battery->serial_number, serial_number);
}

static guint
add_watcher(Battery* battery)
{
    Context* context = context_init(battery);
    guint tag = g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                                           config.interval,
                                           (GSourceFunc)battery_handler,
                                           (gpointer)context,
                                           (GDestroyNotify)context_free);
    g_info("Add new battery handler for: %s", battery->name);
    return tag;
}

static gboolean
batteries_supply_handler(GHashTable* watchers)
{
    guint* ptag;
    gchar* key;
    gboolean result;
    Battery* battery;
    GHashTableIter w_iter;
    GError* error = NULL;
    GSList *batteries = NULL, *b_iter;

    g_info("Get batteries supply");
    result = get_batteries_supply(&batteries, &error);
    if (result == FALSE) {
        g_main_loop_quit(loop);
        LOG_WARNING_AND_RETURN(G_SOURCE_REMOVE, error, "Cannot get batteries supply");
    }

    g_info("Create watchers");
    b_iter = batteries;
    while (b_iter != NULL) {
        battery = battery_copy((Battery*)b_iter->data);
        ptag = (guint*)g_hash_table_lookup(watchers, battery->serial_number);
        if (ptag == NULL) {
            ptag = g_new(guint, 1);
            *ptag = add_watcher(battery);
            key = g_strdup(battery->serial_number);
            g_hash_table_insert(watchers, (gpointer)key, (gpointer)ptag);
        }
        b_iter = g_slist_next(b_iter);
    }

    g_info("Remove old watchers");
    g_hash_table_iter_init(&w_iter, watchers);
    while (g_hash_table_iter_next(&w_iter, (gpointer)&key, (gpointer)&ptag)) {
        g_debug("Check battery with serial-number: %s", key);
        b_iter = g_slist_find_custom(
          batteries, (gconstpointer)key, (GCompareFunc)battery_compare_by_serial_number);

        if (b_iter == NULL) {
            g_debug("Remove battery with serial-number: %s", key);
            g_hash_table_iter_remove(&w_iter);
            g_source_remove(*ptag);
        }
    }

    g_slist_free_full(batteries, (GDestroyNotify)battery_free);
    return G_SOURCE_CONTINUE;
}

static gboolean
options_init(int argc, char* argv[])
{
    GError* error = NULL;
    GOptionContext* option_context;

    option_context = g_option_context_new(NULL);
    g_option_context_add_main_entries(option_context, option_entries, PROGRAM_NAME);

    if (g_option_context_parse(option_context, &argc, &argv, &error) == FALSE)
        LOG_WARNING_AND_RETURN(FALSE, error, "Cannot parse command line arguments");

    g_option_context_free(option_context);

    if (config.debug == TRUE)
        g_log_set_handler(
          G_LOG_DOMAIN, G_LOG_LEVEL_MASK | G_LOG_LEVEL_DEBUG, g_log_default_handler, NULL);
    else
        g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK, g_log_default_handler, NULL);

    if (config.low_level < 0 || config.low_level > 100) {
        g_warning("Invalid low level! Low level should be greater then 0, less then 100");
        return FALSE;
    }
    if (config.critical_level < 0 || config.critical_level > 100) {
        g_warning("Invalid critical level! Critical level should be greater then 0, less then 100");
        return FALSE;
    }
    if (config.full_capacity < 0 || config.full_capacity > 100) {
        g_warning("Invalid full capacity! Full capacity should be greater then 0, less then 100");
        return FALSE;
    }

    if (config.low_level < config.critical_level) {
        g_warning("Low level should be less then critical level");
        return FALSE;
    }
    if (config.full_capacity < config.critical_level) {
        g_warning("Full capacity should be greater then critical level");
        return FALSE;
    }

    return TRUE;
}

int
main(int argc, char* argv[])
{
    GHashTable* watchers;

    setlocale(LC_ALL, "");
    g_return_val_if_fail(options_init(argc, argv), 1);
    g_info("Options have been initialized");

    g_return_val_if_fail(notify_init(PROGRAM_NAME), 1);
    g_info("Notify has been initialized");

    watchers = g_hash_table_new_full((GHashFunc)g_str_hash,
                                     (GEqualFunc)g_str_equal,
                                     (GDestroyNotify)g_free,
                                     (GDestroyNotify)g_free);

    loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add_seconds(
      DEFAULT_INTERVAL, (GSourceFunc)batteries_supply_handler, (gpointer)watchers);

    g_info("Run loop");
    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    notify_uninit();
    g_hash_table_destroy(watchers);

    return 0;
}
