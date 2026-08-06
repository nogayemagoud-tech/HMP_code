#include "buffer_sd.h"
#include "pins.h"
#include "etat.h"

#ifdef SIMULATION

bool initBufferSD() { return true; }
bool ecrireMessageBuffer(const char*, const char*, const char*, const char*) { return true; }
void marquerMessageConfirme(const char*) {}
void republierMessagesEnAttente() {}
void enregistrerDonneesHorsLigne() {}
void synchroniserDonneesHorsLigne() {}
uint32_t nombreEnregistrementsEnAttente() { return 0; }
uint8_t pourcentageUtiliseSD() { return 0; }

#else

#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "comm.h"
#include "config.h"

constexpr char FICHIER_QUEUE[] = "/offline_queue/queue.log";
constexpr char FICHIER_INDEX[] = "/offline_queue/index.json";

static SPIClass spiSD(VSPI);
static bool bufferSDPret = false;

bool initBufferSD() {
  bufferSDPret = false;

  spiSD.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  if (!SD.begin(PIN_SD_CS, spiSD)) {
    Serial.println("[BUFFER] Carte SD non detectee");
    return false;
  }

  if (!SD.exists("/offline_queue")) {
    SD.mkdir("/offline_queue");
  }

  bufferSDPret = true;
  Serial.println("[BUFFER] Carte SD initialisee");
  return true;
}

static void mettreAJourIndex(int deltaEnAttente) {
  JsonDocument doc;
  if (SD.exists(FICHIER_INDEX)) {
    File f = SD.open(FICHIER_INDEX, FILE_READ);
    if (f) { deserializeJson(doc, f); f.close(); }
  }
  doc["machine_id"] = MACHINE_ID;
  int pending = doc["pending_count"] | 0;
  pending += deltaEnAttente;
  if (pending < 0) pending = 0;
  doc["pending_count"] = pending;
  doc["storage_status"] = "ok";

  File f = SD.open(FICHIER_INDEX, FILE_WRITE);
  if (f) { serializeJson(doc, f); f.close(); }
}

uint8_t pourcentageUtiliseSD() {
  uint64_t total = SD.totalBytes();
  uint64_t utilise = SD.usedBytes();
  if (total == 0) return 0;
  return (uint8_t)((utilise * 100) / total);
}

bool ecrireMessageBuffer(const char* messageId, const char* topic, const char* payloadJson, const char* priorite) {
    if (!bufferSDPret) {
  return false;
}
    uint8_t usage = pourcentageUtiliseSD();

  if (usage >= 95 && strcmp(priorite, "critique") != 0) {
    Serial.println("[BUFFER] SD presque pleine (>95%) - message non critique ignore");
    return false;
  }
  if (usage >= 80 && usage < 95) {
    Serial.printf("[BUFFER] Alerte stockage: SD utilisee a %u%%\n", usage);
  }

  File f = SD.open(FICHIER_QUEUE, FILE_APPEND);
  if (!f) return false;

  JsonDocument doc;
  deserializeJson(doc, payloadJson);
  doc["message_id"] = messageId;
  doc["topic"] = topic;
  doc["priority"] = priorite;
  doc["status"] = "pending";
  doc["retry_count"] = 0;

  serializeJson(doc, f);
  f.println();
  f.close();

  mettreAJourIndex(+1);
  return true;
}

void marquerMessageConfirme(const char* messageId) {
  if (!SD.exists(FICHIER_QUEUE)) return;

  File source = SD.open(FICHIER_QUEUE, FILE_READ);
  if (!source) return;

  File temp = SD.open("/offline_queue/queue_tmp.log", FILE_WRITE);
  if (!temp) { source.close(); return; }

  bool trouve = false;
  while (source.available()) {
    String ligne = source.readStringUntil('\n');
    if (ligne.length() == 0) continue;
    JsonDocument doc;
    if (deserializeJson(doc, ligne) == DeserializationError::Ok) {
      const char* id = doc["message_id"];
      if (id && strcmp(id, messageId) == 0) {
        trouve = true;
        continue;
      }
    }
    temp.println(ligne);
  }
  source.close();
  temp.close();

  SD.remove(FICHIER_QUEUE);
  SD.rename("/offline_queue/queue_tmp.log", FICHIER_QUEUE);

  if (trouve) {
    mettreAJourIndex(-1);
    Serial.printf("[BUFFER] Message %s confirme et retire de la SD\n", messageId);
  }
}

void republierMessagesEnAttente() {
  if (!SD.exists(FICHIER_QUEUE)) return;
  if (!estConnecteMQTT()) return;

  File source = SD.open(FICHIER_QUEUE, FILE_READ);
  if (!source) return;

  File temp = SD.open("/offline_queue/queue_tmp.log", FILE_WRITE);
  if (!temp) { source.close(); return; }

  while (source.available()) {
    String ligne = source.readStringUntil('\n');
    if (ligne.length() == 0) continue;

    JsonDocument doc;
    if (deserializeJson(doc, ligne) != DeserializationError::Ok) continue;

    const char* topic = doc["topic"];
    int retry = doc["retry_count"] | 0;
    doc["retry_count"] = retry + 1;

    char payloadMisAJour[512];
    serializeJson(doc, payloadMisAJour);

    if (topic) publierBrut(topic, payloadMisAJour);

    serializeJson(doc, temp);
    temp.println();
  }
  source.close();
  temp.close();

  SD.remove(FICHIER_QUEUE);
  SD.rename("/offline_queue/queue_tmp.log", FICHIER_QUEUE);
}

void enregistrerDonneesHorsLigne() {}
void synchroniserDonneesHorsLigne() { republierMessagesEnAttente(); }
uint32_t nombreEnregistrementsEnAttente() { return 0; }

#endif