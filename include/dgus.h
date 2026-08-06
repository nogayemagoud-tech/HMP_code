#ifndef DGUS_H
#define DGUS_H

#include <Arduino.h>

// ============================================================
// TABLE VP OFFICIELLE - HMP
// ============================================================

// ------------------------------------------------------------
// RECETTE / SELECTION OPERATEUR
// Ecrits par l'ecran, lus par l'ESP32
// ------------------------------------------------------------
constexpr uint16_t VP_SELECTION_MATIERE = 0x1010;
constexpr uint16_t VP_SELECTION_PRODUIT = 0x1012;


// ------------------------------------------------------------
// MESURES
// Ecrites par l'ESP32, affichees par l'ecran
// ------------------------------------------------------------
constexpr uint16_t VP_HUMIDITE          = 0x2000;
constexpr uint16_t VP_DEBIT_EAU         = 0x2002;
constexpr uint16_t VP_TEMP_MOTEUR       = 0x2004;
constexpr uint16_t VP_PRODUCTION_KG     = 0x2006;

constexpr uint16_t VP_TENSION_RESEAU    = 0x2008;
constexpr uint16_t VP_COURANT_MOTEUR    = 0x200A;
constexpr uint16_t VP_PUISSANCE         = 0x200C;
constexpr uint16_t VP_ENERGIE           = 0x200E;
constexpr uint16_t VP_FREQUENCE_RESEAU  = 0x2010;

constexpr uint16_t VP_EAU_JOURNALIERE   = 0x2012;
constexpr uint16_t VP_TEMPS_TRAVAIL     = 0x2014;
constexpr uint16_t VP_DEBIT_CIBLE       = 0x2016;


// ------------------------------------------------------------
// COMMANDES OPERATEUR
// Ecrites par l'ecran, lues par l'ESP32
// ------------------------------------------------------------
constexpr uint16_t VP_BOUTON_DEMARRAGE = 0x3000;
constexpr uint16_t VP_BOUTON_STOP      = 0x3002;
constexpr uint16_t VP_BOUTON_ACQUIT    = 0x3004;


// ------------------------------------------------------------
// ETAT MACHINE / ALARMES
// ------------------------------------------------------------
constexpr uint16_t VP_ETAT_MACHINE      = 0x4000;
constexpr uint16_t VP_CAUSE_ARRET       = 0x4002;
constexpr uint16_t VP_ALERTE_ACTIVE     = 0x4004;
constexpr uint16_t VP_DEFAUT_CRITIQUE   = 0x4006;


// ------------------------------------------------------------
// SYNOPTIQUE PROCESS
// ------------------------------------------------------------
constexpr uint16_t VP_PROCESS_ACTIF     = 0x4100;
constexpr uint16_t VP_POMPE_ACTIVE      = 0x4102;


// ------------------------------------------------------------
// ETATS TECHNIQUES
// ------------------------------------------------------------
constexpr uint16_t VP_CONNEXION_RESEAU  = 0x4200;
constexpr uint16_t VP_DEFAUT_MOTEUR     = 0x4202;
constexpr uint16_t VP_DEFAUT_ENERGIE    = 0x4204;


// ============================================================
// FONCTIONS DGUS
// ============================================================

void initEcran();

void ecrireEcran(
  uint16_t vp,
  uint16_t valeur
);

void gererReceptionEcran();

#endif