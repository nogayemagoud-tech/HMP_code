#include "expandeur_uart.h"
#include "pins.h"

#ifndef SIMULATION

#include <Wire.h>
#include <DFRobot_IICSerial.h>

// IA1/IA0 correspondent aux switches DIP physiques du module DFR0627 - a verifier
DFRobot_IICSerial canalEcran(Wire, SUBUART_CHANNEL_1, /*IA1*/1, /*IA0*/1);

void initExpandeurUART() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  canalEcran.begin(115200);
  Serial.println("[EXPANDEUR] DFR0627 initialise (canal ecran)");
}

Stream& uartEcran() { return canalEcran; }

#else

void initExpandeurUART() {}
Stream& uartEcran() { static HardwareSerial d(0); return d; }

#endif