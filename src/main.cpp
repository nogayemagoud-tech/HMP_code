#include <Arduino.h>
#include "capteurs.h"
#include "actionneurs.h"
#include "etat.h"
#include "recette.h"
#include "config.h"
#include "pins.h"
#include "dgus.h"
#include "comm.h"
#include "buffer_sd.h"
#include "protocole_mqtt.h"
#include <math.h>
#include <Preferences.h>
#include <time.h>
#include "expandeur_uart.h"


DonneesPartagees donnees;
SemaphoreHandle_t mutexDonnees;

volatile uint32_t heartbeatSecurite = 0;
volatile uint32_t heartbeatCommande = 0;
volatile uint32_t heartbeatAcquisition = 0;
volatile uint32_t heartbeatCalcul = 0;

TaskHandle_t hSecurite, hAcquisition, hCalcul, hCommande, hHmi, hMqtt, hBufferOffline, hWatchdog;

Preferences prefs;

void chargerDonneesPersistantes();
void sauvegarderDonneesPersistantes();
void verifierNouveauJour();

const char *causeArretTexte(CauseArret c) {
  switch (c) {
    case CAUSE_TEMPERATURE_MOTEUR: return "temperature moteur critique";
    case CAUSE_DEFAUT_MOTEUR:      return "defaut moteur";
    case CAUSE_DEFAUT_ENERGIE:     return "defaut energie";
    case CAUSE_MANQUE_EAU:         return "absence de debit d'eau";
    case CAUSE_ARRET_DISTANT:      return "arret distant";
    case CAUSE_RECETTE_INVALIDE:   return "recette indisponible";
    case CAUSE_CAPTEUR_TEMP_INVALIDE: return "capteur temperature invalide/deconnecte";
    case CAUSE_DEBITMETRE_DECONNECTE: return "debitmetre probablement deconnecte (aucune impulsion)";
    default: return "aucune";
  }
}

void chargerDonneesPersistantes() {
  static constexpr uint32_t VERSION_RECETTE = 1;

  prefs.begin("hmp", false);

  const float eauJour = prefs.getFloat("eauJour", 0.0f);
  const float prodJour = prefs.getFloat("prodJour", 0.0f);
  const uint32_t versionRecette = prefs.getUInt("recVer", 0);

  int matiereSauvegardee = MATIERE_NON_SELECTIONNEE;
  int produitSauvegarde = PRODUIT_NON_SELECTIONNE;

  // Les anciennes valeurs étaient basées sur des enums différents.
  // Elles sont ignorées tant que la nouvelle version n'a pas été écrite.
  if (versionRecette == VERSION_RECETTE) {
    matiereSauvegardee =
      prefs.getInt("matiere", MATIERE_NON_SELECTIONNEE);

    produitSauvegarde =
      prefs.getInt("produit", PRODUIT_NON_SELECTIONNE);
  }

  if (matiereSauvegardee < MATIERE_NON_SELECTIONNEE ||
      matiereSauvegardee > MATIERE_RIZ) {
    matiereSauvegardee = MATIERE_NON_SELECTIONNEE;
  }

  if (produitSauvegarde < PRODUIT_NON_SELECTIONNE ||
      produitSauvegarde > PRODUIT_THIERE) {
    produitSauvegarde = PRODUIT_NON_SELECTIONNE;
  }

  xSemaphoreTake(mutexDonnees, portMAX_DELAY);

  donnees.eauInjecteeJournaliere_L = eauJour;
  donnees.productionJournaliere_kg = prodJour;
  donnees.matiereSelectionnee =
    static_cast<MatierePremiere>(matiereSauvegardee);
  donnees.produitSelectionne =
    static_cast<ProduitFinal>(produitSauvegarde);
    bool verrouDistant =
  prefs.isKey("stopDist")
    ? prefs.getBool("stopDist", false)
    : false;

donnees.arretDistantActif = verrouDistant;

if (verrouDistant) {
  donnees.etat = ETAT_ARRET_DISTANT;
  donnees.demandeDemarrage = false;
  donnees.demandeStop = false;
  donnees.pompeActive = false;
  donnees.moteurActif = false;
  donnees.pompeEnAmorcage = false;
  donnees.pwmPompe = 0;
}

  xSemaphoreGive(mutexDonnees);

  Serial.printf(
    "[PERSISTANCE] Recette restauree : matiere=%d produit=%d\n",
    matiereSauvegardee,
    produitSauvegarde
  );
}

void sauvegarderDonneesPersistantes() {
  static constexpr uint32_t VERSION_RECETTE = 1;

  float eauJour;
  float prodJour;
  MatierePremiere matiere;
  ProduitFinal produit;

  xSemaphoreTake(mutexDonnees, portMAX_DELAY);

  eauJour = donnees.eauInjecteeJournaliere_L;
  prodJour = donnees.productionJournaliere_kg;
  matiere = donnees.matiereSelectionnee;
  produit = donnees.produitSelectionne;

  xSemaphoreGive(mutexDonnees);

  prefs.putFloat("eauJour", eauJour);
  prefs.putFloat("prodJour", prodJour);
  prefs.putInt("matiere", static_cast<int>(matiere));
  prefs.putInt("produit", static_cast<int>(produit));
  prefs.putUInt("recVer", VERSION_RECETTE);
  prefs.putBool(
  "stopDist",
  donnees.arretDistantActif
);

  Serial.printf(
    "[PERSISTANCE] Recette sauvegardee : matiere=%d produit=%d\n",
    static_cast<int>(matiere),
    static_cast<int>(produit)
  );
}

void verifierNouveauJour() {
  time_t maintenant = time(nullptr);

  struct tm infosDate;
  localtime_r(&maintenant, &infosDate);

  int anneeReelle = infosDate.tm_year + 1900;

  if (anneeReelle < ANNEE_MINIMALE_VALIDE) {
    return;
  }

  int jourCourant = anneeReelle * 400 + infosDate.tm_yday;
  bool compteursReinitialises = false;

  xSemaphoreTake(mutexDonnees, portMAX_DELAY);

  if (donnees.dernierJourConnu == -1) {
    // Première synchronisation de la date
    donnees.dernierJourConnu = jourCourant;
  }
  else if (donnees.dernierJourConnu != jourCourant) {
    donnees.eauInjecteeJournaliere_L = 0.0f;
    donnees.productionJournaliere_kg = 0.0f;
    donnees.tempsTravailJournalier_s = 0;

    donnees.dernierJourConnu = jourCourant;
    compteursReinitialises = true;
  }

  xSemaphoreGive(mutexDonnees);

  if (compteursReinitialises) {
    sauvegarderDonneesPersistantes();
    Serial.println(
      "[JOURNALIER] Nouveau jour : compteurs remis a zero"
    );
  }
}

