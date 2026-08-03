//
// Created by xav on 8/1/26.
//

#ifndef MOSQUETTEERSHARED_H
#define MOSQUETTEERSHARED_H

#include <ArduinoJson.h>
#include <memory>
#include <MosquetteerClasses.h>
#include <mqtt_client.h>

// #define MQTTO_ENABLE_LOGGING

#ifdef MQTTO_ENABLE_LOGGING
#define MQTTO_PRINT_HEADER Serial.print("[MOSQUETTEER] ")
#define MQTTO_LOGNW(str) Serial.print('\n')
#define MQTTO_LOG(str) MQTTO_PRINT_HEADER; Serial.print(str)
#define MQTTO_LOGN(str) MQTTO_PRINT_HEADER; Serial.println(str)
#define MQTTO_LOGF(str, p...) MQTTO_PRINT_HEADER; Serial.printf(str, p)
#define MQTTO_LOGFN(str, p...) MQTTO_PRINT_HEADER; Serial.printf(str, p); Serial.println()
#else
#define MQTTO_PRINT_HEADER
#define MQTTO_LOGNW(str)
#define MQTTO_LOG(str)
#define MQTTO_LOGN(str)
#define MQTTO_LOGF(str, p...)
#define MQTTO_LOGFN(str, p...)
#endif

#ifndef MQ_HA_DISCOVERY_PREFIX
#define MQ_HA_DISCOVERY_PREFIX "homeassistant/device"
#endif

#ifndef MQ_HA_DISCOVERY_SUFFIX
#define MQ_HA_DISCOVERY_SUFFIX "config"
#endif


#ifndef MQ_HA_DEVICES_PREFIX
#define MQ_HA_DEVICES_PREFIX "devices"
#endif

#ifndef MQ_HA_AVAILABILITY_SUFFIX
#define MQ_HA_AVAILABILITY_SUFFIX "availability"
#endif

#ifndef MQ_HA_STATE_SUFFIX
#define MQ_HA_STATE_SUFFIX "state"
#endif

#ifndef MQ_HA_SET_SUFFIX
#define MQ_HA_SET_SUFFIX "set"
#endif

#ifndef MQ_HA_PAYLOAD_AVAILABLE
#define MQ_HA_PAYLOAD_AVAILABLE "on"
#endif

#ifndef MQ_HA_PAYLOAD_NOT_AVAILABLE
#define MQ_HA_PAYLOAD_NOT_AVAILABLE "off"
#endif

#ifndef MQ_UNSET_VAL
#define MQ_UNSET_VAL NAN
#endif

#ifndef MQ_AVAILABILITY_TIMER_VAL
#define MQ_AVAILABILITY_TIMER_VAL (1500 * 1000)
#endif

#ifndef MQ_DISCOVERY_TIMEOUT
#define MQ_DISCOVERY_TIMEOUT (10000 * 1000)
#endif

#define MQTTO_NAME "Mosquetteer Client"
#define MQTTO_VER "0.0.1"
#define MQTTO_URL "https://github.com/"

#define MQ_HA_COMMAND_TEMPLATE R"({"id":"%s","val":"{{ value }}"})"
#define MQ_HA_COMMAND_TEMPLATE_SEARCH R"({"id":")"
#define MQ_UNSET_VAL_VALID(v) std::isnan(v)

class MosquetteerProp
{
public:
    explicit MosquetteerProp(const char* data, const bool hasChanged) : hasChanged(hasChanged), data(data)
    {
    }

    int toInt() const
    {
        return data != nullptr ? atoi(data) : 0;
    }

    long toLong() const
    {
        return data != nullptr ? atol(data) : 0l;
    }

    long long toLongLong() const
    {
        return data != nullptr ? atoll(data) : 0ll;
    }

    double toDouble() const
    {
        return data != nullptr ? atof(data) : 0.0;
    }

    const char* toString() const
    {
        return this->data;
    }

    bool isOnState() const
    {
        return strcasecmp(MQ_HA_PAYLOAD_AVAILABLE, data) == 0;
    }

    /**
     * True if the value itself has changed from the last emitted value.
     */
    const bool hasChanged;

private:
    const char* data;
};

namespace MosquetteerShared
{
    enum DiscoveryState
    {
        DISCOVERY_WAITING,
        DISCOVERY_TIMEOUT,
        DISCOVERY_OK,
    };

    enum Type
    {
        SENSOR = 0,
        BINARY_SENSOR,
        SWITCH,
        BUTTON,
        NUMBER,
        TEXT,
        EVENT,
    };

    inline const char *typeName[] = {
        "sensor",
        "binary_sensor",
        "switch",
        "button",
        "number",
        "text",
        "event"
    };

