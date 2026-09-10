# Factory Model

An ESP32-based smart factory model developed to demonstrate industrial monitoring, automation, safety detection, actuator control, and IoT-based remote monitoring using ThingsBoard Cloud.

The project integrates multiple sensors, power monitoring modules, actuators, safety mechanisms, and an ESP32 controller into a miniature factory environment.

---

## Project Overview

The Factory Model is a miniature industrial automation and monitoring system designed to demonstrate how different factory processes can be monitored and controlled through a centralized embedded controller.

The ESP32 acts as the main controller and communicates with sensors and actuators while transmitting telemetry and receiving control commands through MQTT.

ThingsBoard Cloud is used as the IoT platform for:

- Real-time sensor monitoring
- Power monitoring
- Actuator control
- RPC commands
- Servo control
- Factory status visualization
- Remote operation

---

## Key Features

- Temperature and humidity monitoring
- Multiple temperature measurements
- Object detection
- Conveyor motor control
- Conveyor power monitoring
- Factory lighting control
- Lighting power monitoring
- Heater ON/OFF control
- Dual mist maker control
- Four-channel laser safety fence
- Automatic servo movement
- Servo home-position control
- Buzzer-based safety alarm
- Red and green status indication
- MQTT communication
- ThingsBoard Cloud integration
- ThingsBoard dashboard
- Remote RPC-based actuator control

---

# System Architecture

```text
                         ┌─────────────────────┐
                         │        ESP32        │
                         │   Main Controller   │
                         └──────────┬──────────┘
                                    │
              ┌─────────────────────┼─────────────────────┐
              │                     │                     │
              ▼                     ▼                     ▼
        Sensors & Monitoring     Safety System        Actuators
              │                     │                     │
     ┌────────┼────────┐       ┌────┴────┐       ┌──────┼────────┐
     │        │        │       │         │       │      │        │
    SHT31   DS18B20  INA226   Laser     LDR    Motor  Lighting Heater
     │        │        │       │         │       │      │        │
     │        │        │       └────┬────┘       │      │        │
     │        │        │            │            │      │        │
     │        │        │        Buzzer           │      │        │
     │        │        │                         │      │        │
     │        │        └─────────────────────────┘      │        │
     │        │                                         │        │
     └────────┴─────────────────────────────────────────┴────────┘
                                    │
                                    ▼
                              MQTT / Wi-Fi
                                    │
                                    ▼
                          ┌───────────────────┐
                          │ ThingsBoard Cloud │
                          │     Dashboard     │
                          └───────────────────┘
````

---

# Hardware

## Main Controller

| Component               | Quantity | Purpose         |
| ----------------------- | -------: | --------------- |
| ESP32 Development Board |        1 | Main controller |

---

## Sensors

| Component                           | Quantity | Purpose                             |
| ----------------------------------- | -------: | ----------------------------------- |
| SHT31 Temperature & Humidity Sensor |        2 | Temperature and humidity monitoring |
| DS18B20 Temperature Sensor          |        1 | Temperature monitoring              |
| E18-D80NK Sensor                    |        1 | Object detection                    |
| LDR Sensor Module                   |        4 | Laser beam detection                |
| Laser Module                        |        4 | Safety beam generation              |

---

## Power Monitoring

| Component                   | Quantity | Purpose                               |
| --------------------------- | -------: | ------------------------------------- |
| INA226                      |        2 | Voltage, current and power monitoring |
| 0.1 Ω Shunt Resistor (R100) |        2 | Current measurement                   |

### INA226 #1

Monitors the **factory lighting power consumption**.

```text
Bus      : I²C Bus 1
Address  : 0x45
Shunt    : 0.1 Ω
```

### INA226 #2

Monitors the **conveyor motor power consumption**.

```text
Bus      : I²C Bus 2
Address  : 0x45
Shunt    : 0.1 Ω
```

The two INA226 modules can use the same I²C address because they are connected to separate I²C buses.

---

# Actuators

| Component             | Quantity | Purpose                           |
| --------------------- | -------: | --------------------------------- |
| L298N Motor Driver    |        1 | Conveyor motor control            |
| 12 V DC Gear Motor    |        1 | Conveyor                          |
| MOSFET Trigger Module |        3 | Lighting and mist maker switching |
| Relay Module          |        1 | Heater control                    |
| Servo Motor           |        2 | Factory entrance mechanism        |
| 5 V Mist Maker        |        2 | Factory process simulation        |
| 12 V Heater           |        1 | Heating process simulation        |
| 12 V Factory Lighting |        1 | Factory lighting                  |
| Red LED               |        1 | Warning indication                |
| Green LED             |        1 | Normal status indication          |
| Buzzer                |        1 | Safety alarm                      |

---

# Power Supply

The Factory Model uses two separate DC power supplies.

## 12 V DC Supply

```text
12 V DC / 5 A Adapter
        │
        ├── L298N
        │     └── Conveyor Motor
        │
        ├── Lighting MOSFET
        │     └── 12 V Factory Lighting
        │
        └── Heater / Relay circuit
