#include <LoRa/LoRaPeer.h>

#include <GUI/Display.h>
#include <Device.h>

#include <Utils/StringUtils.h>

#include <set>
#include <sstream>
#include <string>
#include <iostream>

#define RF_FREQUENCY 868000000  // Hz

#define TX_OUTPUT_POWER 20      // dBm

#define LORA_BANDWIDTH 2        // [0: 125 kHz,
                                //  1: 250 kHz,
                                //  2: 500 kHz,
                                //  3: Reserved]
#define LORA_SPREADING_FACTOR 6 // [SF7..SF12]
#define LORA_CODINGRATE 1       // [1: 4/5,
                                //  2: 4/6,
                                //  3: 4/7,
                                //  4: 4/8]
#define LORA_PREAMBLE_LENGTH 4  // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0   // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#define RX_TIMEOUT_VALUE 1000

namespace LoRaDevice
{
    char LoRaPeer::txpacket[BUFFER_SIZE];
    char LoRaPeer::rxpacket[BUFFER_SIZE];
    RadioEvents_t LoRaPeer::RadioEvents;
    int16_t LoRaPeer::Rssi;
    int16_t LoRaPeer::rxSize;
    LoRaPeer::States_t LoRaPeer::state = LoRaPeer::LOWPOWER;
    int LoRaPeer::sentPacketNumber;
    int LoRaPeer::receivedPacketNumber;

    LoRaPeer* LoRaPeer::instance;

    LoRaPeer::LoRaPeer()
    {
        Mcu.begin();

        RadioEvents.TxDone = OnTxDone;
        RadioEvents.TxTimeout = OnTxTimeout;
        RadioEvents.RxDone = OnRxDone;

        Radio.Init(&RadioEvents);
        Radio.SetChannel(RF_FREQUENCY);
        Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                          LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                          LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                          true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

        Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                          LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                          LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                          0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

        sentPacketNumber = 0;
        receivedPacketNumber = 0;

        state = STATE_RX;

        instance = this;
    }

    String LoRaPeer::generateMessageUUID() 
    {
        String uuid = "";

        std::set<char> uniqueChars;
        while (uniqueChars.size() < 6)
        {
            char c = random(48, 122); // Generate a random ASCII value for alphanumeric characters
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                uniqueChars.insert(c);
        }

        for (char c : uniqueChars)
            uuid += c;

        return uuid;
    }

    // TODO: send also the order of the message sent to that particular host, so I can rebuild the order on receival
    void LoRaPeer::sendMessage(const char *msg, const char* destination)
    {
        Message message;
        message.body = msg;
        message.header = "M:" + String(strlen(msg)) + "_" +  Device::getInstance().getUUID();
        message.destination = String(destination);
        message.uuid = generateMessageUUID();
        message.lastTimestamp = millis();
        message.orderNumber = messageCounter++;
        message.retries = 20;

        messagesStatus[message.uuid] = message;
        messageQueue.push(message);
        state = STATE_TX;
    }

    void LoRaPeer::sendMessage(const String &msg, const String& destination)
    {
        sendMessage(msg.c_str(), destination.c_str());
    }

    void LoRaPeer::broadcastMessage(const char *msg)
    {
        Message message;
        message.body = msg;
        message.header = "M:" + String(strlen(msg)) + "_" + Device::getInstance().getUUID();
        message.destination = "B";
        message.uuid = generateMessageUUID();
        message.lastTimestamp = millis();
        message.orderNumber = messageCounter++;
        message.retries = 1;

        messagesStatus[message.uuid] = message;
        messageQueue.push(message);
        state = STATE_TX;
    }

    void LoRaPeer::broadcastMessage(const String &msg)
    {
        broadcastMessage(msg.c_str());
    }

    void LoRaPeer::send(const char *msg)
    {
        strncpy(txpacket, msg, BUFFER_SIZE);
        state = STATE_TX;
    }

    void LoRaPeer::send(const String &msg)
    {
        send(msg.c_str());
    }

    void LoRaPeer::sendHeartbeat() 
    {
        String body = Device::getInstance().getName() + "-" + Device::getInstance().getUUID();
        String message = "H:" + String(body.length()) + "_" + body;
        send(message);
    }

