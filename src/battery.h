#ifndef BATTERY_H
#define BATTERY_H

#define BATTERY_STATUS_FILENAME "status"
#define BATTERY_CAPACITY_FILENAME "capacity"
#define BATTERY_CHARGE_NOW_FILENAME "charge_now"
#define BATTERY_CHARGE_FULL_FILENAME "charge_full"
#define BATTERY_CURRENT_NOW_FILENAME "current_now"

typedef enum 
{
    UNKNOWN_STATUS, 
    DISCHARGING_STATUS, 
    NOT_CHARGING_STATUS, 
    CHARGING_STATUS, 
    CHARGED_STATUS,
} BATTERY_STATUS;

gboolean get_battery_status(const gchar* sys_path, BATTERY_STATUS* status, GError** error);
gboolean get_battery_capacity(const gchar* sys_path, guint64* capacity, GError** error);
gboolean get_battery_time(const gchar* sys_path, BATTERY_STATUS status, guint64* time, GError** error);

#endif // BATTERY_H