// ============================================================
void Task_Securite(void *pv) {
  static uint32_t debutSansDebit = 0;
  static bool chronoActif = false;

  for (;;) {

    heartbeatSecurite = millis();

    // ============================================================
    // Lecture des données utiles
    // ============================================================

    xSemaphoreTake(mutexDonnees, portMAX_DELAY);

    float temp = donnees.tempMoteur;
    float debit = donnees.debitEau;

    bool pompeActive = donnees.pompeActive;
    bool pompeEnAmorcage = donnees.pompeEnAmorcage;

    bool defautMoteur = donnees.defautMoteur;
    bool defautEnergie = donnees.defautEnergie;

    EtatMachine etatAvant = donnees.etat;

    xSemaphoreGive(mutexDonnees);


    // ============================================================
    // Surveillance température
    // ============================================================

    bool tempInvalide = isnan(temp);

    bool tempAlerte =
      !tempInvalide &&
      temp >= SEUIL_TEMP_ALERTE_C &&
      temp < SEUIL_TEMP_ARRET_C;


    // ============================================================
    // Surveillance débit d'eau
    // ============================================================

    bool manqueEauUrgence = false;
    bool debitmetreDeconnecte = false;

    uint32_t compteurImpulsionsTotal =
      lireImpulsionsTotalDebitmetre();

    if (pompeEnAmorcage) {

      chronoActif = false;

    }
    else if (
      pompeActive &&
      debit < SEUIL_DEBIT_MIN_LMIN
    ) {

      if (!chronoActif) {

        chronoActif = true;
        debutSansDebit = millis();

      }
      else if (
        millis() - debutSansDebit >=
        DUREE_MAX_SANS_DEBIT_MS
      ) {

        manqueEauUrgence = true;

        if (compteurImpulsionsTotal == 0) {
          debitmetreDeconnecte = true;
        }
      }

    }
    else {

      chronoActif = false;
    }


    // ============================================================
    // Détermination de la cause d'arrêt
    // ============================================================

    CauseArret cause = CAUSE_AUCUNE;

    if (tempInvalide) {

      cause = CAUSE_CAPTEUR_TEMP_INVALIDE;

    }
    else if (temp >= SEUIL_TEMP_ARRET_C) {

      cause = CAUSE_TEMPERATURE_MOTEUR;

    }
    else if (defautMoteur) {

      cause = CAUSE_DEFAUT_MOTEUR;

    }
    else if (defautEnergie) {

      cause = CAUSE_DEFAUT_ENERGIE;

    }
    else if (debitmetreDeconnecte) {

      cause = CAUSE_DEBITMETRE_DECONNECTE;

    }
    else if (manqueEauUrgence) {

      cause = CAUSE_MANQUE_EAU;

    }


    bool defautCritique =
      cause != CAUSE_AUCUNE;


    // ============================================================
    // Mise à jour de l'état de sécurité
    // ============================================================

    xSemaphoreTake(mutexDonnees, portMAX_DELAY);

    bool etaitCritique =
      donnees.defautCritique;

    bool etaitAlerte =
      donnees.alerteActive;

    donnees.defautCritique =
      defautCritique;

    donnees.alerteActive =
      tempAlerte;


    if (defautCritique) {

      donnees.causeArret = cause;

      if (donnees.etat != ETAT_ARRET_SECURISE) {
        donnees.etat = ETAT_ARRET_SECURISE;
        donnees.publicationEtatDemandee = true;
      }

      donnees.pompeActive = false;
      donnees.moteurActif = false;

      // Interdiction de redémarrage sans acquittement local
      donnees.acquittementLocalRequis = true;

      // Supprimer toute ancienne demande de démarrage
      donnees.demandeDemarrage = false;


    }
    else if (
      etaitCritique &&
      etatAvant == ETAT_ARRET_SECURISE &&
      donnees.acquittementLocalRequis
    ) {

      if (
        donnees.etat !=
        ETAT_ALERTE_ACQUITTEMENT
      ) {

        donnees.etat =
          ETAT_ALERTE_ACQUITTEMENT;

        donnees.publicationEtatDemandee = true;
      }
    }

    xSemaphoreGive(mutexDonnees);


    // ============================================================
    // Préparation de l'alerte MQTT
    // Aucun accès direct au modem ici
    // ============================================================

    if (
      defautCritique &&
      !etaitCritique
    ) {

      const char* nomAlerte =
        "arret_securise";

      if (
        cause ==
        CAUSE_TEMPERATURE_MOTEUR
      ) {

        nomAlerte =
          "surchauffe_moteur";

      }
      else if (
        cause ==
        CAUSE_MANQUE_EAU
      ) {

        nomAlerte =
          "debit_eau_anormal";

      }
      else if (
        cause ==
        CAUSE_DEFAUT_ENERGIE
      ) {

        nomAlerte =
          "defaut_reseau";
      }


      char champs[128];

      snprintf(
        champs,
        sizeof(champs),
        "{\"niveau\":\"critique\","
        "\"cause\":\"%s\"}",
        causeArretTexte(cause)
      );


      xSemaphoreTake(
        mutexDonnees,
        portMAX_DELAY
      );

      strncpy(
        donnees.nomAlerteEnAttente,
        nomAlerte,
        sizeof(
          donnees.nomAlerteEnAttente
        ) - 1
      );

      donnees.nomAlerteEnAttente[
        sizeof(
          donnees.nomAlerteEnAttente
        ) - 1
      ] = '\0';


      strncpy(
        donnees.champsAlerteEnAttente,
        champs,
        sizeof(
          donnees.champsAlerteEnAttente
        ) - 1
      );

      donnees.champsAlerteEnAttente[
        sizeof(
          donnees.champsAlerteEnAttente
        ) - 1
      ] = '\0';


      donnees.alerteCritiqueEnAttente =
        true;

      donnees.publicationAlerteDemandee =
        true;

      xSemaphoreGive(mutexDonnees);
    }

    else if (
      tempAlerte &&
      !etaitAlerte
    ) {

      xSemaphoreTake(
        mutexDonnees,
        portMAX_DELAY
      );

      strncpy(
        donnees.nomAlerteEnAttente,
        "surchauffe_moteur",
        sizeof(
          donnees.nomAlerteEnAttente
        ) - 1
      );

      donnees.nomAlerteEnAttente[
        sizeof(
          donnees.nomAlerteEnAttente
        ) - 1
      ] = '\0';


      strncpy(
        donnees.champsAlerteEnAttente,
        "{\"niveau\":\"alerte\"}",
        sizeof(
          donnees.champsAlerteEnAttente
        ) - 1
      );

      donnees.champsAlerteEnAttente[
        sizeof(
          donnees.champsAlerteEnAttente
        ) - 1
      ] = '\0';


      donnees.alerteCritiqueEnAttente =
        false;

      donnees.publicationAlerteDemandee =
        true;

      xSemaphoreGive(mutexDonnees);
    }


    // ============================================================
    // Arrêt physique immédiat
    // ============================================================

    if (defautCritique) {

      commanderPompePWM(0);
      commanderMoteur(false);
      commanderAlarme(true);


     


      Serial.printf(
        "!!! DEFAUT CRITIQUE: %s !!!\n",
        causeArretTexte(cause)
      );

    }
    else if (tempAlerte) {

      commanderAlarme(true);

      Serial.printf(
        ">>> ALERTE - temperature moteur %.1f C\n",
        temp
      );

    }
    else {

      commanderAlarme(false);
    }


    vTaskDelay(
      pdMS_TO_TICKS(1000)
    );
  }
}

