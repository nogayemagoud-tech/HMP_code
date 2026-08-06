#include "protocole_mqtt.h"
#include "config.h"
#include "etat.h"
#include "comm.h"
#include "buffer_sd.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

const char* etatVersChaine(EtatMachine etat);
const char* matiereVersChaine(MatierePremiere matiere);
const char* produitVersChaine(ProduitFinal produit);
const char* modeVersChaine(ModeFonctionnement mode);

static Preferences prefsSeq;

const char* etatVersChaine(EtatMachine etat) {
  switch (etat) {
    case ETAT_DEMARRAGE:
      return "demarrage";

    case ETAT_INITIALISATION:
      return "initialisation";

    case ETAT_MACHINE_PRETE:
      return "machine_prete";

    case ETAT_CONFIGURATION:
      return "configuration";

    case ETAT_PRODUCTION:
      return "production";

    case ETAT_FIN_PRODUCTION:
      return "fin_production";

    case ETAT_HISTORISATION:
      return "historisation";

    case ETAT_ALERTE_CONFIG:
      return "alerte_config";

    case ETAT_ARRET_SECURISE:
      return "arret_securise";

    case ETAT_ALERTE_ACQUITTEMENT:
      return "alerte_acquittement";

    case ETAT_MAINTENANCE:
      return "maintenance";

    default:
      return "inconnu";
  }
}

const char* prioriteVersChaine(PrioriteMessage p) {
  switch (p) {
    case PRIORITE_CRITIQUE: return "critique";
    case PRIORITE_HAUTE:    return "haute";
    case PRIORITE_MOYENNE:  return "moyenne";
    default:                return "basse";
  }
}

void genererMessageId(char* out, size_t tailleOut, uint32_t* seqOut) {
  prefsSeq.begin("seq", false);
  uint32_t seq = prefsSeq.getUInt("last", 0) + 1;
  prefsSeq.putUInt("last", seq);
  prefsSeq.end();
  snprintf(out, tailleOut, "%s-%06u", MACHINE_ID, seq);
  if (seqOut) *seqOut = seq;
}

void obtenirHorodatage(char* out, size_t tailleOut) {
  time_t maintenant = time(nullptr);
  struct tm infos;
  gmtime_r(&maintenant, &infos);
  strftime(out, tailleOut, "%Y-%m-%dT%H:%M:%SZ", &infos);
}

const char* matiereVersChaine(MatierePremiere matiere) {
  switch (matiere) {
    case MATIERE_MIL:
      return "mil";

    case MATIERE_MAIS:
      return "mais";

    case MATIERE_RIZ:
      return "riz";

    default:
      return "inconnue";
  }
}

const char* produitVersChaine(ProduitFinal produit) {
  switch (produit) {
    case PRODUIT_ARRAW:
      return "arraw";

    case PRODUIT_THIAKRY:
      return "thiakry";

    case PRODUIT_THIERE:
      return "thiere";

    default:
      return "inconnu";
  }
}

const char* modeVersChaine(ModeFonctionnement mode) {
  switch (mode) {
    case MODE_AUTOMATIQUE:
      return "automatique";

    case MODE_MANUEL:
      return "manuel";

    case MODE_MAINTENANCE:
      return "maintenance";

    default:
      return "inconnu";
  }
}


void construireTopic(const char* categorie, const char* parametre, char* out, size_t tailleOut) {
  if (parametre && strlen(parametre) > 0) {
    snprintf(out, tailleOut, "arrawtech/%s/%s/%s/%s", MACHINE_TYPE, MACHINE_ID, categorie, parametre);
  } else {
    snprintf(out, tailleOut, "arrawtech/%s/%s/%s", MACHINE_TYPE, MACHINE_ID, categorie);
  }
}

static void envoyerEnveloppe(const char* categorie, const char* parametre, JsonDocument& doc, PrioriteMessage priorite) {
  char messageId[24];
  char horodatage[24];
  char topic[80];
  uint32_t seq = 0;

  genererMessageId(messageId, sizeof(messageId), &seq);
  obtenirHorodatage(horodatage, sizeof(horodatage));
  construireTopic(categorie, parametre, topic, sizeof(topic));

  doc["message_id"] = messageId;
  doc["seq"] = seq;
  doc["machine_id"] = MACHINE_ID;
  doc["machine_type"] = MACHINE_TYPE;
  doc["timestamp"] = horodatage;

  char payload[512];
  serializeJson(doc, payload);

  ecrireMessageBuffer(messageId, topic, payload, prioriteVersChaine(priorite));

  if (estConnecteMQTT()) {
    publierBrut(topic, payload);
  }
}

