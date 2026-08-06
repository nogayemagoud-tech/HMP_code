#ifndef BUFFER_SD_H
#define BUFFER_SD_H
#include <Arduino.h>

bool initBufferSD();
bool ecrireMessageBuffer(const char* messageId, const char* topic, const char* payloadJson, const char* priorite);
void marquerMessageConfirme(const char* messageId);
void republierMessagesEnAttente();
void enregistrerDonneesHorsLigne();
void synchroniserDonneesHorsLigne();
uint32_t nombreEnregistrementsEnAttente();
uint8_t pourcentageUtiliseSD();

#endif