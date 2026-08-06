#ifndef ETAT_H
#define ETAT_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "recette.h"

enum EtatMachine {
  ETAT_DEMARRAGE,
  ETAT_INITIALISATION,
  ETAT_MACHINE_PRETE,
  ETAT_CONFIGURATION,
  ETAT_PRODUCTION,
  ETAT_FIN_PRODUCTION,
  ETAT_HISTORISATION,
  ETAT_ARRET_DISTANT, 
  ETAT_ALERTE_CONFIG,
  ETAT_ARRET_SECURISE,
  ETAT_ALERTE_ACQUITTEMENT,
  ETAT_MAINTENANCE
};

enum CauseArret {
  CAUSE_AUCUNE,
  CAUSE_TEMPERATURE_MOTEUR,
  CAUSE_DEFAUT_MOTEUR,
  CAUSE_DEFAUT_ENERGIE,
  CAUSE_MANQUE_EAU,
  CAUSE_ARRET_DISTANT,
  CAUSE_RECETTE_INVALIDE,
  CAUSE_CAPTEUR_TEMP_INVALIDE,
  CAUSE_DEBITMETRE_DECONNECTE
};

enum ModeFonctionnement {
  MODE_AUTOMATIQUE,
  MODE_MANUEL,
  MODE_MAINTENANCE
};

struct DonneesPartagees {
  float humiditeFarine = 0;
  float debitEau = 0;
  float tempMoteur = 0;
  float tensionReseau = 0;
  float courantMoteur = 0;
  float puissance = 0;
  float energie = 0;
  float frequenceReseau = 0;
  bool defautMoteur = false;
  bool defautEnergie = false;

  bool publicationAlerteDemandee = false;
  char nomAlerteEnAttente[40] = "";
  char champsAlerteEnAttente[160] = "";
  bool alerteCritiqueEnAttente = false;

  EtatMachine etat = ETAT_DEMARRAGE;
  bool publicationEtatDemandee = false;
  bool publicationProductionDemandee = false;
  bool defautCritique = false;
  bool alerteActive = false;
  CauseArret causeArret = CAUSE_AUCUNE;

  bool pompeActive = false;
  bool moteurActif = false;
  bool pompeEnAmorcage = false;
  uint8_t pwmPompe = 0;

  // Aucune recette n'est imposée au premier démarrage.
  MatierePremiere matiereSelectionnee = MATIERE_NON_SELECTIONNEE;
  ProduitFinal produitSelectionne = PRODUIT_NON_SELECTIONNE;

  ModeFonctionnement modeFonctionnement = MODE_AUTOMATIQUE;
  char dateProchaineMaintenance[11] = "";
  bool publicationContexteDemandee = true;

  float debitDoseuse_kgMin = 2.0;
  float debitEauCible = 0;
  float quantiteEauTotaleInjectee = 0;
  float eauInjecteeJournaliere_L = 0;
  float productionJournaliere_kg = 0;
  uint32_t tempsTravailJournalier_s = 0;
  int dernierJourConnu = -1;

  uint32_t debutProduction = 0;
  bool ecartDebitDetecte = false;

  bool demandeDemarrage = false;
  bool demandeStop = false;
  bool demandeStopDistant = false;
  bool arretDistantActif = false;
  bool acquittementLocalRequis = false;
  char commandIdStopEnAttente[48] = "";
  bool confirmationStopDemandee = false;
  bool demandeAcquittement = false;

  bool connexionDisponible = false;
};

extern DonneesPartagees donnees;
extern SemaphoreHandle_t mutexDonnees;

extern volatile uint32_t heartbeatSecurite;
extern volatile uint32_t heartbeatCommande;
extern volatile uint32_t heartbeatAcquisition;
extern volatile uint32_t heartbeatCalcul;

// Utilisée par main.cpp et comm.cpp après une modification de recette.
void sauvegarderDonneesPersistantes();

#endif