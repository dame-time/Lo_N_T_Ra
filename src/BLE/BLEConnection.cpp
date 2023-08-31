#include <BLE/BLEConnection.h>

#include <BLE/BLECallback.h>

#define DISCONNECT_CMD "disconnect"
#define DEBUG 1

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

namespace BT {
    bool BLEConnection::isConnected;

    BLEConnection::BLEConnection() {
        isConnected = false;

        Serial.println("Setting up BLE...");

        BLEDevice::init("LoNTRa Device");

        pServer = BLEDevice::createServer();

        pService = pServer->createService(SERVICE_UUID);

        pCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ |
                BLECharacteristic::PROPERTY_WRITE |
                BLECharacteristic::PROPERTY_NOTIFY |
                BLECharacteristic::PROPERTY_INDICATE);

        BLEDescriptor *pDescriptor = new BLEDescriptor("00002902-0000-1000-8000-00805f9b34fb");
        pCharacteristic->addDescriptor(pDescriptor);

        pCharacteristic->setCallbacks(new Callbacks::BLECallback(pServer));

        pService->start();

        pServer->getAdvertising()->start();

        // Serial.println("BLE and OLED setup complete.");
    }

    BLEConnection::BLEConnection(const String& deviceName) {
        isConnected = false;

        Serial.println("Setting up BLE...");
        BLEDevice::init(deviceName.c_str());

        pServer = BLEDevice::createServer();

        pService = pServer->createService(SERVICE_UUID);

        pCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ |
                BLECharacteristic::PROPERTY_WRITE |
                BLECharacteristic::PROPERTY_NOTIFY |
                BLECharacteristic::PROPERTY_INDICATE);

        BLEDescriptor *pDescriptor = new BLEDescriptor("00002902-0000-1000-8000-00805f9b34fb");
        pCharacteristic->addDescriptor(pDescriptor);

        pCharacteristic->setCallbacks(new Callbacks::BLECallback(pServer));

        pService->start();

        pServer->getAdvertising()->start();

        // Serial.println("BLE and OLED setup complete.");
    }

    void BLEConnection::sendData(const String &data)
    {
        const char *cstr = data.c_str();

        pCharacteristic->setValue(cstr);
        pCharacteristic->notify();
    }

    void BLEConnection::sendStoredMessage(const String& data, const String& destinationChat)
    {
        String payload = destinationChat + "~" + data;
        const char *cstr = payload.c_str();

#if DEBUG == 1
        Serial.println("Transmitting message: " + payload);
#endif

        pCharacteristic->setValue(cstr);
        pCharacteristic->notify();
    }

    void BLEConnection::addMessageToQueue(const String &data, const String &destinationChat, const String& messageID, const int &index)
    {
        String payload = destinationChat + "~" + data + "~" + String(index) + "~" + messageID;
        Message msg = Message(payload, messageID, index);

        const char *cstr = payload.c_str();

#if DEBUG == 1
        Serial.println("Adding to queue: " + payload);
#endif

        messageToSend.push(msg);
    }

    void BLEConnection::confirmMessage(const String& messageID)
    {
        notConfirmedMessages.erase(std::remove_if(notConfirmedMessages.begin(), notConfirmedMessages.end(),
                                                  [&messageID](const Message &msg)
                                                  {
                                                      return msg.id == messageID;
                                                  }),
                                   notConfirmedMessages.end());
    }

    void BLEConnection::update()
    {
        static auto startTime = millis();
        auto currentTime = millis();

        if (currentTime - startTime > 6000)
        {
            notConfirmedMessages.erase(std::remove_if(notConfirmedMessages.begin(), notConfirmedMessages.end(),
                                                      [](const Message &msg)
                                                      {
                                                          return msg.retries == 0;
                                                      }),
                                       notConfirmedMessages.end());

            for (int i = 0; i < notConfirmedMessages.size(); i++)
                if (notConfirmedMessages[i].retries > 0)
                {
                    messageToSend.push(notConfirmedMessages[i]);
                    notConfirmedMessages[i].retries--;
                }

            startTime = millis();
        }

        if (messageToSend.empty() || !isConnected)
            return;

        Message newMessage = messageToSend.top(); // assuming your new message is at the top of messageToSend stack
        auto it = std::find_if(notConfirmedMessages.begin(), notConfirmedMessages.end(),
                               [&newMessage](const Message &msg)
                               {
                                   return msg.id == newMessage.id; // assuming Message struct has an "id" field
                               });

        delay(150);
        sendData(newMessage.payload);

// #if DEBUG == 1
        Serial.println("Sending: " + newMessage.payload);
// #endif

        if (it == notConfirmedMessages.end() && !messageToSend.top().payload.startsWith("ALL"))
            notConfirmedMessages.push_back(messageToSend.top());
            
        messageToSend.pop();

        Serial.flush();
    }
}