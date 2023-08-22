#pragma once

#include <BLEDevice.h>
#include <BLEServer.h>

#include "Arduino.h"

namespace BT {
    class BLEConnection {
        public:
            BLEServer* pServer;
            BLEService* pService;

            BLEConnection();
            BLEConnection(const String& deviceName);
    };
}