```

The 12 V supply is used for the higher-power factory loads.

### Rating

```text
Output : 12 V DC
Current: 5 A
```

---

## 5 V DC Supply

```text
5 V DC / 5 A Adapter
        │
        ├── ESP32 / controller supply
        ├── SHT31 / sensor supply
        ├── Servos
        ├── Mist Maker #1
        ├── Mist Maker #2
        ├── Laser modules
        ├── Relay/control modules
        └── Other 5 V peripherals
```

### Rating

```text
Output : 5 V DC
Current: 5 A
```

---

## Grounding

The system uses a common ground reference between the controller and relevant modules.

```text
12 V GND ─────────────┐
                      │
5 V GND ──────────────┼──── ESP32 GND
                      │
Module GND ───────────┘
```

The positive 12 V and 5 V rails remain separate.

---

# ESP32 GPIO Mapping

|    GPIO | Device          | Function                  |
| ------: | --------------- | ------------------------- |
| GPIO 21 | I²C Bus 1       | SDA                       |
| GPIO 22 | I²C Bus 1       | SCL                       |
| GPIO 26 | I²C Bus 2       | SDA                       |
| GPIO 25 | I²C Bus 2       | SCL                       |
|  GPIO 4 | DS18B20         | Temperature data          |
| GPIO 13 | E18-D80NK       | Object detection          |
| GPIO 27 | LDR #1          | Laser beam detection      |
| GPIO 32 | LDR #2          | Laser beam detection      |
| GPIO 33 | LDR #3          | Laser beam detection      |
| GPIO 34 | LDR #4          | Laser beam detection      |
| GPIO 19 | L298N ENA       | Conveyor motor enable     |
| GPIO 14 | Mist MOSFET #1  | Mist Maker #1             |
| GPIO 23 | Mist MOSFET #2  | Mist Maker #2             |
| GPIO 18 | Heater Relay    | Heater control            |
| GPIO 12 | Lighting MOSFET | Factory lighting          |
| GPIO 16 | Servo #1        | Entrance/inspection servo |
| GPIO 17 | Servo #2        | Entrance/inspection servo |
|  GPIO 2 | Red LED         | Warning indication        |
|  GPIO 5 | Green LED       | Normal status             |
| GPIO 15 | Buzzer          | Safety alarm              |

---

# I²C Architecture

Two independent I²C buses are used.

## I²C Bus 1

```text
SDA → GPIO21
SCL → GPIO22
```

Devices:

```text
INA226 #1 → 0x45
SHT31  #1 → 0x44
```

Purpose:

```text
Factory lighting monitoring
Temperature and humidity monitoring
```

---

## I²C Bus 2

```text
SDA → GPIO26
SCL → GPIO25
```

Devices:

```text
INA226 #2 → 0x45
SHT31  #2 → 0x44
```

Purpose:

```text
Conveyor motor monitoring
Temperature and humidity monitoring
```

---

# Temperature and Humidity Monitoring

Two SHT31 sensors are used at different locations in the factory model.

### SHT31 #1

```text
I²C Bus : Bus 1
Address : 0x44
```

### SHT31 #2

```text
I²C Bus : Bus 2
Address : 0x44
```

The sensors provide:

* Temperature
* Relative humidity

Example development readings:

```text
Temperature : ~34 °C
Humidity    : ~58–66 %RH
```

---

# DS18B20

The DS18B20 provides an additional temperature measurement.

```text
VCC  → 3.3 V
GND  → GND
DATA → GPIO4
```

A 4.7 kΩ pull-up resistor is connected between DATA and 3.3 V.

Example tested readings:

```text
33.4 °C
33.8 °C
34.1 °C
```

---

# E18-D80NK Object Detection

The E18-D80NK is used to detect objects in the factory model.

### Wiring

```text
Brown → 5 V
Blue  → GND
Black → GPIO13
```

### Detection Logic

```text
GPIO13 = HIGH
        ↓
    NO OBJECT

