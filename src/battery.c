#include <glib.h>
#include <errno.h>

#include "battery.h"

#define PROPAGATE_ERROR(error, _error) \
    if (_error != NULL) \
    { \
        g_propagate_error(error, _error); \
    }

G_DEFINE_QUARK(battery-error-quark, battery_error)

const gdouble HOUR = 3600.0;
const guint64 PERCENTAGE = 100;

static gboolean _get_sysattr_string_by_path(
    const gchar* battery_name,
    const gchar* sys_path,
    const gchar* sys_attr,
    gchar** value,
    GError** error)
{
    gchar *sys_filename;
    gboolean result;

    sys_filename = g_build_filename(sys_path, sys_attr, NULL);
    g_debug("Get attr: \"%s\" for battery: \"%s\"", sys_attr, battery_name);

    result = g_file_get_contents(sys_filename, value, NULL, error);
    g_free(sys_filename);

    return result;

}

static gboolean _get_sysattr_string(
    const Battery* battery, 
    const gchar* sys_attr, 
    gchar **value,
    GError** error)
{
    return _get_sysattr_string_by_path(battery->name, battery->sys_path, sys_attr, value, error);
}

static gboolean _get_sysattr_int(
    const Battery* battery,
    const gchar* sys_attr,
    guint64 *value,
    GError** error)
{
    gchar* s_value;
    gboolean result;

    if (_get_sysattr_string(battery, sys_attr, &s_value, error) == FALSE)
        return FALSE;

    errno = 0;
    gint64 tmp = g_ascii_strtoull(s_value, NULL, 10);
    if (errno)
    {
        g_set_error(error, BATTERY_ERROR, errno, "Faild convert sys-attr value: \"%s\" to int", s_value);
        result = FALSE;
    }
    else
    {
        *value = tmp;
        result = TRUE;
    }

    g_free(s_value);
    return result;
}

gboolean battery_init(Battery* battery, gchar* name, GError** error)
{
    gboolean result;
    gchar* model_name, *manufacture, *technology, *serial_number;
    gchar* sys_path = g_strjoin("", SYSFS_BASE_PATH, name, NULL);
    gchar* charge_file_path;
    
    result = _get_sysattr_string_by_path(name, sys_path, BATTERY_MANUFACTUR_FILENAME, &manufacture, error);
    g_return_val_if_fail(result, FALSE);

    result = _get_sysattr_string_by_path(name, sys_path, BATTERY_MODEL_NAME_FILENAME, &model_name, error);
    g_return_val_if_fail(result, FALSE);

    result = _get_sysattr_string_by_path(name, sys_path, BATTERY_TECHNOLOGY_FILENAME, &technology, error);
    g_return_val_if_fail(result, FALSE);
    result = _get_sysattr_string_by_path(name, sys_path, BATTERY_SERIAL_NUMBER_FILENAME, &serial_number, error);
    g_return_val_if_fail(result, FALSE);

    charge_file_path = g_build_filename(sys_path, BATTERY_CHARGE_NOW_FILENAME, NULL);
    battery->use_charge = g_file_test(charge_file_path, G_FILE_TEST_EXISTS);
    g_free(charge_file_path);

    battery->sys_path = sys_path;
    battery->name = name;
    battery->model_name = g_strstrip(model_name);
    battery->manufacture = g_strstrip(manufacture);
    battery->technology = g_strstrip(technology);
    battery->serial_number = g_strstrip(serial_number);
    return TRUE;
}

Battery* battery_copy(const Battery* battery)
{
    Battery* new_battery = g_new(Battery, 1);
    new_battery->name = g_strdup(battery->name);
    new_battery->sys_path = g_strdup(battery->sys_path);
    new_battery->model_name = g_strdup(battery->model_name);
    new_battery->manufacture = g_strdup(battery->manufacture);
    new_battery->technology = g_strdup(battery->technology);
    new_battery->serial_number = g_strdup(battery->serial_number);
    new_battery->use_charge = battery->use_charge;
    return new_battery;
}

void battery_free(Battery* battery)
{
    g_free(battery->name);
    g_free(battery->sys_path);
    g_free(battery->model_name);
    g_free(battery->manufacture);
    g_free(battery->technology);
    g_free(battery->serial_number);
}

