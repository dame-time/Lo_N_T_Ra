#include <Device.h>

#include <set>

Device* Device::instance = nullptr;

Device::Device() 
{
    randomSeed(analogRead(0));
    mapMutex = xSemaphoreCreateMutex();

    // FIXME: That is not safe I should Improve it
    for (int i = 0; i < KEY_LENGTH; i++)
        key[i] = random(0, 256);

    aes.setKey(key, KEY_LENGTH * 8);
}

void Device::initialize() 
{
    name = name == "" ? "LoNTRa_0" : name;

    btConnection = new BT::BLEConnection(name);
    loraPeer = new LoRa::LoRaPeer();

    connectedPeers.clear();

    std::set<char> uniqueChars;
    while (uniqueChars.size() < 6)
    {                             
        char c = random(48, 122); // Generate a random ASCII value for alphanumeric characters
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            uniqueChars.insert(c);
    }

    for (char c : uniqueChars)
        uuid += c;

#if DEBUG == 1
    loraPeer->sendMessage("My Ass is blue");
#endif

    xTaskCreate(heartbeatTaskWrapper, "Heartbeat Task", 10000, this, 1, nullptr);
}

Device& Device::getInstance() 
{
    if (instance == nullptr)
        instance = new Device();

    return *instance;
}

void Device::setName(const String& name)
{
    auto tmpName = name;

    if (name.length() > 8)
        tmpName = tmpName.substring(0, 7);

    this->name = tmpName;
}

void Device::setName(const char* name)
{
    setName(String(name));
}

String Device::getName() 
{
    return name;
}

String Device::getUUID()
{
    return uuid;
}

std::map<String, long> Device::getConnectedPeers()
{
    return this->connectedPeers;
}

void Device::refreshPeer(const String &peerName)
{
    if (xSemaphoreTake(mapMutex, portMAX_DELAY))
    {
        connectedPeers[peerName] = millis();
        xSemaphoreGive(mapMutex);
        // printMap();    
    }
}

void Device::printMap()
{
    Serial.println("Connected Peers:");
    for (const auto &peer : connectedPeers)
    {
        Serial.print("Peer: ");
        Serial.print(peer.first);
        Serial.print(", Time: ");
        Serial.println(peer.second);
    }
}

void Device::refreshPeer(const char *peerName)
{
    refreshPeer(String(peerName));
}

// FIXME: It works but its a bit buggy
void Device::clearDeadPeers()
{
    if (xSemaphoreTake(mapMutex, portMAX_DELAY))
    {
#if DEBUG == 1
        Serial.println("========= Checking for dead peers... =========");
#endif

        for (auto it = connectedPeers.begin(); it != connectedPeers.end();)
            if (millis() - it->second > heartbeatInterval)
            {
                Serial.println("Succesfully deleted a dead peer: " + it->first);
                Serial.println("With millis: " + String(it->second));
                Serial.println("Difference: " + String(millis() - it->second));
                Serial.println("Map Len: " + String(connectedPeers.size()));
                it = connectedPeers.erase(it);
            }
            else
                ++it;

#if DEBUG == 1
        Serial.println("========= Check Done =========");
#endif
        xSemaphoreGive(mapMutex);
    }
}

void Device::heartbeatTaskWrapper(void *parameter)
{
    Device *device = static_cast<Device *>(parameter);
    device->heartbeatTask(parameter);
}

void Device::heartbeatTask(void *parameter)
{
    Device *device = static_cast<Device *>(parameter);

    while (true)
    {
        unsigned long currentMillis = millis();
        if (currentMillis - device->lastHeartbeatTime >= device->heartbeatInterval)
        {
            device->loraPeer->sendHeartbeat();
            device->clearDeadPeers();
            device->lastHeartbeatTime = currentMillis;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Delay to allow other tasks to run
    }
}

// TODO: Check if it works
void Device::encrypt(uint8_t *data, uint8_t *encrypted)
{
    aes.encryptBlock(encrypted, data);
}

// TODO: Check if it works
void Device::decrypt(uint8_t *encrypted, uint8_t *decrypted)
{
    aes.decryptBlock(decrypted, encrypted);
}

void Device::update() {
    loraPeer->loop();
}