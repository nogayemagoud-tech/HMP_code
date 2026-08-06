#ifndef CAPTEURS_H
#define CAPTEURS_H
#include <Arduino.h>

#ifdef SIMULATION
extern float tempMoteurSimulee;
extern bool defautMoteurSimule;
extern bool defautEnergieSimule;
extern float debitEauSimule;
extern float humiditeFarineSimulee;
extern float tensionReseauSimulee;
extern float courantMoteurSimule;
extern float puissanceSimulee;
extern float energieSimulee;
extern float frequenceReseauSimulee;
#endif

void initCapteurs(); // a appeler dans setup(), demarre OneWire/PZEM/interruption debitmetre

float lireTemperatureMoteur();
bool lireDefautMoteur();
bool lireDefautEnergie();
float lireDebitEau();
float lireHumiditeFarine();
float lireTensionReseau();
float lireCourantMoteur();
float lirePuissance();
float lireEnergie();
float lireFrequenceReseau();

uint32_t lireImpulsionsTotalDebitmetre();


#endif