gboolean get_battery_status(const Battery* battery, BATTERY_STATUS* status, GError** error)
{
    gboolean result;
    GError* _error = NULL;
    gchar* sys_status;

    result = _get_sysattr_string(battery, BATTERY_STATUS_FILENAME, &sys_status, &_error);
    if (result == FALSE)
    {
        PROPAGATE_ERROR(error, _error);
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

static gboolean _get_battery_capacity(
    const Battery* battery, 
    const gchar* now_filename, 
    const gchar* full_filename, 
    guint64* capacity,
    GError** error)
{
    gboolean result;
    guint64 now, full;
    GError* _error;

    result = _get_sysattr_int(battery, now_filename, &now, &_error);
    if (result == FALSE)
    {
        PROPAGATE_ERROR(error, _error);
        return FALSE;
    }

    result = _get_sysattr_int(battery, full_filename, &full, &_error);
    if (result == FALSE)
    {
        PROPAGATE_ERROR(error, _error);
        return FALSE;
    }

    *capacity = (guint64)(((float)now / full) * PERCENTAGE);
    return TRUE;
}

static gboolean _get_battery_capacity_charge(const Battery* battery, guint64* capacity, GError** error)
{
    return _get_battery_capacity(
        battery, 
        BATTERY_CHARGE_NOW_FILENAME, 
        BATTERY_CHARGE_FULL_FILENAME, 
        capacity, 
        error);
}

static gboolean _get_battery_capacity_energy(const Battery* battery, guint64* capacity, GError** error)
{
    return _get_battery_capacity(
        battery, 
        BATTERY_ENERGY_NOW_FILENAME, 
        BATTERY_ENERGY_FULL_FILENAME, 
        capacity, 
        error);
}

gboolean get_battery_capacity(const Battery* battery, guint64* capacity, GError** error)
{
    gboolean result;

    result = _get_sysattr_int(battery, BATTERY_CAPACITY_FILENAME, capacity, NULL);
    if (result == TRUE)
        return TRUE;

    if (battery->use_charge == TRUE)
        result = _get_battery_capacity_charge(battery, capacity, error);
    else
        result = _get_battery_capacity_energy(battery, capacity, error);
    return result;
}

static gboolean _get_battery_time(
    const Battery* battery, 
    const gchar* now_filename,
    const gchar* full_filename,
    const gchar* power_filename,
    BATTERY_STATUS status, 
    guint64* seconds, 
    GError** error)
{
    gboolean result;
    GError* _error = NULL;
    guint64 charge_now, charge_full, current_now;
    
    result = _get_sysattr_int(battery, now_filename, &charge_now, &_error);
    if (result == FALSE)
    {
        PROPAGATE_ERROR(error, _error);
        return FALSE;
    }
    
    result = _get_sysattr_int(battery, full_filename, &charge_full, &_error);
    if (result == FALSE)
    {
        PROPAGATE_ERROR(error, _error);
        return FALSE;
    }

    result = _get_sysattr_int(battery, power_filename, &current_now, &_error);
    if (result == FALSE)
    {
        PROPAGATE_ERROR(error, _error);
        return FALSE;
    }
    
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
            g_set_error(error, BATTERY_ERROR, BATTERY_INVALID_STATUS, "Invalid status for get_battery_time: \"%d\"", status);
            return FALSE;
    }
    return TRUE;
}

static gboolean _get_battery_time_charge(
    const Battery* battery, 
    BATTERY_STATUS status, 
    guint64* seconds, 
    GError** error)
{
    return _get_battery_time(
        battery,
        BATTERY_CHARGE_NOW_FILENAME,
        BATTERY_CHARGE_FULL_FILENAME,
        BATTERY_CURRENT_NOW_FILENAME,
        status,
        seconds,
        error);
}

static gboolean _get_battery_time_energy(
    const Battery* battery,
    BATTERY_STATUS status,
    guint64* seconds,
    GError** error)
{
    return _get_battery_time(
        battery,
        BATTERY_ENERGY_NOW_FILENAME,
        BATTERY_ENERGY_FULL_FILENAME,
        BATTERY_POWER_NOW_FILENAME,
        status,
        seconds,
        error);
}


gboolean get_battery_time(const Battery* battery, BATTERY_STATUS status, guint64* seconds, GError** error)
{
    gboolean result;

    if (battery->use_charge == TRUE)
        result = _get_battery_time_charge(battery, status, seconds, error);
    else
        result = _get_battery_time_energy(battery, status, seconds, error);
    return result;
}

gboolean get_batteries_supply(GSList** list, GError** error)
{
    Battery* battery;
    const gchar* dir_name;
    GDir* dir = g_dir_open(SYSFS_BASE_PATH, 0, error); 
    if (dir == NULL)
        return FALSE;
    
    dir_name = g_dir_read_name(dir);
    while(dir_name != NULL)
    {
        if (g_str_has_prefix(dir_name, SYSFS_BATTERY_PREFIX))
        {
            battery = g_new(Battery, 1);
            if (battery_init(battery, g_strdup(dir_name), error) == FALSE)
            {
                return FALSE;
            }
            (*list) = g_slist_prepend((*list), battery);
        }

        dir_name = g_dir_read_name(dir);
    }
    
    g_dir_close(dir);
    return TRUE;
}
