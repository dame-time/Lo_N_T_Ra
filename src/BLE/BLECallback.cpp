#include <BLE/BLECallback.h>

#include <BLEDevice.h>
#include <Device.h>

#include <GUI/Display.h>
#include <Crypto.h>
#include <SHA256.h>

#include <Utils/StringUtils.h>

#define DISCONNECT_CMD "disconnect"

namespace BT::Callbacks {
    bool BLECallback::isConnected(const String& receivedString)
    {
        if (receivedString == DISCONNECT_CMD)
            return true;

        return false;
    }
    
    bool BLECallback::isConnected(const char* receivedString)
    {
        if (receivedString == DISCONNECT_CMD)
            return true;

        return false;
    }

    bool BLECallback::isConnectectedPeersRequest(const char *receivedString, BLECharacteristic *pCharacteristic)
    {
        String requestString = String(receivedString);
        if (requestString.startsWith("R:"))
        {
            std::map<String, long> peers = Device::getInstance().getConnectedPeers();
            String peersString = "";
            for (auto &peer : peers)
                peersString += peer.first + ";";

            pCharacteristic->setValue(peersString.c_str());
            pCharacteristic->notify();

            return true;
        }

        return false;
    }

    void BLECallback::connectWithPassword(const String& receivedHexString, BLECharacteristic *pCharacteristic)
    {
        if (receivedHexString.length() < 1) {
            GUI::Display::getInstance().clear();
            GUI::Display::getInstance() += "No password provided!";
            GUI::Display::getInstance() += "Ready to Pair!";

            return;
        }

        byte key[] = {'p', 'r', 'o', 'g', 'e', 't', 't', 'o', '-', 's', 'i', 's', 't', 'e', 'm', 'i', '-', 'o', 'p', 'e', 'r', 'a', 't', 'i', 'v', 'i'};

        byte devicePassword[] = {'d', 'a', 'm', 'e', '-', 't', 'i', 'm', 'e'};
        byte actualPasswordHash[32];
        hashWithHMAC(key, sizeof(key), devicePassword, sizeof(devicePassword), actualPasswordHash);

        logHash("Hashed Password: ", actualPasswordHash);
        Serial.print("Hashed Password String: ");
        String resultingString = hashToString(actualPasswordHash);
        Serial.print(resultingString);
        Serial.println();

        if (receivedHexString == resultingString)
        {
            GUI::Display::getInstance().clear();
            GUI::Display::getInstance() += "Password Correct!";
            GUI::Display::getInstance() += "Connected to device.";

            Serial.print("Password are equals!");

            isConnectionValid = true;

            pCharacteristic->setValue("ACK");
            pCharacteristic->notify();
        }
        else
        {
            GUI::Display::getInstance().clear();
            GUI::Display::getInstance() += "Incorrect Password!";
#if DEBUG == 1
            printHash("Expected: ", actualPasswordHash, 1);
            GUI::Display::getInstance() += "Received: " + receivedHexString;
#endif
            Serial.print("Password are not equals!");

            pCharacteristic->setValue("NACK");
            pCharacteristic->notify();
        }

        Serial.println();
    }

    void BLECallback::printHash(const String &label, byte *hash, int y)
    {
        String hashString = label;
        for (int i = 0; i < 32; i++)
        {
            char buffer[3];
            sprintf(buffer, "%02x", hash[i]);
            hashString += buffer;
            if (i % 4 == 3)
                hashString += " ";
        }

        GUI::Display::getInstance() += hashString;
    }

    void BLECallback::logHash(const String &label, byte *hash)
    {
        Serial.print(label);

        for (int i = 0; i < 32; i++)
        {
            Serial.printf("%02x", hash[i]);
        }

        Serial.println();
    }

    String BLECallback::hashToString(byte *hash)
    {
        String hashString = "";

        for (int i = 0; i < 32; i++)
        {
            char buffer[3];
            sprintf(buffer, "%02x", hash[i]);
            hashString += buffer;
        }

        return hashString;
    }

    void BLECallback::hashWithHMAC(byte *key, int keyLength, byte *data, int dataLength, byte *hash)
    {
        SHA256 sha256;
        byte innerHash[32];
        byte temp[1];

        sha256.reset();
        for (int i = 0; i < keyLength; i++)
        {
            temp[0] = key[i] ^ 0x36;
            sha256.update(temp, 1);
        }
        for (int i = keyLength; i < 64; i++)
        {
            temp[0] = 0x36;
            sha256.update(temp, 1);
        }
        sha256.update(data, dataLength);
        sha256.finalize(innerHash, sizeof(innerHash));

        // Outer Hash
        sha256.reset();
        for (int i = 0; i < keyLength; i++)
        {
            temp[0] = key[i] ^ 0x5C;
            sha256.update(temp, 1);
        }

        for (int i = keyLength; i < 64; i++)
        {
            temp[0] = 0x5C;
            sha256.update(temp, 1);
        }

        sha256.update(innerHash, sizeof(innerHash));
        sha256.finalize(hash, sizeof(innerHash));
    }

    void BLECallback::disconnectAndRestart()
    {
        GUI::Display::getInstance().clear();
        GUI::Display::getInstance() += "Disconnecting...";

        Serial.println("Disconnecting...");

        pServer->disconnect(connId);

        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->stop();
        pAdvertising->start();

        GUI::Display::getInstance().clear();
        GUI::Display::getInstance() += "Ready to Pair!";

        isConnectionValid = false;
        BLEConnection::isConnected = false;

        // Device::getInstance().btConnection = new BLEConnection(Device::getInstance().getName());
    }

    BLECallback::BLECallback(BLEServer *server) : pServer(server)
    {
        isConnectionValid = false;
    }

    void BLECallback::onConnect()
    {
        connId = pServer->m_appId;
    }

    // TODO: Handle all the stuff of the connection outside the write
    void BLECallback::onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string rxValue = pCharacteristic->getValue();
        String receivedHexString = "";
        for (int i = 0; i < rxValue.length(); i++)
        {
            char buffer[3];
            sprintf(buffer, "%02x", (unsigned char)rxValue[i]);
            receivedHexString += buffer;
        }

        if (!isConnectionValid)
        {
            connectWithPassword(receivedHexString, pCharacteristic);
            BLEConnection::isConnected = true;
            return;
        }

        // TODO: For now like this in the future this will be crypted
        if (isConnected(rxValue.c_str()))
        {
            Serial.print("Disconnecting");
            disconnectAndRestart();
            return;
        }

        if (isConnectectedPeersRequest(rxValue.c_str(), pCharacteristic))
            return;
        
        if (String(rxValue.c_str()).startsWith("MACK:"))
        {
            String messageIDAck = String(rxValue.c_str()).substring(5);
            Device::getInstance().btConnection->confirmMessage(messageIDAck);
            return;
        }

        auto parsedString = Utils::StringUtils::parseString(rxValue.c_str(), '_');

        if (parsedString.size() > 1)
        {
            auto parsedDestination = Utils::StringUtils::parseString(rxValue.c_str(), '-');
#if DEBUG == 1
            Serial.println("Destination: " + parsedDestination[1]);
#endif
            Device::getInstance().loraPeer->sendMessage(parsedString[0], parsedDestination[1]);
        }
        else if (parsedString.size() > 0)
            Device::getInstance().loraPeer->broadcastMessage(parsedString[0]);
        else
            Serial.println("Message could not be sent due to bad formatting -" + String(rxValue.c_str()) + "-");
    }
}