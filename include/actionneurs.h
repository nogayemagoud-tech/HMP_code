#ifndef ACTIONNEURS_H
#define ACTIONNEURS_H
#include <Arduino.h>

void initActionneurs();
void commanderMoteur(bool actif);
void commanderPompePWM(uint8_t duty);
void commanderAlarme(bool actif); // plus de sortie physique - signalisation logicielle uniquement (ecran/MQTT)

#endif