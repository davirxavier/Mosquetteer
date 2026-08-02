//
// Created by xav on 8/1/26.
//

#ifndef MOSQUETTEER_H
#define MOSQUETTEER_H

#include <MosquetteerShared.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "mqtt_client.h"
#include "esp_timer.h"

#ifndef MQ_CLIENT_BUFSIZE
#define MQ_CLIENT_BUFSIZE 2048
#endif

#ifndef MQ_CLIENT_INITIAL_DEFINITION_CAPACITY
#define MQ_CLIENT_INITIAL_DEFINITION_CAPACITY 10
#endif

class Mosquetteer
{
public:
    ~Mosquetteer()
    {
        stop();
        unalloc(&mqttUri);
        unalloc(&username);
        unalloc(&password);

        unalloc(&deviceId);
        unalloc(&deviceFirmwareVersion);
        unalloc(&deviceName);
        unalloc(&deviceModel);
        unalloc(&deviceManufacturer);

        definitions.clear();
        definitions.shrink_to_fit();
    }

    void begin()
    {
        if (started)
        {
            MQTTO_LOGN("Begin called but client already started.");
            return;
        }

        MQTTO_LOGN("Starting client.");
        static esp_mqtt_client_config_t mqtt_cfg = {
            .broker = {
                .address = {
                    .uri = mqttUri
                }
            },
            .credentials = {
                .client_id = clientId,
            },
            .session = {
                .last_will = {
                    .topic = availabilityTopic,
                    .msg = MQ_HA_PAYLOAD_NOT_AVAILABLE,
                    .qos = 1,
                    .retain = true,
                },
                .keepalive = 40,
            },
            .buffer = {
                .size = MQ_CLIENT_BUFSIZE,
                .out_size = MQ_CLIENT_BUFSIZE,
            },
        };

        if (hasText(username) && hasText(password))
        {
            mqtt_cfg.credentials.username = username;
            mqtt_cfg.credentials.authentication.password = password;
            MQTTO_LOGFN("Username: %s", username);
        }

        if (cacert != nullptr)
        {
            mqtt_cfg.broker.verification.certificate = cacert;
        }

        handle = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(handle, MQTT_EVENT_ANY, handleEventTrampoline, this);
        esp_mqtt_client_start(handle);
        started = true;
    }

    void stop()
    {
        if (!started)
        {
            return;
        }

        stopTimer(&discoveryTimer, DISCOVERY);
        stopTimer(&availabilityTimer, AVAILABILITY);
        esp_mqtt_client_disconnect(handle);
        esp_mqtt_client_destroy(handle);
        started = false;
    }

    void setTlsConfig(const char *cacert)
    {
        this->cacert = cacert;
    }

    /**
     * @param uri URI for the mqtt server. Use the mqtts:// protocol if loading the server cacert. Example: mqtt://192.168.1.100:1883 or mqtts://192.168.1.100:8883
     * @param username Username to authenticate in the serve. Optional.
     * @param password Password to authenticate in the serve. Optional.
     */
    void setClientConfig(const char* uri, const char* username = nullptr, const char* password = nullptr)
    {
        allocAndSet(&mqttUri, uri);
        allocAndSet(&this->username, username);
        allocAndSet(&this->password, password);
    }

