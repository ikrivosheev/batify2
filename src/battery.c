#include <glib.h>
#include <errno.h>

#include "battery.h"

const gdouble HOUR = 3600.0;
const guint64 PERCENTAGE = 100;

static gboolean _get_sysattr_string(
    const gchar* path, 
    const gchar* sys_attr, 
    gchar **value)
{
    gchar *sys_filename;
    gboolean sys_status;

    sys_filename = g_build_filename(path, sys_attr, NULL);
    sys_status = g_file_get_contents(sys_filename, value, NULL, NULL);
    g_free(sys_filename);

    return sys_status;
}

static gboolean _get_sysattr_int(
    const gchar* path,
    const gchar* sys_attr,
    guint64 *value)
{
    gchar* s_value;
    gboolean sys_status;

    sys_status = _get_sysattr_string(path, sys_attr, &s_value);
    if (sys_status == TRUE) 
    {
        gint64 tmp = g_ascii_strtoull(s_value, NULL, 10);
        if (tmp != NULL)
            *value = tmp;
        else
            sys_status = FALSE;
        
        g_free(s_value);
    }

    return sys_status;
}

gboolean get_battery_status(const gchar* sys_path, BATTERY_STATUS* status)
{
    gboolean result;
    gchar* sys_status;

    result = _get_sysattr_string(sys_path, "status", &sys_status);
    
    if (result == TRUE)
    {
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
    }
    return result;
}

gboolean get_battery_capacity(const gchar* sys_path, guint64* capacity)
{
    gboolean result;
    guint64 now, full;

    result = _get_sysattr_int(sys_path, "capacity", capacity);
    if (result == TRUE)
        return TRUE;

    result = _get_sysattr_int(sys_path, "charge_now", &now);
    if (result == FALSE)
        return FALSE;

    result = _get_sysattr_int(sys_path, "charge_full", &full);
    if (result == FALSE)
        return FALSE;
    
    *capacity = (guint64)(((float)now / full) * PERCENTAGE);
    return TRUE;
}

gboolean get_battery_time(const gchar* sys_path, gboolean remaining, guint64* seconds)
{
    gboolean result;
    guint64 charge_now, charge_full, current_now;
    
    result = _get_sysattr_int(sys_path, "charge_now", &charge_now);
    if (result == FALSE)
        return FALSE;
    
    result = _get_sysattr_int(sys_path, "charge_full", &charge_full);
    if (result == FALSE)
        return FALSE;

    result = _get_sysattr_int(sys_path, "current_now", &current_now);

    if (result == FALSE)
        return FALSE;
    
    if (remaining == TRUE)
        *seconds = (guint64)(HOUR * charge_now / current_now);
    else
        *seconds = (guint64)(HOUR * (charge_full - charge_now) / current_now);
    return TRUE;
}
