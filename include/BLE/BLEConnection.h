#pragma once

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#include "Arduino.h"

#include <queue>
#include <vector>
#include <algorithm>

namespace BT {
    class BLEConnection {
    private:
        struct Message
        {
            String payload;
            String id;
            int index;
            int retries;

            Message(const String& p, const String& idx, const int& i) : payload(p), id(idx), index(i) 
            {
                retries = 6;
            }

            bool operator<(const Message &other) const
            {
                return index > other.index;
            }
        };

        BLEServer *pServer;
        BLEService *pService;
        BLECharacteristic *pCharacteristic;

        std::priority_queue<Message> messageToSend;
        std::vector<Message> notConfirmedMessages;

    public:
        static bool isConnected;

        BLEConnection();
        BLEConnection(const String &deviceName);

        void sendData(const String &data);
        void sendStoredMessage(const String& data, const String& destinationChat);
        void addMessageToQueue(const String &data, const String &destinationChat, const String &messageID, const int &index);

        void confirmMessage(const String& messageID);

        void update();
    };
}