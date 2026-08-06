#ifndef PINS_H
#define PINS_H
#include <Arduino.h>

// ===================================================================
// AUCUNE BROCHE N'EST ENCORE DEFINITIVE - en attente du cablage reel
// Valeur 255 = placeholder invalide, a remplacer une fois le cablage termine
// Ce fichier est le SEUL a modifier quand les pins seront confirmees
// ===================================================================

// Bus I2C (DFR0627, DFR0553/ADS1115)
constexpr uint8_t PIN_I2C_SDA = 255;
constexpr uint8_t PIN_I2C_SCL = 255;

// Bus SPI (carte SD externe)
constexpr uint8_t PIN_SD_MOSI = 255;
constexpr uint8_t PIN_SD_MISO = 255;
constexpr uint8_t PIN_SD_SCK  = 255;
constexpr uint8_t PIN_SD_CS   = 255;

// Actionneurs
constexpr uint8_t PIN_PWM_POMPE = 255;
constexpr uint8_t PIN_COMMANDE_MOTEUR = 255; // vers le relais DFR0017

// Capteurs
constexpr uint8_t PIN_DS18B20 = 255;
constexpr uint8_t PIN_DEBITMETRE = 255;
constexpr uint8_t PIN_DEFAUT_MOTEUR = 255; // contact auxiliaire du relais thermique JR28-25

// Modem A7670E - a confirmer si ces pins sont vraiment libres (voir manquements notes)
constexpr uint8_t MODEM_TX_PIN = 26;
constexpr uint8_t MODEM_RX_PIN = 27;
constexpr uint8_t MODEM_PWRKEY_PIN = 4;
constexpr uint8_t MODEM_POWERON_PIN = 12;
constexpr uint8_t MODEM_RESET_PIN = 255;

constexpr uint8_t PIN_PZEM_RX = 255; // vers TX du PZEM
constexpr uint8_t PIN_PZEM_TX = 255; // vers RX du PZEM



#endif