#pragma once

#include "Arduino.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <AES.h>
#include <Crypto.h>

#include <BLE/BLEConnection.h>
#include <LoRa/LoRaPeer.h>

#include <map>

#define KEY_LENGTH 16 // AES-128

class Device 
{
    private:
        String name = "";
        String uuid = "";

        uint8_t key[KEY_LENGTH];
        AES128 aes;

        std::map<String, long> connectedPeers;

        SemaphoreHandle_t mapMutex;

        static Device* instance;

        unsigned long lastHeartbeatTime = 0;
        const long heartbeatInterval = 10000;

        Device();

        void clearDeadPeers();

        void printMap();

        static void heartbeatTaskWrapper(void *parameter);
        void heartbeatTask(void* parameter);

    public:
        BT::BLEConnection* btConnection;
        LoRa::LoRaPeer* loraPeer;

        static Device& getInstance();

        void setName(const String& name);
        void setName(const char* name);

        String getName();
        String getUUID();

        void encrypt(uint8_t *data, uint8_t *encrypted);
        void decrypt(uint8_t *encrypted, uint8_t *decrypted);

        std::map<String, long> getConnectedPeers();
        void refreshPeer(const String& peerName);
        void refreshPeer(const char* peerName);

        void initialize();
        void update();
};