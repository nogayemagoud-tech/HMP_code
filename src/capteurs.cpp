#include "capteurs.h"
#include "pins.h"
#include "config.h"

#if defined(SIMULATION) || defined(SCENARIO_MQTT_TEST)

float tempMoteurSimulee = 40.0;
bool defautMoteurSimule = false;
bool defautEnergieSimule = false;
float debitEauSimule = 2.0;
float humiditeFarineSimulee = 12.0;
float tensionReseauSimulee = 230.0;
float courantMoteurSimule = 1.5;
float puissanceSimulee = 350.0;
float energieSimulee = 0.0;
float frequenceReseauSimulee = 50.0;

void initCapteurs() {}

float lireTemperatureMoteur() { return tempMoteurSimulee; }
bool lireDefautMoteur()       { return defautMoteurSimule; }
bool lireDefautEnergie()      { return defautEnergieSimule; }
float lireDebitEau()          { return debitEauSimule; }
float lireHumiditeFarine()    { return humiditeFarineSimulee; }
float lireTensionReseau()     { return tensionReseauSimulee; }
float lireCourantMoteur()     { return courantMoteurSimule; }
float lirePuissance()         { return puissanceSimulee; }
float lireEnergie()           { return energieSimulee; }
float lireFrequenceReseau()   { return frequenceReseauSimulee; }
uint32_t lireImpulsionsTotalDebitmetre() { return 1; }

#else

#include <OneWire.h>
#include <DallasTemperature.h>
#include <PZEM004Tv30.h>

static OneWire oneWire(PIN_DS18B20);
static DallasTemperature capteurTemp(&oneWire);
static PZEM004Tv30 pzem(Serial2, PIN_PZEM_RX, PIN_PZEM_TX);

static volatile uint32_t compteurImpulsions = 0;
static uint32_t dernierComptage = 0;
static uint32_t dernierTempsMs = 0;

void IRAM_ATTR isrDebitmetre() {
  compteurImpulsions++;
}

void initCapteurs() {
  capteurTemp.begin();

  pinMode(PIN_DEBITMETRE, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_DEBITMETRE), isrDebitmetre, RISING);
  dernierTempsMs = millis();

  pinMode(PIN_DEFAUT_MOTEUR, INPUT_PULLUP);

  Serial.println("[CAPTEURS] DS18B20, debitmetre, PZEM et defaut moteur initialises");
}

float lireTemperatureMoteur() {
  capteurTemp.requestTemperatures();
  float t = capteurTemp.getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C) return NAN;
  return t;
}

bool lireDefautMoteur() {
  return digitalRead(PIN_DEFAUT_MOTEUR) == LOW;
}

bool lireDefautEnergie() {
  float v = pzem.voltage();
  return isnan(v);
}

float lireDebitEau() {
  uint32_t maintenant = millis();
  uint32_t impulsions = compteurImpulsions - dernierComptage;
  uint32_t deltaMs = maintenant - dernierTempsMs;
  dernierComptage = compteurImpulsions;
  dernierTempsMs = maintenant;

  if (deltaMs == 0) return 0.0f;
  float litresEcoules = impulsions / DEBITMETRE_PULSES_PAR_LITRE;
  float minutesEcoulees = deltaMs / 60000.0f;
  return litresEcoules / minutesEcoulees;
}

uint32_t lireImpulsionsTotalDebitmetre() { return compteurImpulsions; }

float lireHumiditeFarine() {
  return NAN;
}

float lireTensionReseau()   { return pzem.voltage(); }
float lireCourantMoteur()   { return pzem.current(); }
float lirePuissance()       { return pzem.power(); }
float lireEnergie()         { return pzem.energy(); }
float lireFrequenceReseau() { return pzem.frequency(); }

#endif