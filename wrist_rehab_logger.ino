#include <Wire.h>

#if defined(ESP8266)
constexpr uint8_t SDA_PIN = D2; // GPIO4
constexpr uint8_t SCL_PIN = D1; // GPIO5
#else
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;
#endif
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t SAMPLE_INTERVAL_US = 20000; // 50 Hz

constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_GYRO_CONFIG = 0x1B;
constexpr uint8_t REG_WHO_AM_I = 0x75;

char currentLabel[10] = "idle";
uint8_t mpuAddress = 0x68;
bool logging = false;
bool liveMode = false;
uint32_t nextSampleUs = 0;

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool readRegisters(uint8_t reg, uint8_t *buffer, size_t length) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(mpuAddress, static_cast<uint8_t>(length), true) !=
      static_cast<uint8_t>(length)) {
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    buffer[i] = Wire.read();
  }

  return true;
}

int16_t combineBytes(uint8_t highByte, uint8_t lowByte) {
  return static_cast<int16_t>(
      (static_cast<uint16_t>(highByte) << 8) | lowByte);
}

bool readMotion(int16_t &ax, int16_t &ay, int16_t &az,
                int16_t &gx, int16_t &gy, int16_t &gz) {
  uint8_t data[14];

  if (!readRegisters(REG_ACCEL_XOUT_H, data, sizeof(data))) {
    return false;
  }

  ax = combineBytes(data[0], data[1]);
  ay = combineBytes(data[2], data[3]);
  az = combineBytes(data[4], data[5]);
  gx = combineBytes(data[8], data[9]);
  gy = combineBytes(data[10], data[11]);
  gz = combineBytes(data[12], data[13]);
  return true;
}

void printHelp() {
  Serial.println("# Commands:");
  Serial.println("# l=idle       Set the label");
  Serial.println("# l=flexion");
  Serial.println("# l=extension");
  Serial.println("# l=partial");
  Serial.println("# start        Start CSV logging");
  Serial.println("# live         Start raw live streaming");
  Serial.println("# stop         Stop CSV logging");
  Serial.println("# test         Read one sensor sample");
  Serial.println("# help         Show this help");
}

void printMotionSample() {
  int16_t ax, ay, az, gx, gy, gz;
  if (!readMotion(ax, ay, az, gx, gy, gz)) {
    Serial.println("# Sensor read failed.");
    return;
  }

  Serial.print("# Sample: ");
  Serial.print(ax);
  Serial.print(", ");
  Serial.print(ay);
  Serial.print(", ");
  Serial.print(az);
  Serial.print(", ");
  Serial.print(gx);
  Serial.print(", ");
  Serial.print(gy);
  Serial.print(", ");
  Serial.println(gz);
}

void processCommand() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  command.trim();

  if (command == "start") {
    liveMode = false;
    if (!writeRegister(REG_PWR_MGMT_1, 0x00)) {
      Serial.println("# Could not wake the MPU-6050.");
      return;
    }
    delay(10);
    logging = true;
    nextSampleUs = micros();
    Serial.println("# Logging started");
    Serial.println("timestamp_ms,ax,ay,az,gx,gy,gz,label");
  } else if (command == "stop") {
    logging = false;
    liveMode = false;
    Serial.println("# Logging stopped");
  } else if (command == "live") {
    logging = false;
    liveMode = true;
    nextSampleUs = micros();
    Serial.println("# Live streaming started");
  } else if (command == "test") {
    printMotionSample();
  } else if (command == "help") {
    printHelp();
  } else if (command.startsWith("l=")) {
    String label = command.substring(2);
    if (label == "idle" || label == "flexion" ||
        label == "extension" || label == "partial") {
      label.toCharArray(currentLabel, sizeof(currentLabel));
      Serial.print("# Label set to ");
      Serial.println(currentLabel);
    } else {
      Serial.println("# Invalid label. Use idle, flexion, extension, or partial.");
    }
  } else if (command.length() > 0) {
    Serial.println("# Unknown command. Type help.");
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.setTimeout(20);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  uint8_t whoAmI = 0;
  mpuAddress = 0x68;
  if (!readRegisters(REG_WHO_AM_I, &whoAmI, 1)) {
    mpuAddress = 0x69;
    if (!readRegisters(REG_WHO_AM_I, &whoAmI, 1)) {
      Serial.println("# MPU-6050 not detected at address 0x68 or 0x69.");
      while (true) {
        delay(1000);
      }
    }
  }

  if (whoAmI != 0x68 && whoAmI != 0x69) {
    Serial.println("# The I2C device is not an MPU-6050.");
    while (true) {
      delay(1000);
    }
  }

  // Wake the sensor and select the common +/-2 g and +/-250 deg/s ranges.
  if (!writeRegister(REG_PWR_MGMT_1, 0x00) ||
      !writeRegister(REG_ACCEL_CONFIG, 0x00) ||
      !writeRegister(REG_GYRO_CONFIG, 0x00)) {
    Serial.println("# MPU-6050 initialization failed.");
    while (true) {
      delay(1000);
    }
  }
  delay(100);

  Serial.println("# Wrist rehabilitation data logger ready.");
  Serial.println("# Keep the IMU fixed on the wrist in the same orientation.");
  int16_t testAx, testAy, testAz, testGx, testGy, testGz;
  if (readMotion(testAx, testAy, testAz, testGx, testGy, testGz)) {
    Serial.print("# Test read: ");
    Serial.print(testAx);
    Serial.print(", ");
    Serial.print(testAy);
    Serial.print(", ");
    Serial.print(testAz);
    Serial.print(", ");
    Serial.print(testGx);
    Serial.print(", ");
    Serial.print(testGy);
    Serial.print(", ");
    Serial.println(testGz);
  } else {
    Serial.println("# Test motion read failed.");
  }
  printHelp();
  delay(1000);
  liveMode = true;
  nextSampleUs = micros();
}

void loop() {
  processCommand();

  if (!logging && !liveMode) {
    delay(2);
    return;
  }

  const uint32_t nowUs = micros();
  if (static_cast<int32_t>(nowUs - nextSampleUs) < 0) {
    return;
  }
  nextSampleUs += SAMPLE_INTERVAL_US;

  int16_t ax, ay, az, gx, gy, gz;
  if (!readMotion(ax, ay, az, gx, gy, gz)) {
    logging = false;
    liveMode = false;
    Serial.println("# Sensor read failed; logging stopped.");
    return;
  }

  if (liveMode) {
    Serial.print(ax);
    Serial.print(',');
    Serial.print(ay);
    Serial.print(',');
    Serial.print(az);
    Serial.print(',');
    Serial.print(gx);
    Serial.print(',');
    Serial.print(gy);
    Serial.print(',');
    Serial.println(gz);
    return;
  }

  Serial.print(millis());
  Serial.print(',');
  Serial.print(ax);
  Serial.print(',');
  Serial.print(ay);
  Serial.print(',');
  Serial.print(az);
  Serial.print(',');
  Serial.print(gx);
  Serial.print(',');
  Serial.print(gy);
  Serial.print(',');
  Serial.print(gz);
  Serial.print(',');
  Serial.println(currentLabel);
}