GPIO13 = LOW
        ↓
 OBJECT DETECTED
```

---

# Laser Safety Fence

Four laser modules and four LDR sensors create a multi-beam safety fence.

```text
Laser #1 ───── LDR #1 → GPIO27
Laser #2 ───── LDR #2 → GPIO32
Laser #3 ───── LDR #3 → GPIO33
Laser #4 ───── LDR #4 → GPIO34
```

The laser modules are continuously powered during normal operation.

### LDR Logic

```text
LOW  → Beam Clear / SAFE
HIGH → Beam Blocked / INTRUSION
```

---

## Safety Alarm Operation

When any laser beam is interrupted:

```text
Laser Beam Blocked
        ↓
LDR Detection
        ↓
ESP32 Detects Intrusion
        ↓
Buzzer ON
```

When the beam is restored:

```text
Laser Beam Clear
        ↓
ESP32 Detects Safe State
        ↓
Buzzer OFF
```

This provides a simple simulated industrial safety-zone monitoring system.

---

# Conveyor System

The conveyor is controlled using an L298N motor driver.

### L298N Connections

```text
ESP32 GPIO19 → ENA

IN1 → 3.3 V
IN2 → GND

OUT1 → Motor
OUT2 → Motor
```

The conveyor motor is powered from the 12 V DC supply.

### Power Monitoring

INA226 #2 monitors:

* Conveyor voltage
* Conveyor current
* Conveyor power

---

# Factory Lighting

Factory lighting is controlled using a MOSFET trigger module.

```text
ESP32 GPIO12
      │
      ▼
Lighting MOSFET
      │
      ▼
12 V Factory Lighting
```

INA226 #1 monitors the lighting electrical parameters.

The monitored values include:

* Voltage
* Current
* Power

### Example Tested Reading

```text
Voltage : 12.252 V
Current : 0.819 A
Power   : 10.033 W
```

---

# Heater Control

The heater is controlled using a relay.

```text
ESP32 GPIO18
      │
      ▼
Relay Module
      │
      ▼
Heater
```

The heater does **not** use an automatic temperature threshold in the final implementation.

Instead, the heater is controlled manually through a ThingsBoard RPC command.

```text
ThingsBoard RPC
      ↓
Heater ON/OFF
      ↓
ESP32
      ↓
Relay
      ↓
Heater
```

---

# Mist Maker Control

Two **5 V mist makers** are independently controlled.

### Mist Maker #1

```text
GPIO14 → MOSFET → Mist Maker #1
```

### Mist Maker #2

```text
GPIO23 → MOSFET → Mist Maker #2
```

Each mist maker can be independently controlled through the system.

---

# Servo System

Two servos are used for the factory entrance/inspection mechanism.

| Servo    |   GPIO |
| -------- | -----: |
| Servo #1 | GPIO16 |
| Servo #2 | GPIO17 |

The final firmware implements continuous automatic movement.

### Servo Movement

```text
Servo #1:
0° → 180°

