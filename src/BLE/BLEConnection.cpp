#include <BLE/BLEConnection.h>

#include <BLE/BLECallback.h>

#define DISCONNECT_CMD "disconnect"
#define DEBUG 1

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

namespace BT {
    BLEConnection::BLEConnection() {
        Serial.println("Setting up BLE...");
        // Create the BLE Device
        BLEDevice::init("LoNTRa Device");

        // Create the BLE Server
        BLEServer *pServer = BLEDevice::createServer();

        // Create the BLE Service
        BLEService *pService = pServer->createService(SERVICE_UUID);

        // Create a BLE Characteristic
        BLECharacteristic *pCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ |
                BLECharacteristic::PROPERTY_WRITE |
                BLECharacteristic::PROPERTY_NOTIFY |
                BLECharacteristic::PROPERTY_INDICATE);

        // Set callback for receiving data
        pCharacteristic->setCallbacks(new Callbacks::BLECallback(pServer));

        // Start the service
        pService->start();

        // Start advertising
        pServer->getAdvertising()->start();

        Serial.println("BLE and OLED setup complete.");
    }

    BLEConnection::BLEConnection(const String& deviceName) {
        Serial.println("Setting up BLE...");
        // Create the BLE Device
        BLEDevice::init(deviceName.c_str());

        // Create the BLE Server
        BLEServer *pServer = BLEDevice::createServer();

        // Create the BLE Service
        BLEService *pService = pServer->createService(SERVICE_UUID);

        // Create a BLE Characteristic
        BLECharacteristic *pCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ |
                BLECharacteristic::PROPERTY_WRITE |
                BLECharacteristic::PROPERTY_NOTIFY |
                BLECharacteristic::PROPERTY_INDICATE);

        // Set callback for receiving data
        pCharacteristic->setCallbacks(new Callbacks::BLECallback(pServer));

        // Start the service
        pService->start();

        // Start advertising
        pServer->getAdvertising()->start();

        Serial.println("BLE and OLED setup complete.");
    }
}