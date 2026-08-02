//
// Created by xav on 8/1/26.
//

#ifndef MOSQUETTEERCLASSES_H
#define MOSQUETTEERCLASSES_H

namespace MosquetteerClasses
{
    enum DeviceClass
    {
        DEVICE_CLASS_INVALID = 0,

        // Sensor
        APPARENT_POWER,
        AQI,
        ATMOSPHERIC_PRESSURE,
        BATTERY,
        BLOOD_GLUCOSE_CONCENTRATION,
        CARBON_DIOXIDE,
        CARBON_MONOXIDE,
        CURRENT,
        DATA_RATE,
        DATA_SIZE,
        DATE,
        DISTANCE,
        DURATION,
        ENERGY,
        ENERGY_DISTANCE,
        ENUM,
        FREQUENCY,
        GAS,
        HUMIDITY,
        ILLUMINANCE,
        IRRADIANCE,
        MOISTURE,
        MONETARY,
        NITROGEN_DIOXIDE,
        NITROGEN_MONOXIDE,
        NITROUS_OXIDE,
        OZONE,
        PH,
        PM1,
        PM10,
        PM25,
        POWER,
        POWER_FACTOR,
        PRECIPITATION,
        PRECIPITATION_INTENSITY,
        PRESSURE,
        REACTIVE_POWER,
        SIGNAL_STRENGTH,
        SOUND_PRESSURE,
        SPEED,
        SULPHUR_DIOXIDE,
        TEMPERATURE,
        TIMESTAMP,
        VOLATILE_ORGANIC_COMPOUNDS,
        VOLATILE_ORGANIC_COMPOUNDS_PARTS,
        VOLTAGE,
        VOLUME,
        VOLUME_STORAGE,
        WATER,
        WEIGHT,
        WIND_DIRECTION,
        WIND_SPEED,

        // Binary Sensor
        BATTERY_CHARGING,
        COLD,
        CONNECTIVITY,
        DOOR,
        GARAGE_DOOR,
        HEAT,
        LIGHT,
        LOCK,
        MOTION,
        MOVING,
        OCCUPANCY,
        OPENING,
        PLUG,
        PRESENCE,
        PROBLEM,
        RUNNING,
        SAFETY,
        SMOKE,
        SOUND,
        TAMPER,
        UPDATE,
        VIBRATION,
        WINDOW,

        // Switch
        CONFIG,
        SWITCH,

        // Button
        IDENTIFY,
        RESTART,

        // Text
        DATETIME,
        TIME,

        // Event
        DOORBELL,
    };

    inline constexpr const char* classNames[] = {
        "",

        // Sensor
        "apparent_power",
        "aqi",
        "atmospheric_pressure",
        "battery",
        "blood_glucose_concentration",
        "carbon_dioxide",
        "carbon_monoxide",
        "current",
        "data_rate",
        "data_size",
        "date",
        "distance",
        "duration",
        "energy",
        "energy_distance",
        "enum",
        "frequency",
        "gas",
        "humidity",
        "illuminance",
        "irradiance",
        "moisture",
        "monetary",
        "nitrogen_dioxide",
        "nitrogen_monoxide",
        "nitrous_oxide",
        "ozone",
        "ph",
        "pm1",
        "pm10",
        "pm25",
        "power",
        "power_factor",
        "precipitation",
        "precipitation_intensity",
        "pressure",
        "reactive_power",
        "signal_strength",
        "sound_pressure",
        "speed",
        "sulphur_dioxide",
        "temperature",
        "timestamp",
        "volatile_organic_compounds",
        "volatile_organic_compounds_parts",
        "voltage",
        "volume",
        "volume_storage",
        "water",
        "weight",
        "wind_direction",
        "wind_speed",

        // Binary Sensor
        "battery_charging",
        "cold",
        "connectivity",
        "door",
        "garage_door",
        "heat",
        "light",
        "lock",
        "motion",
        "moving",
        "occupancy",
        "opening",
        "plug",
        "presence",
        "problem",
        "running",
        "safety",
        "smoke",
        "sound",
        "tamper",
        "update",
        "vibration",
        "window",

        // Switch
        "config",
        "switch",

        // Button
        "identify",
        "restart",

        // Text
        "datetime",
        "time",

        // Event
        "doorbell",
    };

    inline const char* getClassName(DeviceClass c)
    {
        return classNames[c];
    }
}

#endif //MOSQUETTEERCLASSES_H
