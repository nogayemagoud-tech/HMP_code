#ifndef COMM_H
#define COMM_H
#include <Arduino.h>

void initComm();
bool connecterReseauCellulaire();
bool connecterMQTT();
void bouclerMQTT();
bool estConnecteMQTT();
bool publierBrut(const char* topic, const char* payload);
bool appliquerMiseAJourOTA(const char* url);

#endif