    /**
     * @param deviceId Home assistant ID for this device. Example: smart_outlet
     * @param deviceName Pretty name to show in home assistant. Example: Smart Outlet
     * @param firmwareVersion Firmware version for this device. Optional.
     * @param deviceModel Device model. Optional.
     * @param deviceManufacturer Device manufacturer. Optional.
     */
    void setDeviceInfo(const char* deviceId,
                       const char* deviceName,
                       const char* firmwareVersion = "1.0.0",
                       const char* deviceModel = "ESP32",
                       const char* deviceManufacturer = "DIY")
    {
        allocAndSet(&this->deviceId, deviceId);
        allocAndSet(&this->deviceName, deviceName);
        allocAndSet(&this->deviceFirmwareVersion, firmwareVersion);
        allocAndSet(&this->deviceModel, deviceModel);
        allocAndSet(&this->deviceManufacturer, deviceManufacturer);

        if (hasText(deviceId))
        {
            String mac = WiFi.macAddress();
            mac.replace(':', '_');

            size_t clientIdLen = strlen(deviceId) + mac.length() + 2;
            char clientId[clientIdLen]{};
            snprintf(clientId, clientIdLen, "%s_%s", deviceId, mac.c_str());
            MQTTO_LOGFN("Client id: %s", clientId);
            allocAndSet(&this->clientId, clientId);

            char lastWillTopic[MosquetteerShared::getTopic(deviceId, nullptr, MQ_HA_AVAILABILITY_SUFFIX, nullptr)]{};
            MosquetteerShared::getTopic(deviceId, nullptr, MQ_HA_AVAILABILITY_SUFFIX, lastWillTopic);
            MQTTO_LOGFN("Last will topic: %s", lastWillTopic);
            allocAndSet(&this->availabilityTopic, lastWillTopic);
        }
    }

    /**
     * Defines an entity for this device. Definition is asynchronous.
     * @param id Home assistant entity id. Example: outlet_switch
     * @param name Pretty name to be shown on home assistant. Example: Outlet Switch
     * @param type Entity type. Example: MosquetteerShared::SWITCH
     * @param extraConfig Extra settings for the specified type, see inheritors of MosquetteerShared::BaseConfig for all properties.
     */
    void define(const char *id, const char *name, MosquetteerShared::Type type, const MosquetteerShared::BaseConfig *extraConfig = nullptr)
    {
        if (!hasText(id) || started)
        {
            return;
        }

        if (definitions.empty())
        {
            definitions.reserve(MQ_CLIENT_INITIAL_DEFINITION_CAPACITY);
        }

        size_t idlen = strlen(id) + 1;
        auto idStr = std::make_unique<char[]>(idlen);
        snprintf(idStr.get(), idlen, "%s", id);

        size_t nameLen = strlen(name) + 1;
        auto nameStr = std::make_unique<char[]>(nameLen);
        snprintf(nameStr.get(), nameLen, "%s", name);

        MosquetteerShared::Definition def {
            .id = (std::move(idStr)),
            .name = (std::move(nameStr)),
            .type = type,
            .cb = nullptr,
        };

        if (extraConfig != nullptr)
        {
            def.config = extraConfig->clone();
        }

        definitions.push_back(std::move(def));
        MQTTO_LOGFN("Added object of type %d, with id %s.", type, id);
    }

    /**
     * Registers callback for home assistant commands for a specified entity.
     * @param id Entity id.
     * @param cb Callback definition.
     */
    void onCommand(const char *id, const MosquetteerShared::Callback cb)
    {
        for (auto &def : definitions)
        {
            if (strcasecmp(def.id.get(), id) != 0)
            {
                continue;
            }

            def.cb = cb;
        }
    }

    /**
     * Sends a state update to home assistant for a specified entity.
     * @param id Entity id.
     * @param val Value to be sent.
     * @return
     */
    int sendState(const char *id, const char *val)
    {
        for (auto &def : definitions)
        {
            if (strcasecmp(def.id.get(), id) != 0 || !MosquetteerShared::hasCapability(def.type, MosquetteerShared::CAP_STATE))
            {
                continue;
            }

            char topic[MosquetteerShared::getTopic(def.id.get(), deviceId, MQ_HA_STATE_SUFFIX, nullptr)]{};
            MosquetteerShared::getTopic(def.id.get(), deviceId, MQ_HA_STATE_SUFFIX, topic);
            return esp_mqtt_client_enqueue(handle, topic, val, strlen(val), 1, true, true);
        }
        return -3;
    }

    /**
     * Sends a state update to home assistant for a specified entity.
     * @param id Entity id.
     * @param val Value to be sent.
     * @return
     */
    int sendState(const char *id, bool val)
    {
        char buf[16]{};
        snprintf(buf, sizeof(buf), "%s", val ? MQ_HA_PAYLOAD_AVAILABLE : MQ_HA_PAYLOAD_NOT_AVAILABLE);
        return sendState(id, buf);
    }

