#ifndef BATTERY_H
#define BATTERY_H

typedef enum 
{
    UNKNOWN_STATUS, 
    DISCHARGING_STATUS, 
    NOT_CHARGING_STATUS, 
    CHARGING_STATUS, 
    CHARGED_STATUS
} BATTERY_STATUS;

gboolean get_battery_status(const gchar* sys_path, BATTERY_STATUS* status);
gboolean get_battery_capacity(const gchar* sys_path, guint64* capacity);
gboolean get_battery_time(const gchar* sys_path, gboolean remaining, guint64* time);

#endif // BATTERY_H
