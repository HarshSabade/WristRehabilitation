# Wrist Rehabilitation Monitor

An on-device Edge AI prototype for recognizing personalized wrist rehabilitation movements with an MPU-6050 and an ESP8266 NodeMCU.

## Current model

The included Edge Impulse Arduino library contains a three-class model:

- `idle`
- `flexion`
- `extension`

The model expects 100 samples per window at 50 Hz, with six inputs in this order:

```text
ax, ay, az, gx, gy, gz
```

The inference sketch rejects predictions below 80% confidence as `uncertain`.

## Hardware

- ESP8266 NodeMCU v3
- MPU-6050

### Wiring

| MPU-6050 | NodeMCU v3 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | D2 / GPIO4 |
| SCL | D1 / GPIO5 |

Use 3.3 V logic. The MPU-6050 address can be `0x68` or `0x69`; the sketch checks both.

## Arduino setup

1. Install the ESP8266 board package in Arduino IDE.
2. Select **NodeMCU 1.0 (ESP-12E Module)**.
3. Install the included Edge Impulse library:
   - `Sketch` → `Include Library` → `Add .ZIP Library`, or
   - copy `ei-wrist-rehabilitation-monitor-arduino-1.0.2-impulse-#1` into the Arduino libraries directory.
4. Open `wrist_rehab_inference/wrist_rehab_inference.ino`.
5. Select the NodeMCU COM port.
6. Upload and open Serial Monitor at `115200`.

The sketch collects a two-second window, runs inference locally, and prints class probabilities and the confidence-filtered decision.

## Data logger

`wrist_rehab_logger.ino` is the 50 Hz labeled CSV logger used to collect training data. It supports:

```text
l=idle
l=flexion
l=extension
start
stop
```

The logger emits:

```csv
timestamp_ms,ax,ay,az,gx,gy,gz,label
```

`save_wrist_data.py` captures clean CSV files over USB Serial. Install its only Python dependency with:

```powershell
py -m pip install pyserial
```

Close Arduino Serial Monitor before running the capture script because the COM port is exclusive.

## Limitations

This is a research prototype, not a medical device. Repetition counting, phone feedback, and user-specific calibration are not yet included in the inference sketch. Validate predictions on a separately recorded session before using the system for rehabilitation decisions.
