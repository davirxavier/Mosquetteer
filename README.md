# Mosquetteer

> A lightweight ESP32 library for exposing devices to Home Assistant
> over MQTT with automatic device discovery.

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
#include <Mosquetteer.h>

Mosquetteer mqtt;

void setup() {
    mqtt.setClientConfig("mqtt://192.168.1.100:1883");

    mqtt.setDeviceInfo(
        "weather_station",
        "Weather Station"
    );

    MosquetteerShared::SensorConfig tempCfg;
    tempCfg.deviceClass = MosquetteerClasses::TEMPERATURE;
    tempCfg.stateClass = MosquetteerShared::MEASUREMENT;
    strcpy(tempCfg.unit.unit, "°C");

    mqtt.define(
        "temperature",
        "Temperature",
        MosquetteerShared::SENSOR,
        &tempCfg
    );

    mqtt.define(
        "humidity",
        "Humidity",
        MosquetteerShared::SENSOR
    );

    mqtt.begin();
}

void loop() {
    mqtt.sendState("temperature", 22.8);
    mqtt.sendState("humidity", 41);
}
```