    void LoRaPeer::sendACK(const char *senderID)
    {
        String ack = "ACK:" + String(senderID);
        send(ack);
    }

    void LoRaPeer::sendACK(const String &senderID)
    {
        sendACK(senderID.c_str());
    }

    void LoRaPeer::checkSentMessages()
    {
        const int interval = 5000;
        static int lastTimestamp = millis();

        static int counter = 0;

        if (millis() - lastTimestamp >= interval)
        {
            // if (Device::getInstance().getConnectedPeers().size() > 0)
            // {
            //     auto sender = Utils::StringUtils::parseString(Device::getInstance().getConnectedPeers().begin()->first.c_str(), '-');
            //     sendMessage("test" + String(counter++), sender[1]);
            // }

            // Device::getInstance().btConnection->sendStoredMessage("test123", Device::getInstance().getConnectedPeers().begin()->first);
            for (auto& element : messagesStatus)
                if (element.second.retries > 0)
                {
                    messageQueue.push(element.second);
// #if DEBUG == 1
                    Serial.println();
                    Serial.println("Re-sending Message: " + element.second.body);
// #endif
                    --element.second.retries;
                    element.second.lastTimestamp = millis();
                }

            lastTimestamp = millis();
        }
    }

    void LoRaPeer::loop()
    {
        checkSentMessages();

        switch (state)
        {
            case STATE_TX:
                if (strlen(txpacket) != 0)
                {
#if DEBUG == 1
                    Serial.println("Sending message: " + String(txpacket) + ", Length: " + String(strlen(txpacket)));
#endif
                    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
                    GUI::Display::getInstance().write("Message: " + String(txpacket));
                    sentPacketNumber += 1;
#if DEBUG == 1
                    Serial.println("Resetting txPacket!");
#endif
                    txpacket[0] = '\0';
                    delay(10);
                }

                while (!messageQueue.empty())
                {
                    Message message = messageQueue.back();
                    String messageString = message.header + "_" + message.destination + "_" + message.uuid + "_" + String(message.orderNumber) + "_" + message.body;

                    char packet[BUFFER_SIZE];
                    strncpy(packet, messageString.c_str(), BUFFER_SIZE);
#if DEBUG == 1
                    Serial.println("Sending message: " + String(packet) + ", Length: " + String(strlen(packet)));
#endif
                    Radio.Send((uint8_t *)packet, strlen(packet));
                    GUI::Display::getInstance().append("Message: " + String(packet));
                    sentPacketNumber += 1;
                    messageQueue.pop();
                    delay(10); // Give it time to process all the messages
                }

                GUI::Display::getInstance().clear();
                state = LOWPOWER;
                break;
            case STATE_RX:
                Radio.Rx(0);
                state = LOWPOWER;
                break;
            case LOWPOWER:
                Radio.IrqProcess();
                break;
            default:
                Radio.IrqProcess();
                state = STATE_RX;
                break;
        }
    }

    void LoRaPeer::OnTxDone(void)
    {
        state = STATE_RX;
    }

    void LoRaPeer::OnTxTimeout(void)
    {
        Radio.Sleep();
        state = STATE_TX;
    }

