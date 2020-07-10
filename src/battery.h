#ifndef BATTERY_H
#define BATTERY_H

#define SYSFS_BATTERY_PREFIX "BAT"
#define SYSFS_BASE_PATH "/sys/class/power_supply/"

#define BATTERY_MANUFACTUR_FILENAME "manufacturer"
#define BATTERY_MODEL_NAME_FILENAME "model_name"
#define BATTERY_TECHNOLOGY_FILENAME "technology"
#define BATTERY_STATUS_FILENAME "status"
#define BATTERY_CAPACITY_FILENAME "capacity"
#define BATTERY_CHARGE_NOW_FILENAME "charge_now"
#define BATTERY_CHARGE_FULL_FILENAME "charge_full"
#define BATTERY_CURRENT_NOW_FILENAME "current_now"

#define BATTERY_ERROR battery_error_quark()
GQuark       g_io_error_quark      (void);

#define BATTERY_CHARGE_NOW_ERROR 1000
#define BATTERY_CHARGE_FULL_ERROR 1001
#define BATTERY_CURRENT_NOW_ERROR 1002
#define BATTERY_INVALID_STATUS 1003
#define BATTERY_BATTERIES_SUPPLIES 1004

typedef enum 
{
    UNKNOWN_STATUS, 
    DISCHARGING_STATUS, 
    NOT_CHARGING_STATUS, 
    CHARGING_STATUS, 
    CHARGED_STATUS,
} BATTERY_STATUS;

struct _Battery {
    gchar* name;
    gchar* sys_path;
    gchar* model_name;
    gchar* manufacture;
    gchar* technology;
};
typedef struct _Battery Battery;

gboolean battery_init(Battery* battery, gchar* name, GError** error);
void battery_free(Battery* battery);
gboolean get_battery_status(const Battery* battery, BATTERY_STATUS* status, GError** error);
gboolean get_battery_capacity(const Battery* battery, guint64* capacity, GError** error);
gboolean get_battery_time(const Battery* battery, BATTERY_STATUS status, guint64* time, GError** error);

gboolean get_batteries_supplies(GSList** list, GError** error);

#endif // BATTERY_H