void envoyerMesure(const char* parametre, float valeur, const char* unite, PrioriteMessage priorite) {
  JsonDocument doc;
  doc["parametre"] = parametre;
  doc["valeur"] = valeur;
  doc["unite"] = unite;
  xSemaphoreTake(mutexDonnees, portMAX_DELAY);
  doc["etat_machine"] = etatVersChaine(donnees.etat);
  xSemaphoreGive(mutexDonnees);
  doc["source"] = "esp32_s3";
  envoyerEnveloppe("telemetry", parametre, doc, priorite);
}

void envoyerAlerte(const char* nomAlerte, const char* jsonChampsSupplementaires, PrioriteMessage priorite) {
  JsonDocument doc;
  if (jsonChampsSupplementaires && strlen(jsonChampsSupplementaires) > 0) {
    deserializeJson(doc, jsonChampsSupplementaires);
  }
  doc["alerte"] = nomAlerte;
  envoyerEnveloppe("alert", nomAlerte, doc, priorite);
}

void envoyerEtatMachine() {
  JsonDocument doc;

  xSemaphoreTake(mutexDonnees, portMAX_DELAY);

  EtatMachine etat = donnees.etat;
  bool connecte = donnees.connexionDisponible;
  bool erreurActive =
    donnees.defautCritique ||
    donnees.alerteActive ||
    donnees.causeArret != CAUSE_AUCUNE;

  ModeFonctionnement mode = donnees.modeFonctionnement;
  MatierePremiere matiere = donnees.matiereSelectionnee;
  ProduitFinal produit = donnees.produitSelectionne;

  char prochaineMaintenance[11];
  strncpy(
    prochaineMaintenance,
    donnees.dateProchaineMaintenance,
    sizeof(prochaineMaintenance)
  );
  prochaineMaintenance[sizeof(prochaineMaintenance) - 1] = '\0';

  xSemaphoreGive(mutexDonnees);

  doc["etat_machine"] = etatVersChaine(etat);

  // Ancien champ conservé pour ne pas casser le backend
  doc["connectivite"] = connecte ? "online" : "offline";

  // Nouveau champ destiné à l’affichage
  doc["etat_reseau"] = connecte ? "connecte" : "deconnecte";

  doc["mode_fonctionnement"] = modeVersChaine(mode);
  doc["matiere_selectionnee"] = matiereVersChaine(matiere);
  doc["produit_selectionne"] = produitVersChaine(produit);
  doc["erreur_active"] = erreurActive;

  if (strlen(prochaineMaintenance) > 0) {
    doc["date_prochaine_maintenance"] = prochaineMaintenance;
  } else {
    doc["date_prochaine_maintenance"] = nullptr;
  }

  envoyerEnveloppe(
    "status",
    "etat",
    doc,
    PRIORITE_HAUTE
  );
}

void envoyerProduction(const char* sousTopic, const char* jsonChampsSupplementaires) {
  JsonDocument doc;
  if (jsonChampsSupplementaires && strlen(jsonChampsSupplementaires) > 0) {
    deserializeJson(doc, jsonChampsSupplementaires);
  }
  envoyerEnveloppe("production", sousTopic, doc, PRIORITE_HAUTE);
}

void envoyerAckCommande(const char* commandId, const char* commande, const char* statut, const char* message) {
  JsonDocument doc;
  doc["command_id"] = commandId;
  doc["machine_id"] = MACHINE_ID;
  doc["machine_type"] = MACHINE_TYPE;
  char horodatage[24];
  obtenirHorodatage(horodatage, sizeof(horodatage));
  doc["timestamp"] = horodatage;
  doc["commande"] = commande;
  doc["statut"] = statut;
  xSemaphoreTake(mutexDonnees, portMAX_DELAY);
  doc["etat_machine"] = etatVersChaine(donnees.etat);
  xSemaphoreGive(mutexDonnees);
  doc["message"] = message;

  char payload[384];
  serializeJson(doc, payload);
  char topic[80];
  construireTopic("ack", "command", topic, sizeof(topic));

  ecrireMessageBuffer(commandId, topic, payload, "critique");
  if (estConnecteMQTT()) publierBrut(topic, payload);
}

void traiterAckEntrant(const char* messageId) {
  marquerMessageConfirme(messageId);
}