    inline const char *iconByType[] = {
        "mdi:chart-line",
        "mdi:checkbox-blank-circle-outline",
        "mdi:toggle-switch",
        "mdi:gesture-tap-button",
        "mdi:numeric",
        "mdi:form-textbox",
        "mdi:bell-ring",
    };

    inline bool isTypeInvalid(int t)
    {
        return t < SENSOR || t > EVENT;
    }

    inline const char *getIconByType(Type t)
    {
        if (isTypeInvalid(t))
        {
            return nullptr;
        }

        return iconByType[t];
    }

    inline const char *getTypeName(Type t)
    {
        if (isTypeInvalid(t))
        {
            return nullptr;
        }

        return typeName[t];
    }

    enum Capability : uint16_t
    {
        CAP_NONE           = 0,

        // Has a state_topic
        CAP_STATE          = 1 << 0,

        // Has a command_topic
        CAP_COMMAND        = 1 << 1,

        // Uses payload_on / payload_off
        CAP_PAYLOAD_ON_OFF = 1 << 2,

        // Uses state_on / state_off
        CAP_STATE_ON_OFF   = 1 << 3,

        // Uses payload_press
        CAP_PRESS          = 1 << 4,
    };

    inline constexpr uint16_t capabilities[] = {
        // SENSOR
        CAP_STATE,
        // BINARY_SENSOR
        CAP_STATE | CAP_PAYLOAD_ON_OFF,
        // SWITCH
        CAP_STATE | CAP_COMMAND | CAP_PAYLOAD_ON_OFF | CAP_STATE_ON_OFF,
        // BUTTON
        CAP_COMMAND | CAP_PRESS,
        // NUMBER
        CAP_STATE | CAP_COMMAND,
        // TEXT
        CAP_STATE | CAP_COMMAND,
        // EVENT
        CAP_STATE
    };

    inline bool hasCapability(Type t, Capability cap)
    {
        return (capabilities[t] & cap) != 0;
    }

    enum StateClass
    {
        MEASUREMENT = 0,
        TOTAL,
        TOTAL_INCREASING,
        STATE_CLASS_INVALID,
    };

    inline const char *stateClassNames[] = {
        "measurement",
        "total",
        "total_increasing"
    };

    inline const char* getStateClassName(StateClass sc)
    {
        if (sc < MEASUREMENT || sc >= STATE_CLASS_INVALID)
        {
            return nullptr;
        }
        return stateClassNames[sc];
    }

    enum EntityMode
    {
        MODE_AUTO, // For number only
        MODE_BOX, // For number only
        MODE_SLIDER, // For number only
        MODE_TEXT, // For text only
        MODE_PASSWORD, // For text only
        ENTITY_MODE_INVALID,
    };

    inline const char *modeNames[] = {
        "auto",
        "box",
        "slider",
        "text",
        "password",
        ""
    };

    inline const char* getModeName(EntityMode m)
    {
        if (m < MODE_AUTO || m >= ENTITY_MODE_INVALID)
        {
            return nullptr;
        }
        return modeNames[m];
    }

    inline bool isValid(double val)
    {
        return MQ_UNSET_VAL_VALID(val);
    }

    inline bool isValid(const char *val)
    {
        return val != nullptr && strlen(val) > 0;
    }

    inline bool isValid(EntityMode val)
    {
        return val != ENTITY_MODE_INVALID;
    }

    inline bool isValid(StateClass val)
    {
        return val != STATE_CLASS_INVALID;
    }

    inline bool isValid(MosquetteerClasses::DeviceClass val)
    {
        return val != MosquetteerClasses::DEVICE_CLASS_INVALID;
    }

    struct BaseConfig
    {
        MosquetteerClasses::DeviceClass deviceClass = MosquetteerClasses::DEVICE_CLASS_INVALID;

        void setBaseConfig(JsonObject &doc) const
        {
            if (isValid(deviceClass))
            {
                doc["device_class"] = getClassName(deviceClass);
            }
        }

        virtual void setConfig(JsonObject& doc) const = 0;
        [[nodiscard]] virtual std::unique_ptr<BaseConfig> clone() const = 0;
        virtual ~BaseConfig() = default;
    };

    struct MinMaxConfig
    {
        double min = MQ_UNSET_VAL;
        double max = MQ_UNSET_VAL;

        void setConfig(JsonObject& doc) const
        {
            if (isValid(min))
            {
                doc["min"] = min;
            }

            if (isValid(max))
            {
                doc["max"] = max;
            }
        }
    };

    struct ModeConfig
    {
        EntityMode mode = ENTITY_MODE_INVALID;

        void setConfig(JsonObject& doc) const
        {
            if (isValid(mode))
            {
                doc["mode"] = getModeName(mode);
            }
        }
    };

