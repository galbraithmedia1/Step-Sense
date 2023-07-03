
#include <Wire.h>
#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define BUTTON_PIN 2
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define STEP_DATA_CHAR_UUID "beefcafe-36e1-4688-b7f5-00000000000b"

BLECharacteristic *pStepDataCharacteristic;

const int MPU_ADDR = 0x68; // I2C address of MPU-6050
int16_t accelX, accelY, accelZ;
int stepCount = 0;
float accMagnitudePrev = 0;


void resetEEPROM() {
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  stepCount = 0;
  Serial.println("EEPROM reset");
}

void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  Serial.begin(9600);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  delay(2000);

    // Create BLE device, server, and service
  BLEDevice::init("Step-Sense");
  
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create step data characteristic
  pStepDataCharacteristic = pService->createCharacteristic(
      STEP_DATA_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pStepDataCharacteristic->addDescriptor(new BLE2902());

  // Start BLE server and advertising
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE device is ready to be connected");


  pinMode(BUTTON_PIN, INPUT_PULLUP);

  EEPROM.begin(sizeof(int));
  EEPROM.get(0, stepCount);

}

void loop() {
  // put your main code here, to run repeatedly:
  if (digitalRead(BUTTON_PIN) == LOW) {
    resetEEPROM();
    // delay(1000); // Debouncing the button press
  }
  readAccelerometerData();
  detectStep();
  displayStepCount();
  delay(100);

}

void readAccelerometerData() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // Starting register for accelerometer data
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  accelX = Wire.read() << 8 | Wire.read();
  accelY = Wire.read() << 8 | Wire.read();
  accelZ = Wire.read() << 8 | Wire.read();
}

void saveStepCount() {
  // Save stepCount to EEPROM
  EEPROM.put(0, stepCount);
  EEPROM.commit();
}

void detectStep() {
  float accX = accelX / 16384.0; 
  float accY = accelY / 16384.0;
  float accZ = accelZ / 16384.0;

  // Calculate the magnitude of acceleration
  // accX * accX is equivalent to pow(accX, 2)
  float accMagnitude = sqrt(accX * accX + accY * accY + accZ * accZ);
  // float accMagnitude = sqrt(accX * accX + accY * accY + accZ * accZ);: This line calculates the magnitude of the acceleration vector using the calculated acceleration values for the X, Y, and Z axes. The sqrt() function is used to calculate the square root of the sum of the squared acceleration values.
  
  // Peak detection
  if (accMagnitudePrev > accMagnitude + 0.1 && accMagnitudePrev > 1.5) {
    stepCount++;
    saveStepCount();
      // Create a string containing the step count data
    String stepData = String(stepCount);
    // Update the characteristic value
    pStepDataCharacteristic->setValue(stepData.c_str());
    pStepDataCharacteristic->notify();
  }
  accMagnitudePrev = accMagnitude;
}

void displayStepCount() {
  Serial.print("Steps: ");
  Serial.println(stepCount);
}