#ifndef PROTOCOLE_MQTT_H
#define PROTOCOLE_MQTT_H
#include <Arduino.h>

enum PrioriteMessage { PRIORITE_BASSE, PRIORITE_MOYENNE, PRIORITE_HAUTE, PRIORITE_CRITIQUE };

const char* prioriteVersChaine(PrioriteMessage p);
void genererMessageId(char* out, size_t tailleOut, uint32_t* seqOut);
void obtenirHorodatage(char* out, size_t tailleOut);
void construireTopic(const char* categorie, const char* parametre, char* out, size_t tailleOut);

void envoyerMesure(const char* parametre, float valeur, const char* unite, PrioriteMessage priorite);
void envoyerAlerte(const char* nomAlerte, const char* jsonChampsSupplementaires, PrioriteMessage priorite);
void envoyerEtatMachine();
void envoyerProduction(const char* sousTopic, const char* jsonChampsSupplementaires);
void envoyerAckCommande(const char* commandId, const char* commande, const char* statut, const char* message);

void traiterAckEntrant(const char* messageId);

#endif