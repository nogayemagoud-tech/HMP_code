#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>

// PWM pompe
constexpr uint8_t CANAL_PWM_POMPE = 0;
constexpr uint32_t FREQUENCE_PWM_POMPE_HZ = 1000;
constexpr uint8_t RESOLUTION_PWM_BITS = 8;

constexpr uint8_t PWM_AMORCAGE = 60;
constexpr uint32_t DUREE_AMORCAGE_MS = 5000;
constexpr uint8_t PWM_MIN_REGULATION = 20;
constexpr uint8_t PWM_MAX_REGULATION = 255;

// Regulation PID de la pompe - Ki et Kd a 0 par defaut
constexpr float KP_DEBIT_PWM = 40.0f;
constexpr float KI_DEBIT_PWM = 0.0f;
constexpr float KD_DEBIT_PWM = 0.0f;
constexpr float INTEGRALE_MAX = 100.0f;

// Niveaux logiques actionneurs
constexpr bool NIVEAU_ACTIF_MOTEUR = HIGH;

// Seuils de securite
constexpr float SEUIL_TEMP_ALERTE_C = 75.0f;
constexpr float SEUIL_TEMP_ARRET_C = 90.0f;
constexpr uint32_t DUREE_MAX_SANS_DEBIT_MS = 10000;
constexpr float SEUIL_DEBIT_MIN_LMIN = 0.10f;
constexpr float TOLERANCE_DEBIT_RELATIVE = 0.20f;

// Debitmetre - A CALIBRER avec le vrai capteur
constexpr float DEBITMETRE_PULSES_PAR_LITRE = 450.0f;

// AR991 via ADS1115
constexpr uint8_t ADS1115_CANAL_HUMIDITE = 0;

// Production / horloge
constexpr int ANNEE_MINIMALE_VALIDE = 2024;

// Reseau cellulaire - Orange Senegal
constexpr char APN[] = "internet";
constexpr char APN_USER[] = "";
constexpr char APN_PASS[] = "";
constexpr uint32_t MODEM_BAUD = 115200;

// MQTT / TLS (plafond reel du modem: TLS 1.2)
constexpr char MQTT_BROKER_HOST[] = "mqtt.arrawtech.com";
constexpr uint16_t MQTT_BROKER_PORT = 8883;
constexpr char MQTT_CLIENT_ID_PREFIX[] = "hmp_";
constexpr uint8_t SSL_CONTEXT_ID = 1;
constexpr uint8_t SSL_VERSION_TLS12 = 3;

// Authentification MQTT - VERIFIE que ce mot de passe correspond EXACTEMENT
// a celui configure cote serveur avec mosquitto_passwd
constexpr char MQTT_USER[] = "hmp_arrawtech";
constexpr char MQTT_PASSWORD[] = "O+6+Jk1AuMF4g75vZuL57XG6";

constexpr uint32_t INTERVALLE_PUBLICATION_MS = 5000;
constexpr uint32_t DELAI_RECONNEXION_MS = 5000;

constexpr char FIRMWARE_VERSION[] = "1.0.0";

// Identifiants machine (protocole topics arrawtech/{type}/{id}/{categorie}/{parametre})
constexpr char MACHINE_ID[] = "ARWT-HMP-07-26-004";
constexpr char MACHINE_TYPE[] = "hmp";

#endif