    /**
     * Sends a state update to home assistant for a specified entity.
     * @param id Entity id.
     * @param val Value to be sent.
     * @return
     */
    int sendState(const char *id, char val)
    {
        char buf[2]{};
        snprintf(buf, sizeof(buf), "%c", val);
        return sendState(id, buf);
    }

    /**
     * Sends a state update to home assistant for a specified entity.
     * @param id Entity id.
     * @param val Value to be sent.
     * @return
     */
    int sendState(const char *id, const std::string &val)
    {
        return sendState(id, val.c_str());
    }

    /**
     * Sends a state update to home assistant for a specified entity.
     * @param id Entity id.
     * @param val Value to be sent.
     * @return
     */
    int sendState(const char *id, const String &val)
    {
        return sendState(id, val.c_str());
    }

    /**
     * Sends a state update to home assistant for a specified entity.
     * @param id Entity id.
     * @param val Value to be sent.
     * @return
     */
    template <typename T>
    requires std::is_integral_v<T>
    int sendState(const char *id, const T &val)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(val));
        return sendState(id, buf);
    }

    /**
     * Sends a state update to home assistant for a specified entity.
     * @param id Entity id.
     * @param val Value to be sent.
     * @return
     */
    template <typename T>
    requires std::is_floating_point_v<T>
    int sendState(const char *id, const T &val)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%g", static_cast<double>(val));
        return sendState(id, buf);
    }

