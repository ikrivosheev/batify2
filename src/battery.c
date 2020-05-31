#include <glib.h>
#include <errno.h>

#include "battery.h"

const gdouble HOUR = 3600.0;
const guint64 PERCENTAGE = 100;

static gboolean _get_sysattr_string(
    const gchar* path, 
    const gchar* sys_attr, 
    gchar **value,
    GError** error)
{
    gchar *sys_filename;
    gboolean result;

    sys_filename = g_build_filename(path, sys_attr, NULL);
    result = g_file_get_contents(sys_filename, value, NULL, error);
    g_free(sys_filename);

    return result;
}

static gboolean _get_sysattr_int(
    const gchar* path,
    const gchar* sys_attr,
    guint64 *value,
    GError** error)
{
    gchar* s_value;
    gboolean result;

    result = _get_sysattr_string(path, sys_attr, &s_value, error);
    if (result == FALSE)
        return FALSE;

    errno = 0;
    gint64 tmp = g_ascii_strtoull(s_value, NULL, 10);
    g_free(s_value);
    if (errno)
        return FALSE;
    *value = tmp;

    return TRUE;
}

gboolean get_battery_status(const gchar* sys_path, BATTERY_STATUS* status, GError** error)
{
    gboolean result;
    GError* _error = NULL;
    gchar* sys_status;

    result = _get_sysattr_string(sys_path, BATTERY_STATUS_FILENAME, &sys_status, &_error);
    if (result == FALSE)
    {
        g_propagate_error(error, _error);
        return FALSE;
    }
    
    if (g_str_has_prefix(sys_status, "Charging") == TRUE)
        *status = CHARGING_STATUS;
    else if (g_str_has_prefix(sys_status, "Discharging") == TRUE)
        *status = DISCHARGING_STATUS;
    else if (g_str_has_prefix(sys_status, "Not charging") == TRUE)
        *status = NOT_CHARGING_STATUS;
    else if (g_str_has_prefix(sys_status, "Full") == TRUE)
        *status = CHARGED_STATUS;
    else
        *status = UNKNOWN_STATUS;
    
    g_free(sys_status);
    return TRUE;
}

gboolean get_battery_capacity(const gchar* sys_path, guint64* capacity, GError** error)
{
    gboolean result;
    GError* _error = NULL;
    guint64 now, full;

    result = _get_sysattr_int(sys_path, BATTERY_CAPACITY_FILENAME, capacity, NULL);
    if (result == TRUE)
    {
        return TRUE;
    }

    result = _get_sysattr_int(sys_path, BATTERY_CHARGE_NOW_FILENAME, &now, &_error);
    if (result == FALSE)
    {
        g_propagate_error(error, _error);
        return FALSE;
    }

    result = _get_sysattr_int(sys_path, BATTERY_CHARGE_FULL_FILENAME, &full, &_error);
    if (result == FALSE)
    {
        g_propagate_error(error, _error);
        return FALSE;
    }
    
    if (full == 0)
        return FALSE;

    *capacity = (guint64)(((float)now / full) * PERCENTAGE);
    return TRUE;
}

gboolean get_battery_time(const gchar* sys_path, BATTERY_STATUS status, guint64* seconds, GError** error)
{
    gboolean result;
    GError* _error;
    guint64 charge_now, charge_full, current_now;
    
    result = _get_sysattr_int(sys_path, BATTERY_CHARGE_NOW_FILENAME, &charge_now, &_error);
    if (result == FALSE)
    {
        g_propagate_error(error, _error);
        return FALSE;
    }
    
    result = _get_sysattr_int(sys_path, BATTERY_CHARGE_FULL_FILENAME, &charge_full, &_error);
    if (result == FALSE)
    {
        g_propagate_error(error, _error);
        return FALSE;
    }

    result = _get_sysattr_int(sys_path, BATTERY_CURRENT_NOW_FILENAME, &current_now, &_error);
    if (result == FALSE)
    {
        g_propagate_error(error, _error);
        return FALSE;
    }
    
    if (current_now <= 0)
        return FALSE;

    switch (status)
    {
        case DISCHARGING_STATUS:
        case NOT_CHARGING_STATUS:
            *seconds = (guint64)(HOUR * charge_now / current_now);
            break;
        case CHARGING_STATUS:
        case CHARGED_STATUS:
            *seconds = (guint64)(HOUR * (charge_full - charge_now) / current_now);
            break;
        default:
            return FALSE;
    }
    return TRUE;
}
