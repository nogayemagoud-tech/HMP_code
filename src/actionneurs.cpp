#include "actionneurs.h"
#include "config.h"
#include "pins.h"

void initActionneurs() {
  pinMode(PIN_COMMANDE_MOTEUR, OUTPUT);
  digitalWrite(PIN_COMMANDE_MOTEUR, !NIVEAU_ACTIF_MOTEUR);

  ledcSetup(CANAL_PWM_POMPE, FREQUENCE_PWM_POMPE_HZ, RESOLUTION_PWM_BITS);
  ledcAttachPin(PIN_PWM_POMPE, CANAL_PWM_POMPE);
  ledcWrite(CANAL_PWM_POMPE, 0);

  Serial.println("[ACTIONNEUR] Moteur et pompe initialises a OFF");
}

void commanderMoteur(bool actif) {
  digitalWrite(PIN_COMMANDE_MOTEUR, actif ? NIVEAU_ACTIF_MOTEUR : !NIVEAU_ACTIF_MOTEUR);
}

void commanderPompePWM(uint8_t duty) {
  ledcWrite(CANAL_PWM_POMPE, duty);
}

void commanderAlarme(bool actif) {
  // Pas de buzzer/voyant physique - l'etat d'alerte est deja porte par
  // donnees.alerteActive et donnees.causeArret, transmis via publierEtat() (MQTT)
  // et affichable sur l'ecran DWIN. Cette fonction ne fait rien de plus
  // qu'un log, conservee pour ne pas casser les appels existants dans Task_Securite.
  if (actif) Serial.println("[SIGNALISATION] Etat d'alerte actif (ecran/MQTT)");
}