private:
    size_t getDeviceTopic(char *out) const
    {
        size_t topicLen = strlen(MQ_HA_DISCOVERY_PREFIX) + strlen(deviceId) + strlen(MQ_HA_DISCOVERY_SUFFIX) + 4;
        if (out == nullptr)
        {
            return topicLen;
        }
        return snprintf(out, topicLen, "%s/%s/%s", MQ_HA_DISCOVERY_PREFIX, deviceId, MQ_HA_DISCOVERY_SUFFIX);
    }

    [[nodiscard]] bool isSetup() const
    {
        return hasText(mqttUri) && hasText(deviceId) && hasText(deviceName) && hasText(clientId) && hasText(availabilityTopic);
    }

    static void handleEventTrampoline(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
    {
        auto* instance = static_cast<Mosquetteer*>(handler_args);
        instance->handleEvent(base, event_id, event_data);
    }

    void handleEvent(esp_event_base_t base, int32_t event_id, void* event_data)
    {
        MQTTO_LOGFN("Event dispatched from event loop base=%s, event_id=%d" PRIi32, base, event_id);
        auto event = static_cast<esp_mqtt_event_handle_t>(event_data);
        esp_mqtt_client_handle_t client = event->client;

        switch ((esp_mqtt_event_id_t)event_id)
        {
        case MQTT_EVENT_CONNECTED:
            MQTTO_LOGN("MQTT_EVENT_CONNECTED");
            startDiscovery();
            break;
        case MQTT_EVENT_DISCONNECTED:
            MQTTO_LOGN("MQTT_EVENT_DISCONNECTED");
            stopTimer(&discoveryTimer, DISCOVERY);
            stopTimer(&availabilityTimer, AVAILABILITY);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            MQTTO_LOGFN("MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            MQTTO_LOGFN("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            MQTTO_LOGFN("MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            {
                MQTTO_LOGN("MQTT_EVENT_DATA");
                MQTTO_LOGF("TOPIC=%.*s\r\n", event->topic_len, event->topic);
                MQTTO_LOGF("DATA=%.*s\r\n", event->data_len, event->data);

                constexpr char templateStart[] = MQ_HA_COMMAND_TEMPLATE_SEARCH;
                constexpr size_t prefixLen = sizeof(templateStart) - 1;

                if (discoveryState == MosquetteerShared::DISCOVERY_WAITING)
                {
                    size_t topicLen = getDeviceTopic(nullptr);
                    char topicName[topicLen]{};
                    getDeviceTopic(topicName);

                    if (strcasecmp(topicName, event->topic) == 0)
                    {
                        discoveryState = MosquetteerShared::DISCOVERY_OK;
                        stopTimer(&discoveryTimer, DISCOVERY);
                        makeDefinitions(event->data, event->data_len);
                    }
                }
                else if (discoveryState == MosquetteerShared::DISCOVERY_OK &&
                        (size_t)event->data_len >= prefixLen &&
                        strncmp(event->data, templateStart, prefixLen) == 0)
                {
                    parseCallback(event->data, event->data_len);
                }
                break;
            }
        case MQTT_EVENT_ERROR:
            MQTTO_LOGN("MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
            {
                MQTTO_LOGFN("Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                MQTTO_LOGFN("Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                MQTTO_LOGFN("Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                         strerror(event->error_handle->esp_transport_sock_errno));
            }
            else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
            {
                MQTTO_LOGFN("Connection refused error: 0x%x", event->error_handle->connect_return_code);
            }
            else
            {
                MQTTO_LOGFN("Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;
        default:
            MQTTO_LOGFN("Other event id:%d", event->event_id);
            break;
        }
    }

    void parseCallback(const char *data, size_t len)
    {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err != DeserializationError::Ok)
        {
            MQTTO_LOGFN("Error parsing json for event: %s", err.c_str());
            return;
        }

        const char *id = doc["id"];
        for (auto &def : definitions)
        {
            if (strcasecmp(def.id.get(), id) != 0 || def.cb == nullptr)
            {
                continue;
            }

            const char *val = doc["val"];
            uint32_t hash = MosquetteerShared::fnv1a32((const uint8_t*) val, strlen(val));
            MosquetteerProp prop(val, hash != def.lastValueHash);
            def.lastValueHash = hash;
            MQTTO_LOGFN("Calling callback for %s, with data %s", def.id.get(), prop.toString());
            def.cb(prop);
        }
    }

    static void discoveryTimeout(void* arg)
    {
        if (arg == nullptr)
        {
            return;
        }

        auto* instance = static_cast<Mosquetteer*>(arg);
        instance->doDiscoveryTimeout();
    }

    void doDiscoveryTimeout()
    {
        if (discoveryState == MosquetteerShared::DISCOVERY_OK)
        {
            MQTTO_LOGN("Discovery timed out, but state is ok, cancelling timer.");
            return;
        }

        stopTimer(&discoveryTimer, DISCOVERY);
        MQTTO_LOGN("Discovery for device definition timed out, proceeding with definition.");
        discoveryState = MosquetteerShared::DISCOVERY_TIMEOUT;
        makeDefinitions();
    }

    void startDiscovery()
    {
        discoveryState = MosquetteerShared::DISCOVERY_WAITING;
        esp_timer_create_args_t args = {
            .callback = discoveryTimeout,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "discovery_timeout"
        };

        char topicName[getDeviceTopic(nullptr)]{};
        getDeviceTopic(topicName);
        esp_mqtt_client_subscribe(handle, topicName, 1);

        esp_timer_create(&args, &discoveryTimer);
        esp_timer_start_once(discoveryTimer, MQ_DISCOVERY_TIMEOUT);
    }

    void makeDefinitions(const char *discoveryData = nullptr, size_t discoveryLen = 0)
    {
        MQTTO_LOGN("Running definitions for entities.");
        char *buffer = new char[MQ_CLIENT_BUFSIZE]{};

        char deviceTopic[getDeviceTopic(nullptr)]{};
        getDeviceTopic(deviceTopic);
        esp_mqtt_client_unsubscribe(handle, deviceTopic);

        JsonDocument rootDoc;
        rootDoc["availability_topic"] = availabilityTopic;
        rootDoc["payload_available"] = MQ_HA_PAYLOAD_AVAILABLE;
        rootDoc["payload_not_available"] = MQ_HA_PAYLOAD_NOT_AVAILABLE;

        JsonObject device = rootDoc["dev"].to<JsonObject>();
        device["ids"] = deviceId;

        if (hasText(deviceName))
        {
            device["name"] = deviceName;
        }

        if (hasText(deviceManufacturer))
        {
            device["mf"] = deviceManufacturer;
        }

        if (hasText(deviceModel))
        {
            device["mdl"] = deviceModel;
        }

        if (hasText(deviceFirmwareVersion))
        {
            device["sw"] = deviceFirmwareVersion;
        }

        JsonObject o = rootDoc["o"].to<JsonObject>();
        o["name"] = MQTTO_NAME;
        o["sw"] = MQTTO_VER;
        o["url"] = MQTTO_URL;

        JsonObject comps = rootDoc["cmps"].to<JsonObject>();
        for (auto &def : definitions)
        {
            MQTTO_LOGFN("Making definitions for %s.", def.id.get());
            snprintf(buffer, MQ_CLIENT_BUFSIZE, "%s_%s", deviceId, def.id.get());

            JsonObject obj = comps[def.id.get()].to<JsonObject>();
            obj["p"] = MosquetteerShared::getTypeName(def.type);
            obj["name"] = def.name.get();
            obj["unique_id"] = buffer;
            obj["icon"] = MosquetteerShared::getIconByType(def.type);

            if (MosquetteerShared::hasCapability(def.type, MosquetteerShared::CAP_STATE))
            {
                MosquetteerShared::getTopic(def.id.get(), deviceId, MQ_HA_STATE_SUFFIX, buffer);
                obj["state_topic"] = buffer;
                MQTTO_LOGFN("State topic: %s", buffer);
            }

            if (MosquetteerShared::hasCapability(def.type, MosquetteerShared::CAP_COMMAND))
            {
                snprintf(buffer, MQ_CLIENT_BUFSIZE, MQ_HA_COMMAND_TEMPLATE, def.id.get());
                obj["command_template"] = buffer;

                MosquetteerShared::getTopic(def.id.get(), deviceId, MQ_HA_SET_SUFFIX, buffer);
                obj["command_topic"] = buffer;

                MQTTO_LOGFN("Subscribe to command topic: %s", buffer);
                esp_mqtt_client_subscribe(handle, buffer, 1);
            }

            if (MosquetteerShared::hasCapability(def.type, MosquetteerShared::CAP_PAYLOAD_ON_OFF))
            {
                obj["payload_on"] = MQ_HA_PAYLOAD_AVAILABLE;
                obj["payload_off"] = MQ_HA_PAYLOAD_NOT_AVAILABLE;
            }

            if (MosquetteerShared::hasCapability(def.type, MosquetteerShared::CAP_STATE_ON_OFF))
            {
                obj["state_on"] = MQ_HA_PAYLOAD_AVAILABLE;
                obj["state_off"] = MQ_HA_PAYLOAD_NOT_AVAILABLE;
            }

            if (MosquetteerShared::hasCapability(def.type, MosquetteerShared::CAP_PRESS))
            {
                obj["payload_press"] = MQ_HA_PAYLOAD_AVAILABLE;
            }

            if (def.config != nullptr)
            {
                def.config->setConfig(obj);
            }
        }

        if (discoveryData != nullptr && discoveryLen > 0)
        {
            MQTTO_LOGN("Receiving existing device definition, checking.");

            JsonDocument filter;
            filter["cmps"] = true;

            JsonDocument discoveryDoc;
            deserializeJson(discoveryDoc, discoveryData, discoveryLen, DeserializationOption::Filter(filter));

            JsonObject discoveryComps = discoveryDoc["cmps"];
            for (JsonPair kv : discoveryComps) {
                const char* componentName = kv.key().c_str();
                auto component = kv.value().as<JsonObject>();

                bool found = false;
                for (auto &def : definitions)
                {
                    if (strcasecmp(def.id.get(), componentName) == 0)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    MQTTO_LOGFN("Found orphan definition for id %s, adding for removal", componentName);
                    comps[componentName]["p"] = component["p"];
                }
            }
        }

        MQTTO_LOG("Generated json for device: ");
        serializeJson(rootDoc, Serial);
        MQTTO_LOGNW();

        size_t len = measureJson(rootDoc);
        if (len > MQ_CLIENT_BUFSIZE)
        {
            MQTTO_LOGFN("Error serializing json, generated json does not fit in the client buffer, please increase the buffer size. "
                        "Buffer size is %zu, json size is %zu", MQ_CLIENT_BUFSIZE, len);
        }

        MQTTO_LOGFN("Sending discovery package to topic %s.", deviceTopic);
        len = serializeJson(rootDoc, buffer, MQ_CLIENT_BUFSIZE);
        esp_mqtt_client_publish(handle, deviceTopic, buffer, len, 1, true);
        sendAvailabilityBurst(true);
        delete[] buffer;
    }

    static void sendAvailability(void* arg)
    {
        if (arg == nullptr)
        {
            return;
        }

        auto *instance = static_cast<Mosquetteer*>(arg);
        instance->doSendAvailability();
    }

    void doSendAvailability()
    {
        MQTTO_LOGFN("Sending online package to availability topic %s, count %d", availabilityTopic, availabilityCounter);
        size_t len = strlen(MQ_HA_PAYLOAD_AVAILABLE) + 1;
        char buf[len]{};
        size_t written = snprintf(buf, len, "%s", isAvailabilityOn ? MQ_HA_PAYLOAD_AVAILABLE : MQ_HA_PAYLOAD_NOT_AVAILABLE);
        esp_mqtt_client_publish(handle, availabilityTopic, buf, written, 1, true);

        availabilityCounter++;
        if (availabilityCounter >= 3)
        {
            stopTimer(&availabilityTimer, AVAILABILITY);
            hasAvailabilityTimer = false;
        }
    }

    void sendAvailabilityBurst(bool on)
    {
        if (hasAvailabilityTimer)
        {
            return;
        }
        hasAvailabilityTimer = true;
        availabilityCounter = 0;
        isAvailabilityOn = on;

        esp_timer_create_args_t args = {
            .callback = sendAvailability,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "availability_burst"
        };

        esp_timer_create(&args, &availabilityTimer);
        esp_timer_start_periodic(
            availabilityTimer,
            MQ_AVAILABILITY_TIMER_VAL
        );
    }

    static void allocAndSet(char** ptr, const char* val)
    {
        if (ptr == nullptr || val == nullptr || strlen(val) == 0)
        {
            return;
        }

        if (*ptr != nullptr)
        {
            delete[] *ptr;
            *ptr = nullptr;
        }

        size_t len = strlen(val) + 1;
        *ptr = new char[len];
        snprintf(*ptr, len, "%s", val);
    }

    static void unalloc(char** ptr)
    {
        if (ptr != nullptr && *ptr != nullptr)
        {
            delete[] *ptr;
            *ptr = nullptr;
        }
    }

    static bool hasText(const char* str)
    {
        return str != nullptr && strlen(str) > 0;
    }

    enum MQTTOTimerType
    {
        AVAILABILITY,
        DISCOVERY,
    };

    void stopTimer(esp_timer_handle_t *handle, MQTTOTimerType t)
    {
        if (handle == nullptr || *handle == nullptr)
        {
            return;
        }

        esp_timer_stop(*handle);
        esp_timer_delete(*handle);
        *handle = nullptr;

        if (t == AVAILABILITY)
        {
            availabilityCounter = 0;
            hasAvailabilityTimer = false;
        }
    }

    char *mqttUri = nullptr;
    char *username = nullptr;
    char *password = nullptr;
    char *clientId = nullptr;
    char *availabilityTopic = nullptr;

    char *deviceId = nullptr;
    char *deviceFirmwareVersion = nullptr;
    char *deviceName = nullptr;
    char *deviceModel = nullptr;
    char *deviceManufacturer = nullptr;

    esp_mqtt_client_handle_t handle = nullptr;
    esp_mqtt_client_config_t cfg{};
    bool started = false;
    bool hasAvailabilityTimer = false;

    std::vector<MosquetteerShared::Definition> definitions;
    MosquetteerShared::DiscoveryState discoveryState = MosquetteerShared::DISCOVERY_WAITING;

    esp_timer_handle_t discoveryTimer = nullptr;
    esp_timer_handle_t availabilityTimer = nullptr;
    bool isAvailabilityOn = false;
    int availabilityCounter = 0;
    const char *cacert = nullptr;
};

#endif //MOSQUETTEER_H