    struct UnitConfig
    {
        char unit[16]{};

        void setConfig(JsonObject &doc) const
        {
            if (isValid(unit))
            {
                doc["unit_of_measurement"] = unit;
            }
        }
    };

    struct SensorConfig : BaseConfig
    {
        StateClass stateClass = STATE_CLASS_INVALID;
        UnitConfig unit{};

        void setConfig(JsonObject& doc) const override
        {
            setBaseConfig(doc);
            unit.setConfig(doc);

            if (isValid(stateClass))
            {
                doc["state_class"] = getStateClassName(stateClass);
            }
        }

        [[nodiscard]] std::unique_ptr<BaseConfig> clone() const override
        {
            return std::make_unique<SensorConfig>(*this);
        }
    };

    struct BinarySensorConfig : BaseConfig
    {
        double offDelay = MQ_UNSET_VAL;

        void setConfig(JsonObject& doc) const override
        {
            setBaseConfig(doc);

            if (isValid(offDelay))
            {
                doc["off_delay"] = offDelay;
            }
        }

        [[nodiscard]] std::unique_ptr<BaseConfig> clone() const override
        {
            return std::make_unique<BinarySensorConfig>(*this);
        }
    };

    struct SwitchConfig : BaseConfig
    {
        void setConfig(JsonObject& doc) const override
        {
            setBaseConfig(doc);
        }

        [[nodiscard]] std::unique_ptr<BaseConfig> clone() const override
        {
            return std::make_unique<SwitchConfig>(*this);
        }
    };

    struct ButtonConfig : BaseConfig
    {
        void setConfig(JsonObject& doc) const override
        {
            setBaseConfig(doc);
        }

        [[nodiscard]] std::unique_ptr<BaseConfig> clone() const override
        {
            return std::make_unique<ButtonConfig>(*this);
        }
    };

    struct NumberConfig : BaseConfig
    {
        double step = MQ_UNSET_VAL;
        MinMaxConfig minMax;
        UnitConfig unit;
        ModeConfig mode;

        void setConfig(JsonObject& doc) const override
        {
            setBaseConfig(doc);
            minMax.setConfig(doc);
            unit.setConfig(doc);
            mode.setConfig(doc);

            if (isValid(step))
            {
                doc["step"] = step;
            }
        }

        [[nodiscard]] std::unique_ptr<BaseConfig> clone() const override
        {
            return std::make_unique<NumberConfig>(*this);
        }
    };

    struct TextConfig : BaseConfig
    {
        char pattern[64]{};
        MinMaxConfig minMax;
        ModeConfig mode;

        void setConfig(JsonObject& doc) const override
        {
            setBaseConfig(doc);
            minMax.setConfig(doc);
            mode.setConfig(doc);

            if (isValid(pattern))
            {
                doc["pattern"] = pattern;
            }
        }

        [[nodiscard]] std::unique_ptr<BaseConfig> clone() const override
        {
            return std::make_unique<TextConfig>(*this);
        }
    };

    struct EventConfig : BaseConfig
    {
        void setConfig(JsonObject& doc) const override
        {
            setBaseConfig(doc);
        }

        [[nodiscard]] std::unique_ptr<BaseConfig> clone() const override
        {
            return std::make_unique<EventConfig>(*this);
        }
    };

    typedef void (*Callback)(const MosquetteerProp& p);
    inline std::function<void(MosquetteerProp)> OnChangeCallback;

    struct Definition
    {
        std::unique_ptr<char[]> id;
        std::unique_ptr<char[]> name;
        Type type;
        std::unique_ptr<BaseConfig> config;
        Callback cb = nullptr;
        uint32_t lastValueHash = 0;
    };

    inline size_t getTopic(const char *id, const char *deviceId, const char *suffix, char *out)
    {
        size_t len = strlen(id) +
            strlen(suffix) +
            strlen(MQ_HA_DEVICES_PREFIX) +
            (deviceId == nullptr ? 0 : (strlen(deviceId) + 1)) +
            3;
        if (out == nullptr)
        {
            return len;
        }

        if (deviceId == nullptr)
        {
            return snprintf(out, len, "%s/%s/%s", MQ_HA_DEVICES_PREFIX, id, suffix);
        }
        else
        {
            return snprintf(out, len, "%s/%s/%s/%s", MQ_HA_DEVICES_PREFIX, deviceId, id, suffix);
        }
    }

    inline uint32_t fnv1a32(const uint8_t* data, size_t len)
    {
        uint32_t hash = 2166136261u; // FNV offset basis

        for (size_t i = 0; i < len; i++)
        {
            hash ^= data[i];
            hash *= 16777619u; // FNV prime
        }

        return hash;
    }
}

#endif //MOSQUETTEERSHARED_H