Servo #2:
180° → 0°
```

The two servos therefore move in opposite directions.

---

## Servo Home Function

A **Home** button is available on the ThingsBoard dashboard.

The Home command returns both servos to their configured home position.

```text
ThingsBoard Home
       ↓
     RPC
       ↓
     ESP32
       ↓
 ┌─────┴─────┐
 ▼           ▼
Servo #1   Servo #2
 Home       Home
```

---

# Status LEDs

## Red LED

```text
GPIO2 → 220 Ω resistor → Red LED
```

Used for warning/fault indications.

## Green LED

```text
GPIO5 → 220 Ω resistor → Green LED
```

Used for normal/status indication.

---

# Buzzer

The buzzer is connected to:

```text
GPIO15
```

Primary safety operation:

```text
Beam Blocked → Buzzer ON

Beam Clear   → Buzzer OFF
```

The buzzer can also be tested manually through the control interface.

---

# IoT Communication

The Factory Model is connected to **ThingsBoard Cloud** using MQTT.

```text
┌─────────────┐
│    ESP32    │
└──────┬──────┘
       │
       │ Wi-Fi
       ▼
┌─────────────┐
│    MQTT     │
└──────┬──────┘
       │
       ▼
┌───────────────────┐
│ ThingsBoard Cloud │
└─────────┬─────────┘
          │
          ▼
      Dashboard
```

MQTT is used for communication between the ESP32 and ThingsBoard.

---

# ThingsBoard Dashboard

A ThingsBoard Cloud dashboard was created to monitor and control the Factory Model.

The dashboard provides real-time visualization and remote control of the system.

### Monitoring

The dashboard can display values such as:

* Temperature
* Humidity
* DS18B20 temperature
* Object detection status
* Laser/LDR status
* Lighting voltage
* Lighting current
* Lighting power
* Conveyor voltage
* Conveyor current
* Conveyor power
* Actuator states

### Remote Control

RPC controls are provided for:

* Conveyor motor
* Factory lighting
* Heater
* Mist Maker #1
* Mist Maker #2
* Servo Home position
* Other supported actuator functions

---

# RPC Control

The system uses ThingsBoard RPC commands to remotely control actuators.

Example concept:

```text
ThingsBoard Button
        ↓
      RPC
        ↓
      MQTT
        ↓
      ESP32
        ↓
    GPIO Control
        ↓
     Actuator
```

This allows the factory model to be controlled remotely from the ThingsBoard dashboard.

---

# Firmware

The ESP32 firmware is developed using the Arduino framework.

## Main Libraries

```text
Wire
INA226_WE
Adafruit SHT31
OneWire
DallasTemperature
ESP32Servo
WiFi
PubSubClient
```

The firmware handles:

* Sensor initialization
* I²C communication
* Temperature monitoring
* Humidity monitoring
* Power monitoring
* Object detection
* Laser safety monitoring
* Conveyor control
* Lighting control
* Heater control
* Mist maker control
* Servo control
* LED indication
* Buzzer control
* MQTT communication
* ThingsBoard RPC handling
* Telemetry transmission

---

# System Operating Flow

```text
                    SYSTEM START
                         │
                         ▼
                  ESP32 INITIALIZE
                         │
                         ▼
               Connect to Wi-Fi
                         │
                         ▼
              Connect to ThingsBoard
                         │
                         ▼
              Initialize Sensors
                         │
                         ▼
              Initialize Actuators
                         │
                         ▼
                    MAIN LOOP
                         │
        ┌────────────────┼────────────────┐
        │                │                │
        ▼                ▼                ▼
     Sensors          Safety          Actuators
        │                │                │
        ▼                ▼                ▼
 Temperature       Laser/LDR        Motor
 Humidity          Monitoring       Lighting
 DS18B20                │            Heater
 Object                  │            Mist
 Detection               ▼            Servos
 Power              Buzzer
 Monitoring
        │
        └────────────────┬────────────────┘
                         │
                         ▼
                  Send Telemetry
                         │
                         ▼
                  ThingsBoard Cloud
                         │
                         ▼
                   RPC Commands
                         │
                         ▼
                      ESP32
                         │
                         ▼
                    Actuators
