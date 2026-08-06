#include "comm.h"
#include "config.h"
#include "pins.h"
#include "etat.h"
#include "recette.h"

#ifdef SIMULATION

void initComm() {}
bool connecterReseauCellulaire() { return true; }
bool connecterMQTT() { return true; }
void bouclerMQTT() {}
bool estConnecteMQTT() { return true; }
bool publierBrut(const char* topic, const char* payload) { return true; }
bool appliquerMiseAJourOTA(const char* url) { return true; }

#else

#include <TinyGsmClient.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>
#include <Update.h>
#include "protocole_mqtt.h"

HardwareSerial serialModem(1);
TinyGsm modem(serialModem);

static constexpr uint8_t MQTT_CLIENT_INDEX = 0;
static constexpr uint32_t MQTT_RX_BUFFER_SIZE = 4096;
static constexpr uint32_t MQTT_KEEPALIVE_SECONDS = 30;
static constexpr uint32_t MQTT_CONNECTION_CHECK_MS = 10000;

static bool mqttServiceInitialise = false;
static bool mqttPret = false;
static uint32_t dernierControleConnexionMQTT = 0;
static uint8_t echecsConnexionMQTT = 0;

char clientIdUnique[32];

static const char MQTT_CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

static bool convertirMatiere(
  const char* valeur,
  MatierePremiere& resultat
) {
  if (!valeur) return false;

  if (strcmp(valeur, "mil") == 0) {
    resultat = MATIERE_MIL;
    return true;
  }

  if (strcmp(valeur, "mais") == 0) {
    resultat = MATIERE_MAIS;
    return true;
  }

  if (strcmp(valeur, "riz") == 0) {
    resultat = MATIERE_RIZ;
    return true;
  }

  return false;
}

static bool convertirProduit(
  const char* valeur,
  ProduitFinal& resultat
) {
  if (!valeur) return false;

  if (strcmp(valeur, "arraw") == 0) {
    resultat = PRODUIT_ARRAW;
    return true;
  }

  if (strcmp(valeur, "thiakry") == 0) {
    resultat = PRODUIT_THIAKRY;
    return true;
  }

  if (strcmp(valeur, "thiere") == 0) {
    resultat = PRODUIT_THIERE;
    return true;
  }

  return false;
}

static bool appliquerRecetteDistante(
  MatierePremiere matiere,
  ProduitFinal produit,
  const char* cerealeStr,
  const char* produitStr
) {
  float humiditeActuelle;
  float debitDoseuseActuel;

  xSemaphoreTake(mutexDonnees, portMAX_DELAY);
  humiditeActuelle = donnees.humiditeFarine;
  debitDoseuseActuel = donnees.debitDoseuse_kgMin;
  xSemaphoreGive(mutexDonnees);

  const float debitCible =
    calculerDebitCible_mLparMin(
      matiere,
      produit,
      humiditeActuelle,
      debitDoseuseActuel
    );

  if (debitCible < 0.0f) {
    Serial.printf(
      "[CONFIG] Recette indisponible : %s + %s\n",
      cerealeStr,
      produitStr
    );
    return false;
  }

  xSemaphoreTake(mutexDonnees, portMAX_DELAY);
  donnees.matiereSelectionnee = matiere;
  donnees.produitSelectionne = produit;
  donnees.debitEauCible = debitCible;
  donnees.publicationEtatDemandee = true;
  xSemaphoreGive(mutexDonnees);

  sauvegarderDonneesPersistantes();

  Serial.printf(
    "[CONFIG] Cycle HMP accepte : cereale=%s produit=%s\n",
    cerealeStr,
    produitStr
  );

  Serial.printf(
    "[CALCUL] Humidite mesuree=%.2f %% | Debit eau cible=%.1f mL/min\n",
    humiditeActuelle,
    debitCible
  );

  return true;
}

