# Mosquetteer

> A lightweight ESP32 library for exposing devices to Home Assistant
> over MQTT with automatic device discovery.

- [Mosquetteer](#mosquetteer)
    * [Features](#features)
    * [Supported Entity Types](#supported-entity-types)
- [Quick Start](#quick-start)
- [Defining Entities](#defining-entities)
- [Entity Configuration](#entity-configuration)
- [Publishing State](#publishing-state)
    * [Return Value](#return-value)
    * [Example](#example)
- [Receiving Commands](#receiving-commands)
- [Device Discovery](#device-discovery)
- [TLS Support](#tls-support)

---

## Features

- Automatic Home Assistant MQTT Device Discovery
- Automatic entity registration
- State publishing
- Command subscriptions
- Automatic cleanup of removed entities
- Strongly typed entity configuration
- TLS support
- Minimal setup

---

## Supported Entity Types

Currently supported Home Assistant entities:

- Sensor
- Binary Sensor
- Switch
- Button
- Number
- Text
- Event
- Select
- Image
- Wrapping custom topics

Additional entity types may be added in future releases.

---

# Quick Start

Create a Mosquetteer instance, configure your MQTT broker and device
information, define your entities, then connect.

```cpp
#include <Mosquetteer.h>

Mosquetteer mqtt;

void setup() {
    mqtt.setClientConfig("mqtt://192.168.1.100:1883");

    mqtt.setDeviceInfo(
        "living_room_sensor",
        "Living Room Sensor"
    );

    mqtt.define(
        "temperature",
        "Temperature",
        MosquetteerShared::SENSOR
    );

    mqtt.begin();
}

void loop() {
    mqtt.sendState("temperature", 23.5);
}
```

Once connected:

1. The device publishes its discovery configuration.
2. Home Assistant automatically creates the entity.
3. State updates appear immediately in Home Assistant.

---

# Defining Entities

Every exposed Home Assistant entity must first be defined.

```cpp
mqtt.define(
    "temperature",
    "Temperature",
    MosquetteerShared::SENSOR
);
```

Arguments:

| Parameter | Description                                |
|-----------|--------------------------------------------|
| ID        | Unique entity identifier within the device |
| Name      | Display name shown in Home Assistant       |
| Type      | Entity type                                |

The entity ID is used when sending state and receiving commands.

---

# Entity Configuration

Most entity types support additional Home Assistant metadata.

Example:

```cpp
MosquetteerShared::SensorConfig cfg;

cfg.deviceClass = MosquetteerClasses::TEMPERATURE;
cfg.stateClass = MosquetteerShared::MEASUREMENT;
strcpy(cfg.unit.unit, "°C");

mqtt.define(
    "temperature",
    "Temperature",
    MosquetteerShared::SENSOR,
    &cfg
);
```

Common configuration options include:

- Device class
- State class
- Unit of measurement
- Value limits
- Display mode
- Entity category
- Icons
- Availability options

The available fields depend on the entity type.

## Select Entity

The select entity allows you to expose a dropdown menu of predefined options to Home Assistant. 
To define a select entity, configure a MosquetteerShared::SelectConfig with an array of C-strings representing your options and the total option count.

```cpp
const char* fanSpeeds[] = {"Low", "Medium", "High"};

MosquetteerShared::SelectConfig selectCfg;
selectCfg.options = fanSpeeds;
selectCfg.optionsCount = 3;

mqtt.define(
    "fan_speed",
    "Fan Speed",
    MosquetteerShared::SELECT,
    &selectCfg
);
```

Commands selected from the Home Assistant UI are received via the standard onCommand() callback.

> The options array must remain valid for the lifetime of the entity. Define it with static or global storage rather than as a temporary local variable.

## Image Entity

The image entity allows you to send binary image payloads directly to Home Assistant. 
Configure it using MosquetteerShared::ImageConfig to specify the image MIME type via the ``binaryConfig.contentType`` property.

```cpp
MosquetteerShared::ImageConfig imgCfg;
imgCfg.binaryConfig.contentType = "image/jpeg";

mqtt.define(
    "camera_feed",
    "Camera Feed",
    MosquetteerShared::IMAGE,
    &imgCfg
);
```

To publish an image, use an overloaded sendState() method that accepts the entity ID, a pointer to the binary data, and the data length as a size_t.

```cpp
// Assuming `imageBuffer` holds your data and `imageLen` is its size in bytes
mqtt.sendState("camera_feed", (const char*)imageBuffer, imageLen);
```

> The content-type char array must remain valid for the lifetime of the entity. Define it with static or global storage rather than as a temporary local variable.

## Custom Topic

The custom_topic entity lets you route arbitrary MQTT topics through the Mosquetteer client while bypassing standard Home Assistant capabilities.

Configure it using MosquetteerShared::CustomConfig, specifying the sendTopic and receiveTopic strings. You can also configure publish behavior with options such as qos, retain, and enqueue.

This is useful when you need to send and receive MQTT messages without running multiple MQTT clients on your board.

```cpp
MosquetteerShared::CustomConfig customCfg;
customCfg.sendTopic = "custom/device/tx";
customCfg.receiveTopic = "custom/device/rx";
customCfg.qos = 1;
customCfg.retain = false;
customCfg.enqueue = false;

mqtt.define(
    "custom_bridge",
    "Custom Bridge",
    MosquetteerShared::CUSTOM_TOPIC,
    &customCfg
);
```

To publish raw data to the sendTopic, use sendState() with the payload and length.

```cpp
mqtt.sendState("custom_bridge", rawPayload, payloadLength);
```

To listen for messages on the receiveTopic, register a callback using onData() instead of onCommand(). 
The DataCallback provides the raw data buffer and length.

```cpp
mqtt.onData("custom_bridge", [](const char* data, size_t len) {
    // Process incoming raw data
});
```

---

# Publishing State

State can be published at any time after `begin()`.

```cpp
mqtt.sendState("temperature", 22.4);
mqtt.sendState("enabled", true);
mqtt.sendState("status", "Running");
```

Mosquetteer automatically serializes supported value types.

Typical supported values include:

- `bool`
- Integer types
- Floating-point types
- Strings
- Character arrays

## Return Value

`sendState()` returns an integer indicating the result of the publish request.

| Return value | Meaning                                  |
|--------------|------------------------------------------|
| `>= 0`       | State successfully queued for publishing |
| `-1`         | Failed to queue or publish the state     |
| `-2`         | Publish queue is full                    |

Example:

```cpp
int result = mqtt.sendState("temperature", 23.5);

if (result < 0) {
    // Handle publish failure
}
```

## Example

```cpp
mqtt.sendState("temperature", 22.4);
mqtt.sendState("enabled", true);
mqtt.sendState("status", "Running");
mqtt.sendState("speed", 1500);
```

> **Note**
>
> `sendState()` does not publish the state immediately. Instead, the update is
> added to an internal publish queue and sent asynchronously.

---

# Receiving Commands

Entities that support commands can register callbacks.

```cpp
mqtt.onCommand("brightness", [](const MosquetteerProp& value) {
    if (value.hasChanged) {
        int brightness = value.toInt();
    }
});
```

The callback receives a `MosquetteerProp` which provides convenient conversion
methods for reading the incoming value.

Example conversions:

```cpp
value.toBool();
value.toInt();
value.toFloat();
value.toString();
```

The `hasChanged` flag allows ignoring duplicate commands.

---

# Device Discovery

When `begin()` is called, Mosquetteer automatically publishes Home Assistant
MQTT Discovery payloads for every defined entity.

If entities are removed from your firmware, Mosquetteer will try to automatically clean up
their discovery topics so obsolete entities disappear from Home Assistant.

---

# TLS Support

Mosquetteer supports secure MQTT connections over TLS.

Simply call `setTlsConfig` to set the CA certificate of your MQTT server, then
use the `mqtts://` protocol on the client URI before calling `begin()`.

---

# Full Example

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <Mosquetteer.h>

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

constexpr char WIFI_SSID[] = "ssid";
constexpr char WIFI_PASSWORD[] = "password";

constexpr char MQTT_URI[] = "mqtts://192.168.100.1:8883";
constexpr char MQTT_USERNAME[] = "esp32";
constexpr char MQTT_PASSWORD[] = "password";

constexpr char DEVICE_ID[] = "pressure_station";
constexpr char DEVICE_NAME[] = "Pressure Sensor";

constexpr char PRESSURE_ID[] = "pressure";
constexpr char ENABLED_ID[] = "enabled";

// -----------------------------------------------------------------------------
// CA Certificate
// -----------------------------------------------------------------------------

const char* CA_CERT = R"EOF(
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
)EOF";

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

Mosquetteer mqtt;

uint32_t pressure = 1000;
unsigned long lastUpdate = 0;

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void connectWiFi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(250);
        Serial.print(".");
    }

    Serial.println("\nConnected.");
}

void configureMQTT()
{
    mqtt.setClientConfig(
        MQTT_URI,
        MQTT_USERNAME,
        MQTT_PASSWORD
    );

    mqtt.setTlsConfig(CA_CERT);

    mqtt.setDeviceInfo(
        DEVICE_ID,
        DEVICE_NAME
    );
}

void defineEntities()
{
    MosquetteerShared::SensorConfig pressureCfg;

    pressureCfg.deviceClass = MosquetteerClasses::ATMOSPHERIC_PRESSURE;
    strcpy(pressureCfg.unit.unit, "hPa");

    mqtt.define(
        PRESSURE_ID,
        "Pressure",
        MosquetteerShared::SENSOR,
        &pressureCfg
    );

    mqtt.define(
        ENABLED_ID,
        "Enabled",
        MosquetteerShared::SWITCH
    );

    mqtt.onCommand(ENABLED_ID, [](const MosquetteerProp& value)
    {
        Serial.printf(
            "Switch changed: %s (changed=%s)\n",
            value.toString(),
            value.hasChanged ? "true" : "false"
        );
    });
}

void setup()
{
    Serial.begin(115200);
    connectWiFi();
    configureMQTT();
    defineEntities();
    mqtt.begin();
}

// -----------------------------------------------------------------------------
// Main Loop
// -----------------------------------------------------------------------------

void loop()
{
    if (millis() - lastUpdate >= 3000)
    {
        pressure++;
        mqtt.sendState(PRESSURE_ID, pressure);
        lastUpdate = millis();
    }
}
```