    // TODO: Send received messages via bluetooth to the application
    void LoRaPeer::OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
    {
        Rssi = rssi;
        rxSize = size;
        memcpy(rxpacket, payload, size);
        rxpacket[size] = '\0';
        Radio.Sleep();

        // Serial.printf("\r\nReceived packet \"%s\" with RSSI %d, length %d\r\n", rxpacket, Rssi, rxSize);

        String receivedString = String(rxpacket);
        // Serial.println("Received: " + receivedString);

        if (receivedString.startsWith("H:"))
        {
            // Serial.printf("\r\nReceived packet \"%s\" with RSSI %d, length %d\r\n", rxpacket, Rssi, rxSize);
            // Serial.println("Heartbeat: " + messageBody);
            auto messageBody = receivedString.substring(2);
            auto parsedMessage = Utils::StringUtils::parseUnderscoreString(messageBody, instance->messageDivider);

            String messageLength = parsedMessage[0];
            String peerName = parsedMessage[1];

            if (messageLength.toInt() > peerName.length())
            {
                int peerNameLen = peerName.length();
                Serial.println("Heartbeat needs retransmission! Lengths: " + messageLength + " - " + String(peerNameLen));
            }
            else
                Device::getInstance().refreshPeer(peerName);
        }

        if (receivedString.startsWith("ACK:"))
        {
            auto messageBody = receivedString.substring(4);
            if (instance->messagesStatus.count(messageBody) > 0 && instance->receivedMessages.find(messageBody) == instance->receivedMessages.end())
            {
                Serial.printf("\r\nReceived packet \"%s\" with RSSI %d, length %d\r\n", rxpacket, Rssi, rxSize);
                instance->messagesStatus[messageBody].retries = 0;
                Serial.println("Message -" + messageBody + "- sucsessfuly sent!");
            }
        }

        if (receivedString.startsWith("M:"))
        {
            Serial.printf("\r\nReceived packet \"%s\" with RSSI %d, length %d, size %d\r\n", rxpacket, Rssi, rxSize, size);

            auto messageBody = receivedString.substring(2);
            auto parsedString = Utils::StringUtils::parseString(messageBody.c_str(), instance->messageDivider);

            String messageLength = parsedString[0];
            String senderID = parsedString[1];
            String destinationID = parsedString[2];
            String messageID = parsedString[3];
            String messageOrder = parsedString[4];
            String messageText = "";

            String destinationFullName = "";
            for (const auto &pair : Device::getInstance().getConnectedPeers())
                if (pair.first.indexOf(senderID) != -1)
                    destinationFullName = pair.first;

            MessageType messageType;

            if (destinationID != "B")
            {
                for (int i = 5; i < parsedString.size(); i++)
                    messageText += parsedString[i];

                messageType = MessageType::DIRECT;
            }
            else
            {
                for (int i = 5; i < parsedString.size(); i++)
                    messageText += parsedString[i];

                messageType = MessageType::BROADCAST;
            }

            if (destinationFullName == "")
                return;

            instance->messageCounter++;

            if (messageType == MessageType::DIRECT && Device::getInstance().getUUID().equals(destinationID))
            {
                Serial.println("Message -" + messageID + "- received!");
                Serial.println("Receiver -" + destinationID + "-");
                Serial.println("Length: " + messageLength);
                Serial.println("Order: " + messageOrder);
                Serial.println("Body: " + messageText);

                if (messageLength.toInt() > messageText.length())
                    Serial.println("Message needs retransmission! Lengths: " + messageLength + " - " + String(messageText.length()));
                else if (instance->receivedMessages.find(messageID) == instance->receivedMessages.end())
                {
                    Serial.println("Destination Full Name: " + destinationFullName);
                    Device::getInstance().btConnection->addMessageToQueue(messageText, destinationFullName, messageID, messageOrder.toInt());

                    ReceivedMessage rm;
                    rm.body = messageText;
                    rm.sender = destinationFullName;
                    instance->receivedMessages[messageID] = rm;
                    instance->sendACK(messageID);
                }
            }
            else if (messageType == MessageType::BROADCAST)
            {
                Serial.println("Message -" + messageID + "- received!");
                Serial.println("Length: " + messageLength);
                Serial.println("Body: " + messageText);

                if (messageLength.toInt() > messageText.length())
                    Serial.println("Message needs retransmission! Lengths: " + messageLength + " - " + String(messageText.length()));
                else if (instance->receivedMessages.find(messageID) == instance->receivedMessages.end())
                {
                    Device::getInstance().btConnection->addMessageToQueue(messageText, "ALL", messageID, messageOrder.toInt());

                    ReceivedMessage rm;
                    rm.body = messageText;
                    rm.sender = "ALL";
                    instance->receivedMessages[messageID] = rm;
                    instance->sendACK(messageID);
                }
            }
            else 
            {
                Serial.println("Message is not for me!");
            }
        }

        receivedPacketNumber += 1;

        rxpacket[0] = '\0';
        state = STATE_TX;
    }
}