// ============================================================
void Task_Acquisition_Capteurs(void *pv) {

#ifdef SCENARIO_MQTT_TEST

  static float humiditeSimulee = 12.0f;
  static float debitSimule = 0.0f;
  static float temperatureSimulee = 30.0f;
  static float tensionSimulee = 230.0f;
  static float courantSimule = 0.0f;
  static float energieSimulee = 0.0f;
  static float frequenceSimulee = 50.0f;

#endif

  for (;;) {

    heartbeatAcquisition = millis();

#ifdef SCENARIO_MQTT_TEST

    // Récupérer l'état actuel de la machine
    xSemaphoreTake(mutexDonnees, portMAX_DELAY);

    bool moteurActif = donnees.moteurActif;
    bool pompeActive = donnees.pompeActive;

    xSemaphoreGive(mutexDonnees);


    // ============================================================
    // HUMIDITE FARINE
    // Disponible même avant le démarrage
    // ============================================================

    humiditeSimulee += random(-8, 9) / 100.0f;

    humiditeSimulee =
      constrain(humiditeSimulee, 11.0f, 15.0f);


    // ============================================================
    // RESEAU ELECTRIQUE
    // Disponible en permanence
    // ============================================================

    tensionSimulee += random(-40, 41) / 100.0f;
    frequenceSimulee += random(-2, 3) / 100.0f;

    tensionSimulee =
      constrain(tensionSimulee, 225.0f, 235.0f);

    frequenceSimulee =
      constrain(frequenceSimulee, 49.80f, 50.20f);


    // ============================================================
    // TEMPERATURE MOTEUR
    // Monte lorsque le moteur fonctionne
    // Redescend doucement lorsqu'il est arrêté
    // ============================================================

    if (moteurActif) {

      temperatureSimulee +=
        random(1, 8) / 100.0f;

    } else {

      if (temperatureSimulee > 30.0f) {

        temperatureSimulee -=
          random(1, 5) / 100.0f;
      }
    }

    temperatureSimulee =
      constrain(temperatureSimulee, 30.0f, 48.0f);


    // ============================================================
    // DEBIT D'EAU
    // Débit uniquement lorsque la pompe fonctionne
    // ============================================================

    if (pompeActive) {

      if (debitSimule == 0.0f) {
        debitSimule = 2.0f;
      }

      debitSimule +=
        random(-5, 6) / 100.0f;

      debitSimule =
        constrain(debitSimule, 1.70f, 2.30f);

    } else {

      debitSimule = 0.0f;
    }


    // ============================================================
    // COURANT MOTEUR
    // Courant uniquement lorsque le moteur fonctionne
    // ============================================================

    if (moteurActif) {

      if (courantSimule == 0.0f) {
        courantSimule = 1.5f;
      }

      courantSimule +=
        random(-3, 4) / 100.0f;

      courantSimule =
        constrain(courantSimule, 1.20f, 1.80f);

    } else {

      courantSimule = 0.0f;
    }


    // ============================================================
    // VALEURS FINALES
    // ============================================================

    float humidite = humiditeSimulee;
    float debit = debitSimule;
    float temp = temperatureSimulee;
    float tension = tensionSimulee;
    float courant = courantSimule;

    float puissance = 0.0f;

    if (moteurActif) {

      puissance =
        tension * courant * 0.90f;

      // La boucle s'exécute environ une fois par seconde
      energieSimulee +=
        puissance / 3600000.0f;
    }

    float energie = energieSimulee;
    float frequence = frequenceSimulee;

    bool defMoteur = false;
    bool defEnergie = false;


#else

    // ============================================================
    // VRAIS CAPTEURS
    // ============================================================

    float humidite = lireHumiditeFarine();
    float debit = lireDebitEau();
    float temp = lireTemperatureMoteur();
    float tension = lireTensionReseau();
    float courant = lireCourantMoteur();
    float puissance = lirePuissance();
    float energie = lireEnergie();
    float frequence = lireFrequenceReseau();

    bool defMoteur = lireDefautMoteur();
    bool defEnergie = lireDefautEnergie();

#endif


    // ============================================================
    // MISE A JOUR DES DONNEES PARTAGEES
    // ============================================================

    xSemaphoreTake(mutexDonnees, portMAX_DELAY);

    donnees.humiditeFarine = humidite;
    donnees.debitEau = debit;
    donnees.tempMoteur = temp;
    donnees.tensionReseau = tension;
    donnees.courantMoteur = courant;
    donnees.puissance = puissance;
    donnees.energie = energie;
    donnees.frequenceReseau = frequence;

    donnees.defautMoteur = defMoteur;
    donnees.defautEnergie = defEnergie;

    xSemaphoreGive(mutexDonnees);


    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ============================================================
void Task_Calcul_Humidification(void *pv) {
  static uint32_t dernierTour = 0;
  static uint32_t resteTempsTravailMs = 0;

  for (;;) {
    heartbeatCalcul = millis();
    verifierNouveauJour();

    uint32_t maintenant = millis();
    uint32_t deltaMs = (dernierTour == 0) ? 1000 : (maintenant - dernierTour);
    dernierTour = maintenant;

    xSemaphoreTake(mutexDonnees, portMAX_DELAY);
    EtatMachine etatCourant = donnees.etat;
    EtatMachine etatPrecedent = etatCourant;
    bool defautCritique = donnees.defautCritique;
    bool demandeDemarrage = donnees.demandeDemarrage;
    bool demandeStop = donnees.demandeStop;
    bool demandeAcquittement = donnees.demandeAcquittement;
    float humidite = donnees.humiditeFarine;
    float debitMesure = donnees.debitEau;
    MatierePremiere matiere = donnees.matiereSelectionnee;
    ProduitFinal produit = donnees.produitSelectionne;
    float debitDoseuse = donnees.debitDoseuse_kgMin;
    xSemaphoreGive(mutexDonnees);

    if (!defautCritique) {
      switch (etatCourant) {

        case ETAT_DEMARRAGE:
          etatCourant = ETAT_INITIALISATION;
          break;

        case ETAT_INITIALISATION:
          etatCourant = ETAT_MACHINE_PRETE;
          break;

        case ETAT_MACHINE_PRETE:
          if (demandeDemarrage) {
            Serial.println("[ETAT] Machine prete -> Configuration");
            etatCourant = ETAT_CONFIGURATION;
          }
          break;

        case ETAT_CONFIGURATION: {
          float testDebit = calculerDebitCible_mLparMin(matiere, produit, humidite, debitDoseuse);
          if (testDebit < 0) {
            Serial.println("[ETAT] Configuration -> Alerte config (recette indisponible)");
            xSemaphoreTake(mutexDonnees, portMAX_DELAY);
            donnees.causeArret = CAUSE_RECETTE_INVALIDE;
            xSemaphoreGive(mutexDonnees);
            etatCourant = ETAT_ALERTE_CONFIG;
          } else {
            Serial.println("[ETAT] Configuration -> Production (continu)");
            xSemaphoreTake(mutexDonnees, portMAX_DELAY);
            donnees.quantiteEauTotaleInjectee = 0;
            donnees.debutProduction = millis();
            donnees.pompeActive = true;
            donnees.moteurActif = true;
            xSemaphoreGive(mutexDonnees);
            etatCourant = ETAT_PRODUCTION;
          }
          break;
        }

        case ETAT_ALERTE_CONFIG:
          if (calculerDebitCible_mLparMin(matiere, produit, humidite, debitDoseuse) >= 0) {
            Serial.println("[ETAT] Recette corrigee -> Configuration");
            etatCourant = ETAT_CONFIGURATION;
          } else if (demandeStop) {
            xSemaphoreTake(mutexDonnees, portMAX_DELAY);
            donnees.demandeDemarrage = false;
            donnees.demandeStop = false;
            xSemaphoreGive(mutexDonnees);
            etatCourant = ETAT_MACHINE_PRETE;
          }
          break;

        case ETAT_PRODUCTION: {
          float debitCible = calculerDebitCible_mLparMin(matiere, produit, humidite, debitDoseuse);
          bool ecart = false;
          if (debitCible >= 0) {
            float debitCibleLmin = debitCible / 1000.0f;
            ecart = (debitCibleLmin > 0) && (fabs(debitMesure - debitCibleLmin) > TOLERANCE_DEBIT_RELATIVE * debitCibleLmin);
          }

          xSemaphoreTake(mutexDonnees, portMAX_DELAY);
          donnees.debitEauCible = debitCible;
          donnees.ecartDebitDetecte = ecart;
          donnees.quantiteEauTotaleInjectee += debitMesure * 1000.0f * deltaMs / 60000.0f;
          resteTempsTravailMs += deltaMs;

          donnees.tempsTravailJournalier_s +=
            resteTempsTravailMs / 1000;

          resteTempsTravailMs %= 1000;
          xSemaphoreGive(mutexDonnees);

          if (ecart) Serial.println("[CALCUL] Ecart debit mesure/cible detecte");

          if (demandeStop) {
            Serial.println("[ETAT] Production -> Fin production");
            xSemaphoreTake(mutexDonnees, portMAX_DELAY);
            donnees.pompeActive = false;
            donnees.moteurActif = false;
            xSemaphoreGive(mutexDonnees);
            etatCourant = ETAT_FIN_PRODUCTION;
          }
          break;
        }

        case ETAT_FIN_PRODUCTION:
          etatCourant = ETAT_HISTORISATION;
          break;

       case ETAT_HISTORISATION: {
  xSemaphoreTake(mutexDonnees, portMAX_DELAY);

  float dureeProdMin =
    (millis() - donnees.debutProduction) / 60000.0f;

  donnees.eauInjecteeJournaliere_L +=
    donnees.quantiteEauTotaleInjectee / 1000.0f;

  donnees.productionJournaliere_kg +=
    donnees.debitDoseuse_kgMin * dureeProdMin;

  donnees.demandeDemarrage = false;
  donnees.demandeStop = false;
  donnees.publicationProductionDemandee = true;

  bool verrouDistant =
    donnees.arretDistantActif;

  if (
    verrouDistant &&
    donnees.commandIdStopEnAttente[0] != '\0'
  ) {
    donnees.confirmationStopDemandee = true;
  }

  xSemaphoreGive(mutexDonnees);

  sauvegarderDonneesPersistantes();

  if (verrouDistant) {
    Serial.println(
      "[ETAT] Historisation -> Arret distant"
    );

    etatCourant = ETAT_ARRET_DISTANT;
  } else {
    Serial.println(
      "[ETAT] Historisation -> Machine prete"
    );

    etatCourant = ETAT_MACHINE_PRETE;
  }

  break;
}
        case ETAT_ARRET_DISTANT:
  // Aucun demarrage local ou automatique autorise.
  // Seule une commande START distante peut lever le verrou.
  break;

        case ETAT_ALERTE_ACQUITTEMENT:
          if (demandeAcquittement) {
            Serial.println("[ETAT] Acquittement recu -> Machine prete");
            xSemaphoreTake(mutexDonnees, portMAX_DELAY);
            donnees.demandeAcquittement = false;
            donnees.demandeStopDistant = false;
            donnees.acquittementLocalRequis = false;
            donnees.causeArret = CAUSE_AUCUNE;
            xSemaphoreGive(mutexDonnees);
            etatCourant = ETAT_MACHINE_PRETE;
          }
          break;

        default:
          break;
      }
    } else {
      etatCourant = ETAT_ARRET_SECURISE;
    }

    xSemaphoreTake(mutexDonnees, portMAX_DELAY);

donnees.etat = etatCourant;

if (etatCourant != etatPrecedent) {
  donnees.publicationEtatDemandee = true;
}

xSemaphoreGive(mutexDonnees);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ============================================================
void Task_Commande_Actionneurs(void *pv) {
  static bool pompeEtaitActive = false;
  static uint32_t debutAmorcage = 0;
  static uint32_t dernierTempsRegulation = 0;
  static float integraleErreur = 0;
  static float erreurPrecedente = 0;

  for (;;) {
    heartbeatCommande = millis();

    xSemaphoreTake(mutexDonnees, portMAX_DELAY);
    bool defautCritique = donnees.defautCritique;
    bool pompeVoulue = donnees.pompeActive;
    bool moteurVoulu = donnees.moteurActif;
    float debitCibleMlMin = donnees.debitEauCible;
    float debitMesureLMin = donnees.debitEau;
    xSemaphoreGive(mutexDonnees);

    if (defautCritique) {
      pompeEtaitActive = false;
      integraleErreur = 0;
      erreurPrecedente = 0;
      xSemaphoreTake(mutexDonnees, portMAX_DELAY);
      donnees.pompeEnAmorcage = false;
      xSemaphoreGive(mutexDonnees);
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    commanderMoteur(moteurVoulu);

    uint8_t pwm = 0;
    if (pompeVoulue && debitCibleMlMin > 0) {
      if (!pompeEtaitActive) {
        pompeEtaitActive = true;
        debutAmorcage = millis();
        dernierTempsRegulation = millis();
        integraleErreur = 0;
        erreurPrecedente = 0;
        xSemaphoreTake(mutexDonnees, portMAX_DELAY);
        donnees.pompeEnAmorcage = true;
        xSemaphoreGive(mutexDonnees);
      }

      if (millis() - debutAmorcage < DUREE_AMORCAGE_MS) {
        pwm = PWM_AMORCAGE;
      } else {
        xSemaphoreTake(mutexDonnees, portMAX_DELAY);
        donnees.pompeEnAmorcage = false;
        xSemaphoreGive(mutexDonnees);

        uint32_t maintenant = millis();
        float deltaS = (maintenant - dernierTempsRegulation) / 1000.0f;
        if (deltaS <= 0) deltaS = 0.2f;
        dernierTempsRegulation = maintenant;

        float debitCibleLMin = debitCibleMlMin / 1000.0f;
        float erreur = debitCibleLMin - debitMesureLMin;

        float termeP = KP_DEBIT_PWM * erreur;

        integraleErreur += erreur * deltaS;
        float termeI = KI_DEBIT_PWM * integraleErreur;
        if (termeI > INTEGRALE_MAX)  { termeI = INTEGRALE_MAX;  integraleErreur = (KI_DEBIT_PWM != 0) ? INTEGRALE_MAX / KI_DEBIT_PWM : 0; }
        if (termeI < -INTEGRALE_MAX) { termeI = -INTEGRALE_MAX; integraleErreur = (KI_DEBIT_PWM != 0) ? -INTEGRALE_MAX / KI_DEBIT_PWM : 0; }

        float derivee = (erreur - erreurPrecedente) / deltaS;
        float termeD = KD_DEBIT_PWM * derivee;
        erreurPrecedente = erreur;

        int pwmCalcule = PWM_AMORCAGE + (int)(termeP + termeI + termeD);
        if (pwmCalcule < PWM_MIN_REGULATION) pwmCalcule = PWM_MIN_REGULATION;
        if (pwmCalcule > PWM_MAX_REGULATION) pwmCalcule = PWM_MAX_REGULATION;
        pwm = (uint8_t)pwmCalcule;
      }
    } else {
      pompeEtaitActive = false;
      integraleErreur = 0;
      erreurPrecedente = 0;
      xSemaphoreTake(mutexDonnees, portMAX_DELAY);
      donnees.pompeEnAmorcage = false;
      xSemaphoreGive(mutexDonnees);
    }

    commanderPompePWM(pwm);

    xSemaphoreTake(mutexDonnees, portMAX_DELAY);
    donnees.pwmPompe = pwm;
    xSemaphoreGive(mutexDonnees);

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// ============================================================
// TASK HMI
// ============================================================

void Task_HMI(void *pv) {

  static uint32_t dernierRafraichissement = 0;

#ifndef SIMULATION

  initEcran();

#endif

  for (;;) {

#ifdef SIMULATION

    // ========================================================
    // SIMULATION HMI VIA MONITEUR SERIE
    // ========================================================

    if (Serial.available()) {

      char c = Serial.read();

      xSemaphoreTake(
        mutexDonnees,
        portMAX_DELAY
      );

      // ------------------------------------------------------
      // DEMARRAGE
      // ------------------------------------------------------

      if (c == 's') {

        donnees.demandeDemarrage = true;

        Serial.println(
          "[HMI] Demarrage demande"
        );
      }

      // ------------------------------------------------------
      // ARRET
      // ------------------------------------------------------

      else if (c == 'x') {

        donnees.demandeStop = true;

        Serial.println(
          "[HMI] Arret demande"
        );
      }

      // ------------------------------------------------------
      // ACQUITTEMENT
      // ------------------------------------------------------

      else if (c == 'a') {

        donnees.demandeAcquittement = true;

        Serial.println(
          "[HMI] Acquittement demande"
        );
      }

      // ------------------------------------------------------
      // SELECTION MATIERE
      // m : Mil -> Mais -> Riz
      // ------------------------------------------------------

      else if (c == 'm') {

        int prochaineMatiere =
          static_cast<int>(
            donnees.matiereSelectionnee
          ) + 1;

        if (
          prochaineMatiere >
          MATIERE_RIZ
        ) {
          prochaineMatiere =
            MATIERE_MIL;
        }

        donnees.matiereSelectionnee =
          static_cast<MatierePremiere>(
            prochaineMatiere
          );

        Serial.printf(
          "[HMI] Matiere -> %d\n",
          prochaineMatiere
        );
      }

      // ------------------------------------------------------
      // SELECTION PRODUIT
      // p : Arraw -> Thiakry -> Thiere
      // ------------------------------------------------------

      else if (c == 'p') {

        int prochainProduit =
          static_cast<int>(
            donnees.produitSelectionne
          ) + 1;

        if (
          prochainProduit >
          PRODUIT_THIERE
        ) {
          prochainProduit =
            PRODUIT_ARRAW;
        }

        donnees.produitSelectionne =
          static_cast<ProduitFinal>(
            prochainProduit
          );

        Serial.printf(
          "[HMI] Produit -> %d\n",
          prochainProduit
        );
      }

      xSemaphoreGive(
        mutexDonnees
      );
    }

#else

    // ========================================================
    // ECRAN DGUS REEL
    // ========================================================

    gererReceptionEcran();

    // Rafraichissement de l'ecran toutes les secondes
    if (
      millis() -
      dernierRafraichissement >= 1000
    ) {

      // ------------------------------------------------------
      // COPIE DES DONNEES PARTAGEES
      // ------------------------------------------------------

      xSemaphoreTake(
        mutexDonnees,
        portMAX_DELAY
      );

      float humidite =
        donnees.humiditeFarine;

      float debit =
        donnees.debitEau;

      float temperature =
        donnees.tempMoteur;

      float production =
        donnees.productionJournaliere_kg;

      float tension =
        donnees.tensionReseau;

      float courant =
        donnees.courantMoteur;

      float puissance =
        donnees.puissance;

      float energie =
        donnees.energie;

      float frequence =
        donnees.frequenceReseau;

      float eauJournaliere =
        donnees.eauInjecteeJournaliere_L;

      uint32_t tempsTravailSecondes =
        donnees.tempsTravailJournalier_s;

      float debitCible =
        donnees.debitEauCible;

      EtatMachine etat =
        donnees.etat;

      CauseArret cause =
        donnees.causeArret;

      bool alerteActive =
        donnees.alerteActive;

      bool defautCritique =
        donnees.defautCritique;

      bool moteurActif =
        donnees.moteurActif;

      bool pompeActive =
        donnees.pompeActive;

      bool connexionDisponible =
        donnees.connexionDisponible;

      bool defautMoteur =
        donnees.defautMoteur;

      bool defautEnergie =
        donnees.defautEnergie;

      MatierePremiere matiere =
        donnees.matiereSelectionnee;

      ProduitFinal produit =
        donnees.produitSelectionne;

      xSemaphoreGive(
        mutexDonnees
      );

      // ======================================================
      // RECETTE
      // ======================================================

      ecrireEcran(
        VP_SELECTION_MATIERE,
        static_cast<uint16_t>(matiere)
      );

      ecrireEcran(
        VP_SELECTION_PRODUIT,
        static_cast<uint16_t>(produit)
      );

      // ======================================================
      // MESURES PRINCIPALES
      // ======================================================

      // Exemple : 12.4 % -> 124
      ecrireEcran(
        VP_HUMIDITE,
        static_cast<uint16_t>(
          humidite * 10.0f
        )
      );

      // Exemple : 1.85 L/min -> 185
      ecrireEcran(
        VP_DEBIT_EAU,
        static_cast<uint16_t>(
          debit * 100.0f
        )
      );

      // Exemple : 42.3 C -> 423
      ecrireEcran(
        VP_TEMP_MOTEUR,
        static_cast<uint16_t>(
          temperature * 10.0f
        )
      );

      // Exemple : 125.6 kg -> 1256
      ecrireEcran(
        VP_PRODUCTION_KG,
        static_cast<uint16_t>(
          production * 10.0f
        )
      );

      // ======================================================
      // MESURES ELECTRIQUES
      // ======================================================

      // 230.5 V -> 2305
      ecrireEcran(
        VP_TENSION_RESEAU,
        static_cast<uint16_t>(
          tension * 10.0f
        )
      );

      // 1.52 A -> 152
      ecrireEcran(
        VP_COURANT_MOTEUR,
        static_cast<uint16_t>(
          courant * 100.0f
        )
      );

      // Puissance en W
      ecrireEcran(
        VP_PUISSANCE,
        static_cast<uint16_t>(
          puissance
        )
      );

      // 1.25 kWh -> 125
      ecrireEcran(
        VP_ENERGIE,
        static_cast<uint16_t>(
          energie * 100.0f
        )
      );

      // 50.00 Hz -> 5000
      ecrireEcran(
        VP_FREQUENCE_RESEAU,
        static_cast<uint16_t>(
          frequence * 100.0f
        )
      );

      // ======================================================
      // PRODUCTION / EAU
      // ======================================================

      // 12.45 L -> 1245
      ecrireEcran(
        VP_EAU_JOURNALIERE,
        static_cast<uint16_t>(
          eauJournaliere * 100.0f
        )
      );

      // Temps affiche en minutes
      ecrireEcran(
        VP_TEMPS_TRAVAIL,
        static_cast<uint16_t>(
          tempsTravailSecondes / 60
        )
      );

      // Debit cible deja stocke en mL/min
      ecrireEcran(
        VP_DEBIT_CIBLE,
        static_cast<uint16_t>(
          debitCible
        )
      );

      // ======================================================
      // ETAT MACHINE / ALARMES
      // ======================================================

      ecrireEcran(
        VP_ETAT_MACHINE,
        static_cast<uint16_t>(etat)
      );

      ecrireEcran(
        VP_CAUSE_ARRET,
        static_cast<uint16_t>(cause)
      );

      ecrireEcran(
        VP_ALERTE_ACTIVE,
        alerteActive ? 1 : 0
      );

      ecrireEcran(
        VP_DEFAUT_CRITIQUE,
        defautCritique ? 1 : 0
      );

      // ======================================================
      // SYNOPTIQUE
      // ======================================================

      ecrireEcran(
        VP_PROCESS_ACTIF,
        moteurActif ? 1 : 0
      );

      ecrireEcran(
        VP_POMPE_ACTIVE,
        pompeActive ? 1 : 0
      );

      // ======================================================
      // ETATS TECHNIQUES
      // ======================================================

      ecrireEcran(
        VP_CONNEXION_RESEAU,
        connexionDisponible ? 1 : 0
      );

      ecrireEcran(
        VP_DEFAUT_MOTEUR,
        defautMoteur ? 1 : 0
      );

      ecrireEcran(
        VP_DEFAUT_ENERGIE,
        defautEnergie ? 1 : 0
      );

      dernierRafraichissement =
        millis();
    }

#endif

    vTaskDelay(
      pdMS_TO_TICKS(200)
    );
  }
}
// ============================================================
void Task_Communication_MQTT(void *pv) {
  static uint32_t dernierePublication = 0;

  for (;;) {

    bool connecteMqtt = estConnecteMQTT();

    // On ne fait des commandes AT reseau que si MQTT est deconnecte
    if (!connecteMqtt) {

        bool connecteReseau =
            connecterReseauCellulaire();

        connecteMqtt =
            connecteReseau && connecterMQTT();
    }

    xSemaphoreTake(
        mutexDonnees,
        portMAX_DELAY
    );

    donnees.connexionDisponible =
        connecteMqtt;

    xSemaphoreGive(mutexDonnees);

    if (connecteMqtt) {

        bouclerMQTT();

      // ------------------------------------------------
      // Vérification des publications immédiates
      // ------------------------------------------------

      bool publierEtatMaintenant = false;
      bool publierProductionMaintenant = false;
      bool confirmerStopMaintenant = false;
      char commandIdStop[48] = "";
      bool publierAlerteMaintenant = false;
      bool alerteCritique = false;

      char nomAlerte[40] = "";
      char champsAlerte[160] = "";

      float eauJournaliereProduction = 0;
      float productionJournaliereBilan = 0;

      xSemaphoreTake(mutexDonnees, portMAX_DELAY);

      publierEtatMaintenant =
        donnees.publicationEtatDemandee;

      publierProductionMaintenant =
        donnees.publicationProductionDemandee;

      confirmerStopMaintenant =
  donnees.confirmationStopDemandee;

      publierAlerteMaintenant =
  donnees.publicationAlerteDemandee;

if (publierAlerteMaintenant) {

  strncpy(
    nomAlerte,
    donnees.nomAlerteEnAttente,
    sizeof(nomAlerte) - 1
  );

  nomAlerte[sizeof(nomAlerte) - 1] = '\0';

  strncpy(
    champsAlerte,
    donnees.champsAlerteEnAttente,
    sizeof(champsAlerte) - 1
  );

  champsAlerte[sizeof(champsAlerte) - 1] = '\0';

  alerteCritique =
    donnees.alerteCritiqueEnAttente;

  donnees.publicationAlerteDemandee = false;
}

if (confirmerStopMaintenant) {

  strncpy(
    commandIdStop,
    donnees.commandIdStopEnAttente,
    sizeof(commandIdStop) - 1
  );

  commandIdStop[sizeof(commandIdStop) - 1] = '\0';

  donnees.confirmationStopDemandee = false;
}

      if (publierEtatMaintenant) {
        donnees.publicationEtatDemandee = false;
      }

      if (publierProductionMaintenant) {

        donnees.publicationProductionDemandee = false;

        eauJournaliereProduction =
          donnees.eauInjecteeJournaliere_L;

        productionJournaliereBilan =
          donnees.productionJournaliere_kg;
      }

      xSemaphoreGive(mutexDonnees);

      if (publierAlerteMaintenant) {

  Serial.printf(
    "[MQTT] Publication alerte : %s\n",
    nomAlerte
  );

  envoyerAlerte(
    nomAlerte,
    champsAlerte,
    alerteCritique
      ? PRIORITE_CRITIQUE
      : PRIORITE_HAUTE
  );
}

      if (confirmerStopMaintenant) {

  Serial.println(
    "[MQTT] Confirmation execution du stop distant"
  );

  envoyerAckCommande(
    commandIdStop,
    "stop",
    "executee",
    "Commande stop executee avec succes"
  );

  xSemaphoreTake(mutexDonnees, portMAX_DELAY);

  donnees.commandIdStopEnAttente[0] = '\0';

  xSemaphoreGive(mutexDonnees);
}


      // ------------------------------------------------
      // Publication immédiate d'un changement d'état
      // ------------------------------------------------

      if (publierEtatMaintenant) {

        Serial.println(
          "[MQTT] Publication immediate du nouvel etat"
        );

        envoyerEtatMachine();
      }


      // ------------------------------------------------
      // Publication du bilan après une production
      // ------------------------------------------------

      if (publierProductionMaintenant) {

        char champsProduction[192];

        snprintf(
          champsProduction,
          sizeof(champsProduction),
          "{\"eau_journaliere_L\":%.2f,"
          "\"production_journaliere_kg\":%.2f}",
          eauJournaliereProduction,
          productionJournaliereBilan
        );

        Serial.println(
          "[MQTT] Publication du bilan de production"
        );

        envoyerProduction(
          "journaliere",
          champsProduction
        );
      }


      // ------------------------------------------------
      // Publication périodique toutes les 3 secondes
      // ------------------------------------------------

      if (
        millis() - dernierePublication >=
        INTERVALLE_PUBLICATION_MS
      ) {

        xSemaphoreTake(
          mutexDonnees,
          portMAX_DELAY
        );

        float humidite =
          donnees.humiditeFarine;

        float debit =
          donnees.debitEau;

        float temp =
          donnees.tempMoteur;

        float energie =
          donnees.energie;

        float eauJournaliere =
          donnees.eauInjecteeJournaliere_L;

        float productionJournaliere =
          donnees.productionJournaliere_kg;

        float tempsTravailMinutes =
          donnees.tempsTravailJournalier_s / 60.0f;

        xSemaphoreGive(mutexDonnees);


        envoyerMesure(
          "humidite",
          humidite,
          "%",
          PRIORITE_MOYENNE
        );

        envoyerMesure(
          "debit_eau",
          debit,
          "L/min",
          PRIORITE_MOYENNE
        );

        envoyerMesure(
          "temperature_moteur",
          temp,
          "C",
          PRIORITE_HAUTE
        );

        envoyerMesure(
          "consommation_energetique",
          energie,
          "kWh",
          PRIORITE_MOYENNE
        );

        envoyerMesure(
          "quantite_eau_journaliere",
          eauJournaliere,
          "L",
          PRIORITE_MOYENNE
        );

        envoyerMesure(
          "production_journaliere",
          productionJournaliere,
          "kg",
          PRIORITE_MOYENNE
        );

        envoyerMesure(
          "temps_travail_journalier",
          tempsTravailMinutes,
          "min",
          PRIORITE_MOYENNE
        );

        envoyerEtatMachine();

        dernierePublication = millis();
      }

      vTaskDelay(pdMS_TO_TICKS(500));

    } else {

      vTaskDelay(
        pdMS_TO_TICKS(DELAI_RECONNEXION_MS)
      );
    }
  }
}

  
// ============================================================
void Task_Buffer_Offline(void *pv) {
  static bool sdDisponible = false;
  static bool sdInitTentee = false;

  for (;;) {
    if (!sdInitTentee) {
      sdDisponible = initBufferSD();
      sdInitTentee = true;
    }
    if (sdDisponible) republierMessagesEnAttente();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// ============================================================
void Task_Watchdog(void *pv) {
  const uint32_t TIMEOUT_MS = 5000;
  for (;;) {
    uint32_t maintenant = millis();
    bool blocage = false;
    if (maintenant - heartbeatSecurite > TIMEOUT_MS)    { Serial.println("[WATCHDOG] Task_Securite ne repond plus !"); blocage = true; }
    if (maintenant - heartbeatCommande > TIMEOUT_MS)    { Serial.println("[WATCHDOG] Task_Commande_Actionneurs ne repond plus !"); blocage = true; }
    if (maintenant - heartbeatAcquisition > TIMEOUT_MS) { Serial.println("[WATCHDOG] Task_Acquisition_Capteurs ne repond plus !"); blocage = true; }
    if (maintenant - heartbeatCalcul > TIMEOUT_MS)      { Serial.println("[WATCHDOG] Task_Calcul_Humidification ne repond plus !"); blocage = true; }

    if (blocage) {
      commanderMoteur(false);
      commanderPompePWM(0);
      commanderAlarme(true);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

#ifdef SCENARIO_MQTT_TEST

void Task_Scenario_MQTT(void *pv) {
  Serial.println("[SCENARIO] Attente de la machine prete...");

  // Attendre que la machine termine demarrage et initialisation
  for (;;) {
  xSemaphoreTake(mutexDonnees, portMAX_DELAY);

  EtatMachine etat = donnees.etat;
  bool mqttConnecte = donnees.connexionDisponible;

  xSemaphoreGive(mutexDonnees);

  if (etat == ETAT_MACHINE_PRETE && mqttConnecte) {
    break;
  }

  vTaskDelay(pdMS_TO_TICKS(500));
}

Serial.println("[SCENARIO] Machine prete et MQTT connecte");

  // Simulation des choix de l'operateur
  xSemaphoreTake(mutexDonnees, portMAX_DELAY);
  donnees.matiereSelectionnee = MATIERE_MIL;
  donnees.produitSelectionne = PRODUIT_THIAKRY;
  xSemaphoreGive(mutexDonnees);

  Serial.println("[SCENARIO] Matiere selectionnee : mil");
  Serial.println("[SCENARIO] Produit selectionne : thiakry");

  vTaskDelay(pdMS_TO_TICKS(3000));

  // Simulation de l'appui sur Demarrer
  xSemaphoreTake(mutexDonnees, portMAX_DELAY);
  donnees.demandeDemarrage = true;
  xSemaphoreGive(mutexDonnees);

  Serial.println("[SCENARIO] Demarrage production demande");

  // Laisser la production simulee fonctionner pendant 30 secondes
  vTaskDelay(pdMS_TO_TICKS(30000));

  // Simulation de l'appui sur Arreter
  xSemaphoreTake(mutexDonnees, portMAX_DELAY);
  donnees.demandeStop = true;
  xSemaphoreGive(mutexDonnees);

  Serial.println("[SCENARIO] Arret production demande");

  vTaskDelete(NULL);
}

#endif

// ============================================================
// ============================================================
// SETUP
// ============================================================
void setup() {

  Serial.begin(115200);

  disableCore0WDT();

  delay(1000);

  Serial.println("HMP - demarrage FreeRTOS");


  // ============================================================
  // CREATION DU MUTEX DES DONNEES PARTAGEES
  // ============================================================

  mutexDonnees = xSemaphoreCreateMutex();

  if (mutexDonnees == NULL) {

    Serial.println(
      "[FATAL] Creation du mutex echouee - arret"
    );

    while (true) {
      delay(1000);
    }
  }


  // ============================================================
  // COMMUNICATION + DONNEES PERSISTANTES
  // ============================================================

  initComm();

  chargerDonneesPersistantes();


  // ============================================================
  // TACHE SECURITE
  // ============================================================

  xTaskCreatePinnedToCore(
    Task_Securite,       // Fonction
    "Securite",          // Nom
    4096,                // Stack
    NULL,                // Parametre
    4,                   // Priorite
    &hSecurite,          // Handle
    1                    // Core
  );


  // ============================================================
  // TACHE MQTT
  // ============================================================

  xTaskCreatePinnedToCore(
    Task_Communication_MQTT,
    "MQTT",
    4096,
    NULL,
    3,
    &hMqtt,
    0
  );


  // ============================================================
  // TACHE CALCUL / HUMIDIFICATION
  // ============================================================

  xTaskCreatePinnedToCore(
    Task_Calcul_Humidification,
    "Calcul",
    4096,
    NULL,
    2,
    &hCalcul,
    1
  );

  #ifndef SIMULATION
  initExpandeurUART();
#endif


  // ============================================================
  // TACHE HMI
  // ============================================================

  xTaskCreatePinnedToCore(
    Task_HMI,
    "HMI",
    4096,
    NULL,
    2,
    &hHmi,
    1
  );




  // ============================================================
  // MODE SCENARIO MQTT
  // ============================================================

#ifdef SCENARIO_MQTT_TEST

  initCapteurs();


  // ============================================================
  // CAPTEURS SIMULES
  // ============================================================

  xTaskCreatePinnedToCore(
    Task_Acquisition_Capteurs,
    "CapteursSimules",
    4096,
    NULL,
    2,
    &hAcquisition,
    1
  );

#endif


  Serial.println(
    "[SYSTEME] Toutes les taches FreeRTOS lancees"
  );
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  // Tout fonctionne avec FreeRTOS.
  // La loop Arduino n'est donc pas utilisee.

  vTaskDelete(NULL);
}