```

---

# GPIO Summary

```text
I²C BUS 1
GPIO21 → SDA
GPIO22 → SCL

I²C BUS 2
GPIO26 → SDA
GPIO25 → SCL

GPIO4  → DS18B20
GPIO13 → E18-D80NK

GPIO27 → LDR1
GPIO32 → LDR2
GPIO33 → LDR3
GPIO34 → LDR4

GPIO19 → L298N ENA

GPIO14 → Mist Maker 1
GPIO23 → Mist Maker 2

GPIO18 → Heater Relay
GPIO12 → Lighting MOSFET

GPIO16 → Servo 1
GPIO17 → Servo 2

GPIO2  → Red LED
GPIO5  → Green LED
GPIO15 → Buzzer
```

---

# Testing & Validation

The Factory Model was developed through individual module testing followed by system integration.

## Sensors

| Device    | Status |
| --------- | ------ |
| SHT31 #1  | Tested |
| SHT31 #2  | Tested |
| DS18B20   | Tested |
| E18-D80NK | Tested |
| LDR #1    | Tested |
| LDR #2    | Tested |
| LDR #3    | Tested |
| LDR #4    | Tested |
| INA226 #1 | Tested |
| INA226 #2 | Tested |

---

## Actuators

| Device           | Status |
| ---------------- | ------ |
| Conveyor Motor   | Tested |
| Factory Lighting | Tested |
| Heater Relay     | Tested |
| Mist Maker #1    | Tested |
| Mist Maker #2    | Tested |
| Servo #1         | Tested |
| Servo #2         | Tested |
| Red LED          | Tested |
| Green LED        | Tested |
| Buzzer           | Tested |

---

# Example Sensor Readings

During hardware validation, the following readings were observed.

### SHT31

```text
Temperature : ~34 °C
Humidity    : ~58–66 %RH
```

### DS18B20

```text
Temperature : ~33.4–34.1 °C
```

### Lighting INA226

```text
Voltage : 12.252 V
Current : 0.819 A
Power   : 10.033 W
```

### Conveyor INA226

Small non-zero readings can be observed while the motor is OFF because of:

* INA226 measurement offset
* Electrical noise
* L298N idle consumption
* Module leakage/current consumption

Example:

```text
Voltage : ~12.66 V
Current : ~15.8 mA
Power   : ~0.20 W
```

---

## INA226 Non-Zero Current

The INA226 showed small current and power values even when the conveyor motor was OFF.

This was investigated and determined to be associated with the measurement offset and/or small idle current of the connected driver/module.

---

## Laser/LDR Logic

The LDR logic was validated against the actual hardware.

Final logic:

```text
LOW  → Beam Clear
HIGH → Beam Blocked
```

The firmware uses this logic for safety monitoring.

---


# Project Status


The Factory Model hardware and software integration was completed with:

* ESP32 control
* Multiple sensor interfaces
* Power monitoring
* Motor control
* Lighting control
* Heater control
* Mist maker control
* Servo control
* Laser/LDR safety monitoring
* Buzzer alarm
* MQTT communication
* ThingsBoard Cloud integration
* ThingsBoard dashboard
* RPC-based remote control

---

# Conclusion

The Factory Model demonstrates the integration of embedded control, industrial-style sensors, actuator systems, electrical power monitoring, safety detection, and cloud-based IoT monitoring into a single miniature factory platform.

The ESP32 serves as the central control unit, while ThingsBoard Cloud provides remote monitoring and control through MQTT and RPC communication.

The project provides a foundation for expanding the miniature factory into a more advanced industrial IoT demonstration platform.

---

## Technologies Used

```text
ESP32
Arduino Framework
C/C++
I²C
1-Wire
GPIO
MQTT
ThingsBoard Cloud
RPC
INA226
SHT31
DS18B20
E18-D80NK
LDR
Laser Sensors
L298N
MOSFET
Relay
Servo
```