void callbackCommande(
  const char* topic,
  const uint8_t* payload,
  uint32_t length
) {
  String message;

  for (uint32_t i = 0; i < length; i++) {
    message += static_cast<char>(payload[i]);
  }

  const String topicStr(topic);

  Serial.printf("[MQTT] Message recu sur %s\n", topic);
  Serial.printf("[MQTT] Payload recu : %s\n", message.c_str());

  // ============================================================
  // Configuration de recette envoyée par le backend
  // ============================================================
  if (topicStr.indexOf("/command/cycle_hmp") >= 0) {
    JsonDocument doc;

    if (deserializeJson(doc, message) != DeserializationError::Ok) {
      Serial.println("[CONFIG] Cycle HMP refuse : JSON invalide");
      return;
    }

    const char* cerealeStr = doc["cereale"];
    const char* produitStr = doc["mode"];

    MatierePremiere matiere = MATIERE_NON_SELECTIONNEE;
    ProduitFinal produit = PRODUIT_NON_SELECTIONNE;

    if (!convertirMatiere(cerealeStr, matiere)) {
      Serial.println("[CONFIG] Cycle HMP refuse : cereale inconnue");
      return;
    }

    if (!convertirProduit(produitStr, produit)) {
      Serial.println("[CONFIG] Cycle HMP refuse : produit inconnu");
      return;
    }

    appliquerRecetteDistante(
      matiere,
      produit,
      cerealeStr,
      produitStr
    );

    return;
  }

  // ============================================================
  // Démarrage
  // ============================================================
  if (topicStr.indexOf("/command/start") >= 0) {
    JsonDocument doc;

    if (deserializeJson(doc, message) != DeserializationError::Ok) {
      Serial.println("[COMMANDE] Start refuse : JSON invalide");
      return;
    }

    const char* commandId = doc["command_id"] | "inconnu";

    EtatMachine etat;
    bool defautCritique;
    bool alerteActive;
    bool stopDistant;
    bool acquittementRequis;
    CauseArret cause;
    MatierePremiere matiere;
    ProduitFinal produit;
    float humidite;
    float debitDoseuse;

    xSemaphoreTake(mutexDonnees, portMAX_DELAY);
    etat = donnees.etat;
    defautCritique = donnees.defautCritique;
    alerteActive = donnees.alerteActive;
    stopDistant = donnees.demandeStopDistant;
    acquittementRequis = donnees.acquittementLocalRequis;
    cause = donnees.causeArret;
    matiere = donnees.matiereSelectionnee;
    produit = donnees.produitSelectionne;
    humidite = donnees.humiditeFarine;
    debitDoseuse = donnees.debitDoseuse_kgMin;
    xSemaphoreGive(mutexDonnees);

    const bool recetteSelectionnee =
      matiere != MATIERE_NON_SELECTIONNEE &&
      produit != PRODUIT_NON_SELECTIONNE;

    const bool recetteValide =
      recetteSelectionnee &&
      calculerDebitCible_mLparMin(
        matiere,
        produit,
        humidite,
        debitDoseuse
      ) >= 0.0f;

    const bool demarrageAutorise =
      etat == ETAT_MACHINE_PRETE &&
      !defautCritique &&
      !alerteActive &&
      !stopDistant &&
      !acquittementRequis &&
      cause == CAUSE_AUCUNE &&
      recetteValide;

    if (demarrageAutorise) {
      xSemaphoreTake(mutexDonnees, portMAX_DELAY);
      donnees.demandeDemarrage = true;
      xSemaphoreGive(mutexDonnees);

      Serial.println("[COMMANDE] Lancement du cycle accepte");

      envoyerAckCommande(
        commandId,
        "start",
        "executee",
        "Lancement du cycle accepte"
      );

      return;
    }

    const char* raison = "machine_non_prete";

    if (!recetteSelectionnee) {
      raison = "recette_non_selectionnee";
    }
    else if (!recetteValide) {
      raison = "recette_invalide";
    }
    else if (
      acquittementRequis ||
      defautCritique ||
      stopDistant ||
      cause != CAUSE_AUCUNE ||
      etat == ETAT_ARRET_SECURISE ||
      etat == ETAT_ALERTE_ACQUITTEMENT
    ) {
      raison = "acquittement_local_requis";
    }
    else if (alerteActive) {
      raison = "alerte_active";
    }

    Serial.printf(
      "[COMMANDE] Lancement du cycle refuse : %s\n",
      raison
    );

    envoyerAckCommande(
      commandId,
      "start",
      "refusee",
      raison
    );

    return;
  }

  // ============================================================
  // Arrêt distant
  // ============================================================
  if (topicStr.indexOf("/command/stop") >= 0) {
    JsonDocument doc;

    if (deserializeJson(doc, message) != DeserializationError::Ok) {
      Serial.println("[COMMANDE] Stop refuse : JSON invalide");
      return;
    }

    const char* commandId = doc["command_id"] | "inconnu";

    xSemaphoreTake(mutexDonnees, portMAX_DELAY);
    donnees.demandeStopDistant = true;

    strncpy(
      donnees.commandIdStopEnAttente,
      commandId,
      sizeof(donnees.commandIdStopEnAttente) - 1
    );

    donnees.commandIdStopEnAttente[
      sizeof(donnees.commandIdStopEnAttente) - 1
    ] = '\0';

    xSemaphoreGive(mutexDonnees);

    Serial.println("[COMMANDE] Stop distant recu");
    return;
  }

  // ============================================================
  // Accusé d'ingestion
  // ============================================================
  if (topicStr.indexOf("/ack/ingestion") >= 0) {
    JsonDocument doc;

    if (deserializeJson(doc, message) != DeserializationError::Ok) {
      return;
    }

    const char* messageId = doc["message_id"];
    const char* statut = doc["status"];

    if (messageId && statut && strcmp(statut, "stored") == 0) {
      traiterAckEntrant(messageId);
    }

    return;
  }

  // ============================================================
  // Mise à jour OTA
  // ============================================================
  if (topicStr.indexOf("/command/ota") >= 0) {
    JsonDocument doc;

    if (deserializeJson(doc, message) != DeserializationError::Ok) {
      Serial.println("[OTA] Commande refusee : JSON invalide");
      return;
    }

    const char* url = doc["url"];

    if (!url) {
      Serial.println("[OTA] Commande refusee : URL manquante");
      return;
    }

    Serial.printf("[OTA] Mise a jour demandee: %s\n", url);

    if (appliquerMiseAJourOTA(url)) {
      Serial.println("[OTA] Succes, redemarrage dans 3s...");
      delay(3000);
      ESP.restart();
    }
    else {
      Serial.println(
        "[OTA] Echec de la mise a jour, firmware actuel conserve"
      );
    }

    return;
  }

  // ============================================================
  // Ancien topic conservé pour compatibilité
  // ============================================================
  if (topicStr.indexOf("/config/recette") >= 0) {
    JsonDocument doc;

    if (deserializeJson(doc, message) != DeserializationError::Ok) {
      Serial.println("[CONFIG] Recette refusee : JSON invalide");
      return;
    }

    const char* matiereStr = doc["matiere"];
    const char* produitStr = doc["produit"];

    MatierePremiere matiere = MATIERE_NON_SELECTIONNEE;
    ProduitFinal produit = PRODUIT_NON_SELECTIONNE;

    if (!convertirMatiere(matiereStr, matiere) ||
        !convertirProduit(produitStr, produit)) {
      Serial.println("[CONFIG] Recette distante refusee");
      return;
    }

    appliquerRecetteDistante(
      matiere,
      produit,
      matiereStr,
      produitStr
    );
  }
}

