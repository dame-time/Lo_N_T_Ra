#include <LoRa/LoRaPeer.h>

#include <GUI/Display.h>
#include <Device.h>

#include <set>
#include <sstream>
#include <string>
#include <iostream>

#define RF_FREQUENCY 868000000 // Hz

#define TX_OUTPUT_POWER 5 // dBm

#define LORA_BANDWIDTH 0        // [0: 125 kHz,
                                //  1: 250 kHz,
                                //  2: 500 kHz,
                                //  3: Reserved]
#define LORA_SPREADING_FACTOR 7 // [SF7..SF12]
#define LORA_CODINGRATE 1       // [1: 4/5,
                                //  2: 4/6,
                                //  3: 4/7,
                                //  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8  // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0   // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#define RX_TIMEOUT_VALUE 1000

namespace LoRa 
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

    void LoRaPeer::sendMessage(const char *msg)
    {
        Message message;
        message.body = msg;
        message.header = "M:" +  Device::getInstance().getUUID();
        message.uuid = generateMessageUUID();
        message.lastTimestamp = millis();
        message.retries = 3;

        messagesStatus[message.uuid] = message;
        messageQueue.push(message);
        state = STATE_TX;
    }

    void LoRaPeer::sendMessage(const String &msg)
    {
        sendMessage(msg.c_str());
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
        String message = "H:" + Device::getInstance().getName() + "_" + Device::getInstance().getUUID();
        send(message);
    }

    void LoRaPeer::sendACK(const char *senderID)
    {
        String ack = "ACK" + String(senderID);
        send(ack);
    }

    void LoRaPeer::sendACK(const String &senderID)
    {
        sendACK(senderID.c_str());
    }

    void LoRaPeer::checkSentMessages()
    {
        const int interval = 8000;
        static int lastTimestamp = millis();

        if (millis() - lastTimestamp >= interval)
        {
            for (auto& element : messagesStatus)
                if (element.second.retries > 0)
                {
                    messageQueue.push(element.second);
                    Serial.println();
                    Serial.println("Re-sending Message: " + element.second.body);
                    --element.second.retries;
                    element.second.lastTimestamp = millis();
                }

            lastTimestamp = millis();
        }
    }

    // TODO: Handle the queue trnasmission of things properly
    void LoRaPeer::loop()
    {
        checkSentMessages();

        switch (state)
        {
            case STATE_TX:
                if (strlen(txpacket) != 0)
                {
                    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
                    GUI::Display::getInstance().write("Message: " + String(txpacket));
                    sentPacketNumber += 1;
                }

                while (!messageQueue.empty())
                {
                    Message message = messageQueue.back();
                    String messageString = message.header + "_" + message.uuid + "_" + message.body;
                    char packet[BUFFER_SIZE];
                    strncpy(packet, messageString.c_str(), BUFFER_SIZE);
                    Serial.println("Sending message: " + String(packet) + ", Length: " + String(strlen(packet)));
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
                // Serial.println("into RX mode");
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
            auto messageBody = receivedString.substring(2);
            // Serial.println("Heartbeat: " + messageBody);
            Device::getInstance().refreshPeer(messageBody);
        }

        if (receivedString.startsWith("ACK:"))
        {
            Serial.printf("\r\nReceived packet \"%s\" with RSSI %d, length %d\r\n", rxpacket, Rssi, rxSize);
            auto messageBody = receivedString.substring(4);
            instance->messagesStatus[messageBody].retries = 0;
            Serial.println("Message -" + messageBody + "- sucsessfuly sent!");
        }

        if (receivedString.startsWith("M:"))
        {
            Serial.printf("\r\nReceived packet \"%s\" with RSSI %d, length %d\r\n", rxpacket, Rssi, rxSize);

            auto messageBody = receivedString.substring(2);
            std::string messageBodyString = messageBody.c_str();
            std::stringstream ss(messageBodyString);
            std::string token;
            int i = 0;

            String messageText = "";
            String senderID = "";
            String messageID = "";

            while (std::getline(ss, token, '_'))
            {
                auto splittedString = token.c_str();

                if (i == 0)
                    senderID = splittedString;
                else if (i == 1)
                    messageID = splittedString;
                else
                    messageText = splittedString;

                ++i;
            }

            Serial.println("Message -" + messageID + "- received!");
            Serial.println("Body: " + messageText);

            instance->sendACK(messageID);
        }

        receivedPacketNumber += 1;

        state = STATE_TX;
    }
}