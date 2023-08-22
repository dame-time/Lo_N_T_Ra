#include <BLEServer.h>

#include "Arduino.h"

namespace BT::Callbacks {
    class BLECallback : public BLECharacteristicCallbacks
    {
        private:
            BLEServer *pServer;
            uint16_t connId = 0;

            bool isConnectionValid;

            bool isConnected(const String& receivedString);
            bool isConnected(const char* receivedString);
            void connectWithPassword(const String &receivedString, BLECharacteristic *pCharacteristic);

            bool isConnectectedPeersRequest(const char *receivedString, BLECharacteristic *pCharacteristic);

            void printHash(const String &label, byte *hash, int y);
            void logHash(const String &label, byte *hash);
            String hashToString(byte *hash);

            void hashWithHMAC(byte *key, int keyLength, byte *data, int dataLength, byte *hash);

            void disconnectAndRestart();

        public:
            BLECallback(BLEServer *server);

            void onConnect();
            void onWrite(BLECharacteristic *pCharacteristic);
    };
}
