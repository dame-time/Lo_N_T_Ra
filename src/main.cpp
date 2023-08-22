#include "Arduino.h"

#include <GUI/Display.h>
#include <Device.h>

void setup()
{
  Serial.begin(9600);
  Serial.println("Initializing OLED...");

  GUI::Display& display = GUI::Display::getInstance();
  Device::getInstance().setName("DameTime");
  Device::getInstance().initialize();

  display += "Initialization Complete...";
  display += "Ready to pair!";

  delay(2000);
}

void loop()
{
  Device::getInstance().update();
}