#include <Wire.h>
#include <Wrist_Rehabilitation_Monitor_inferencing.h>

#if defined(ESP8266)
constexpr uint8_t SDA_PIN = D2; // GPIO4
constexpr uint8_t SCL_PIN = D1; // GPIO5
#else
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;
#endif

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t SAMPLE_INTERVAL_US = 20000; // 50 Hz
constexpr float MIN_CONFIDENCE = 0.80f;

constexpr uint8_t MPU_ADDRESS_0 = 0x68;
constexpr uint8_t MPU_ADDRESS_1 = 0x69;
constexpr uint8_t REG_WHO_AM_I = 0x75;
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_GYRO_CONFIG = 0x1B;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;

float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
uint8_t mpuAddress = MPU_ADDRESS_0;
size_t featureIndex = 0;
uint32_t nextSampleUs = 0;

bool readRegisters(uint8_t reg, uint8_t *buffer, uint8_t length) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(mpuAddress, length, true) != length) {
    return false;
  }

  for (uint8_t i = 0; i < length; ++i) {
    buffer[i] = Wire.read();
  }

  return true;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
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

int rawFeatureGetData(size_t offset, size_t length, float *outPtr) {
  memcpy(outPtr, features + offset, length * sizeof(float));
  return 0;
}

bool findMpu6050() {
  uint8_t whoAmI = 0;

  mpuAddress = MPU_ADDRESS_0;
  if (readRegisters(REG_WHO_AM_I, &whoAmI, 1) &&
      (whoAmI == MPU_ADDRESS_0 || whoAmI == MPU_ADDRESS_1)) {
    return true;
  }

  mpuAddress = MPU_ADDRESS_1;
  return readRegisters(REG_WHO_AM_I, &whoAmI, 1) &&
         (whoAmI == MPU_ADDRESS_0 || whoAmI == MPU_ADDRESS_1);
}

void printPrediction(const ei_impulse_result_t &result) {
  uint16_t bestIndex = 0;
  float bestScore = 0.0f;

  Serial.println();
  Serial.println("Predictions:");

  for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i) {
    const float score = result.classification[i].value;
    Serial.print("  ");
    Serial.print(result.classification[i].label);
    Serial.print(": ");
    Serial.println(score, 4);

    if (score > bestScore) {
      bestScore = score;
      bestIndex = i;
    }
  }

  Serial.print("Timing: DSP ");
  Serial.print(result.timing.dsp);
  Serial.print(" ms, inference ");
  Serial.print(result.timing.classification);
  Serial.println(" ms");

  Serial.print("Decision: ");
  if (bestScore >= MIN_CONFIDENCE) {
    Serial.print(result.classification[bestIndex].label);
    Serial.print(" (");
    Serial.print(bestScore * 100.0f, 1);
    Serial.println("%)");
  } else {
    Serial.println("uncertain");
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  Serial.println();
  Serial.println("Wrist rehabilitation Edge Impulse inference");
  Serial.print("Expected features: ");
  Serial.println(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  Serial.print("Expected frequency: ");
  Serial.println(EI_CLASSIFIER_FREQUENCY);

  if (!findMpu6050()) {
    Serial.println("MPU-6050 was not detected at 0x68 or 0x69.");
    while (true) {
      delay(1000);
    }
  }

  if (!writeRegister(REG_PWR_MGMT_1, 0x00) ||
      !writeRegister(REG_ACCEL_CONFIG, 0x00) ||
      !writeRegister(REG_GYRO_CONFIG, 0x00)) {
    Serial.println("MPU-6050 initialization failed.");
    while (true) {
      delay(1000);
    }
  }

  delay(100);
  nextSampleUs = micros();
  Serial.println("Ready. Keep the IMU mounted in the training orientation.");
}

void loop() {
  const uint32_t nowUs = micros();
  if (static_cast<int32_t>(nowUs - nextSampleUs) < 0) {
    return;
  }
  nextSampleUs += SAMPLE_INTERVAL_US;

  int16_t ax, ay, az, gx, gy, gz;
  if (!readMotion(ax, ay, az, gx, gy, gz)) {
    Serial.println("Sensor read failed; restarting the sample window.");
    featureIndex = 0;
    delay(50);
    return;
  }

  features[featureIndex++] = static_cast<float>(ax);
  features[featureIndex++] = static_cast<float>(ay);
  features[featureIndex++] = static_cast<float>(az);
  features[featureIndex++] = static_cast<float>(gx);
  features[featureIndex++] = static_cast<float>(gy);
  features[featureIndex++] = static_cast<float>(gz);

  if (featureIndex < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
    return;
  }

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  signal.get_data = rawFeatureGetData;

  ei_impulse_result_t result = {};
  const EI_IMPULSE_ERROR error = run_classifier(&signal, &result, false);
  if (error != EI_IMPULSE_OK) {
    Serial.print("Inference failed with error: ");
    Serial.println(static_cast<int>(error));
  } else {
    printPrediction(result);
  }

  featureIndex = 0;
}