static void reinitialiserEtatMQTT() {
  mqttPret = false;
  dernierControleConnexionMQTT = 0;
}

static bool abonnerTopicsMQTT() {

  char topicAckIngestion[100];
  char topicCommandStop[100];
  char topicCommandStart[100];
  char topicConfigRecette[100];

  snprintf(
    topicAckIngestion,
    sizeof(topicAckIngestion),
    "arrawtech/%s/%s/ack/ingestion",
    MACHINE_TYPE,
    MACHINE_ID
  );

  char topicCycleHmp[100];

snprintf(
  topicCycleHmp,
  sizeof(topicCycleHmp),
  "arrawtech/%s/%s/command/cycle_hmp",
  MACHINE_TYPE,
  MACHINE_ID
);

  snprintf(
    topicCommandStop,
    sizeof(topicCommandStop),
    "arrawtech/%s/%s/command/stop",
    MACHINE_TYPE,
    MACHINE_ID
  );

  snprintf(
    topicCommandStart,
    sizeof(topicCommandStart),
    "arrawtech/%s/%s/command/start",
    MACHINE_TYPE,
    MACHINE_ID
  );

  snprintf(
    topicConfigRecette,
    sizeof(topicConfigRecette),
    "arrawtech/%s/%s/config/recette",
    MACHINE_TYPE,
    MACHINE_ID
  );

  auto abonner = [](const char* nom,
                    const char* topic,
                    uint8_t qos) -> bool {

    for (int tentative = 1; tentative <= 3; tentative++) {

      Serial.printf(
        "[COMM] Souscription %s tentative %d/3\n",
        nom,
        tentative
      );

      if (modem.mqtt_subscribe(
            MQTT_CLIENT_INDEX,
            topic,
            qos
          )) {

        Serial.printf(
          "[COMM] Souscription %s active\n",
          nom
        );

        delay(1000);
        return true;
      }

      delay(1500);
    }

    return false;
  };

  // Commandes importantes en QoS 1
  bool stopOk =
    abonner("STOP", topicCommandStop, 1);

  bool startOk =
    abonner("START", topicCommandStart, 1);

  bool recetteOk =
    abonner("RECETTE", topicConfigRecette, 1);

  bool cycleOk =
  abonner("CYCLE HMP", topicCycleHmp, 1);

  // ACK ingestion peut rester en QoS 0
  // DEMO : ACK ingestion temporairement desactive
// pour eviter la saturation des messages entrants

Serial.println(
  "[COMM] ACK ingestion desactive pour la demo"
);

return stopOk &&
       startOk &&
       recetteOk &&
       cycleOk;
}
void initComm() {
  const uint64_t mac = ESP.getEfuseMac();

  snprintf(
    clientIdUnique,
    sizeof(clientIdUnique),
    "%s%04X",
    MQTT_CLIENT_ID_PREFIX,
    static_cast<uint16_t>(mac & 0xFFFF)
  );

  serialModem.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(3000);

  pinMode(MODEM_PWRKEY_PIN, OUTPUT);
  pinMode(MODEM_POWERON_PIN, OUTPUT);
  digitalWrite(MODEM_POWERON_PIN, HIGH);

  digitalWrite(MODEM_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(MODEM_PWRKEY_PIN, HIGH);
  delay(1000);
  digitalWrite(MODEM_PWRKEY_PIN, LOW);

  Serial.println("[COMM] Initialisation du modem A7670E...");

  bool modemInitialise = false;

for (uint8_t tentative = 1; tentative <= 5; tentative++) {
  Serial.printf(
    "[COMM] Tentative initialisation modem %u/5...\n",
    tentative
  );

  if (modem.init()) {
    modemInitialise = true;
    break;
  }

  Serial.println("[COMM] Modem pas encore pret");
  delay(3000);
}

if (!modemInitialise) {
  Serial.println("[COMM] Echec definitif d'initialisation du modem");
  return;
}

Serial.printf(
  "[COMM] Modem detecte: %s\n",
  modem.getModemInfo().c_str()
);
  if (!modem.mqtt_set_rx_buffer_size(MQTT_RX_BUFFER_SIZE)) {
    Serial.println("[COMM] Echec allocation buffer MQTT");
  }

  modem.mqtt_set_callback(callbackCommande);
  reinitialiserEtatMQTT();
}

bool connecterReseauCellulaire() {
  if (!modem.isNetworkConnected()) {
    reinitialiserEtatMQTT();
    Serial.println("[COMM] Recherche du reseau cellulaire...");

    if (!modem.waitForNetwork(60000L, true)) {
      Serial.println("[COMM] Reseau non trouve");
      return false;
    }

    Serial.println("[COMM] Enregistrement reseau reussi");
  }

  if (!modem.isGprsConnected()) {
    reinitialiserEtatMQTT();
    Serial.println("[COMM] Ouverture de la connexion de donnees...");

    if (!modem.gprsConnect(APN, APN_USER, APN_PASS)) {
      Serial.println("[COMM] Echec de connexion GPRS/4G");
      return false;
    }

    Serial.println("[COMM] Connexion cellulaire etablie");
    Serial.print("[COMM] Date et heure du modem : ");
    Serial.println(modem.getGSMDateTime(DATE_FULL));
  }

  return true;
}

bool connecterMQTT() {
  if (mqttPret) {
    return true;
  }

  if (!modem.isNetworkConnected() || !modem.isGprsConnected()) {
    Serial.println(
      "[COMM] Connexion MQTT impossible: reseau cellulaire indisponible"
    );

    reinitialiserEtatMQTT();
    return false;
  }

  Serial.println(
    "[COMM] Connexion au broker MQTT integre A7670E..."
  );

  bool serviceDemarreMaintenant = false;

  if (!mqttServiceInitialise) {
    // Démarrage du service MQTT interne avec TLS et SNI
    if (!modem.mqtt_begin(true, true)) {
      Serial.println(
        "[COMM] Echec initialisation du service MQTT du modem"
      );

      reinitialiserEtatMQTT();
      return false;
    }

    modem.mqtt_set_certificate(MQTT_CA_CERT);
    modem.mqtt_set_callback(callbackCommande);

    mqttServiceInitialise = true;
    serviceDemarreMaintenant = true;
  }

  /*
   * En cas de reconnexion, nettoyer l'ancienne session MQTT
   * sans arrêter complètement le service MQTT du modem.
   */
  if (!serviceDemarreMaintenant) {
    if (modem.mqtt_connected(MQTT_CLIENT_INDEX)) {
      Serial.println(
        "[COMM] Nettoyage de l'ancienne session MQTT..."
      );

      modem.mqtt_disconnect(MQTT_CLIENT_INDEX);
      delay(1000);
    }
    else {
      Serial.println(
        "[COMM] Aucune ancienne session MQTT a nettoyer"
      );
    }
  }

  const bool connexionOk = modem.mqtt_connect(
    MQTT_CLIENT_INDEX,
    MQTT_BROKER_HOST,
    MQTT_BROKER_PORT,
    clientIdUnique,
    MQTT_USER,
    MQTT_PASSWORD,
    MQTT_KEEPALIVE_SECONDS
  );

  if (!connexionOk) {
  echecsConnexionMQTT++;

  Serial.printf(
    "[COMM] Echec connexion MQTT (%u/3)\n",
    echecsConnexionMQTT
  );

  mqttPret = false;

  if (echecsConnexionMQTT >= 3) {
    Serial.println(
      "[COMM] Trois echecs MQTT : redemarrage complet du modem..."
    );

    modem.restart();

    mqttServiceInitialise = false;
    mqttPret = false;
    echecsConnexionMQTT = 0;

    delay(3000);
  }

  return false;
}

// La connexion a réussi
echecsConnexionMQTT = 0;

  if (!abonnerTopicsMQTT()) {
    Serial.println(
      "[COMM] Connexion MQTT etablie, mais souscriptions incompletes"
    );

    modem.mqtt_disconnect(MQTT_CLIENT_INDEX);
    reinitialiserEtatMQTT();
    return false;
  }

  mqttPret = true;
  dernierControleConnexionMQTT = millis();

  Serial.println(
    "[COMM] Connecte au broker MQTT, souscriptions actives"
  );

  return true;
}

void bouclerMQTT() {
  if (!mqttServiceInitialise) return;

  modem.mqtt_handle(200);

  if (mqttPret && millis() - dernierControleConnexionMQTT >= MQTT_CONNECTION_CHECK_MS) {
    dernierControleConnexionMQTT = millis();

    if (!modem.mqtt_connected(MQTT_CLIENT_INDEX)) {
      Serial.println("[COMM] Connexion MQTT perdue");
      reinitialiserEtatMQTT();
    }
  }
}

bool estConnecteMQTT() {
  return mqttPret;
}

bool publierBrut(const char* topic, const char* payload) {
  if (!mqttPret) {
    Serial.printf("[MQTT] Publication impossible, MQTT deconnecte : %s\n", topic);
    return false;
  }

  const bool resultat = modem.mqtt_publish(
    MQTT_CLIENT_INDEX, topic, payload, 0, 60, 0
  );

  Serial.printf("[MQTT] PUB %s -> %s\n", topic, resultat ? "OK" : "ECHEC");

  if (!resultat) {
    Serial.printf("[MQTT] Taille payload : %u octets\n", strlen(payload));
    reinitialiserEtatMQTT();
  }

  return resultat;
}

bool appliquerMiseAJourOTA(const char* url) {
  String urlStr(url);
  if (!urlStr.startsWith("https://")) {
    Serial.println("[OTA] URL invalide, https:// requis");
    return false;
  }
  String reste = urlStr.substring(8);
  int indexSlash = reste.indexOf('/');
  String hote = (indexSlash == -1) ? reste : reste.substring(0, indexSlash);
  String chemin = (indexSlash == -1) ? "/" : reste.substring(indexSlash);

  TinyGsmClientSecure clientOTA(modem, SSL_CONTEXT_ID + 1);
  HttpClient http(clientOTA, hote.c_str(), 443);

  Serial.println("[OTA] Telechargement en cours...");
  int erreurGet = http.get(chemin.c_str());
  if (erreurGet != 0) {
    Serial.printf("[OTA] Echec de connexion HTTP, code: %d\n", erreurGet);
    return false;
  }

  int statusCode = http.responseStatusCode();
  if (statusCode != 200) {
    Serial.printf("[OTA] Reponse HTTP inattendue: %d\n", statusCode);
    http.stop();
    return false;
  }

  int tailleContenu = http.contentLength();
  if (tailleContenu <= 0) {
    Serial.println("[OTA] Taille de firmware invalide ou inconnue");
    http.stop();
    return false;
  }

  if (!Update.begin(tailleContenu)) {
    Serial.println("[OTA] Espace flash insuffisant pour cette mise a jour");
    http.stop();
    return false;
  }

  uint8_t buffer[512];
  size_t totalEcrit = 0;
  uint32_t dernierProgres = millis();

  while (http.connected() && totalEcrit < (size_t)tailleContenu) {
    size_t disponible = http.available();
    if (disponible > 0) {
      size_t aLire = disponible < sizeof(buffer) ? disponible : sizeof(buffer);
      size_t lu = http.readBytes(buffer, aLire);
      Update.write(buffer, lu);
      totalEcrit += lu;

      if (millis() - dernierProgres > 2000) {
        Serial.printf("[OTA] Progres: %u / %d octets\n", totalEcrit, tailleContenu);
        dernierProgres = millis();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  http.stop();

  if (totalEcrit != (size_t)tailleContenu) {
    Serial.println("[OTA] Telechargement incomplet");
    Update.abort();
    return false;
  }

  if (!Update.end(true)) {
    Serial.printf("[OTA] Erreur finalisation: %s\n", Update.errorString());
    return false;
  }

  return true;
}


#endif