#ifndef EXPANDEUR_UART_H
#define EXPANDEUR_UART_H
#include <Arduino.h>
#include <Stream.h>

void initExpandeurUART(); // demarre le bus I2C partage + le canal DFR0627 vers l'ecran
Stream& uartEcran();

#endif