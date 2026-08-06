#include "dgus.h"
#include "pins.h"
#include "etat.h"

#ifdef SIMULATION

// ============================================================
// MODE SIMULATION
// ============================================================

void initEcran() {}

void ecrireEcran(uint16_t vp, uint16_t valeur) {}

void gererReceptionEcran() {}

#else

#include "expandeur_uart.h"

// ============================================================
// INITIALISATION ECRAN
// ============================================================

void initEcran() {
  Serial.println(
    "[ECRAN] Liaison DGUS prete (via DFR0627 canal 1)"
  );
}

// ============================================================
// ECRITURE D'UNE VALEUR 16 BITS DANS UN VP DGUS
// ============================================================

void ecrireEcran(
  uint16_t vp,
  uint16_t valeur
) {
  uint8_t trame[8] = {
    0x5A,
    0xA5,
    0x05,
    0x82,

    static_cast<uint8_t>(vp >> 8),
    static_cast<uint8_t>(vp & 0xFF),

    static_cast<uint8_t>(valeur >> 8),
    static_cast<uint8_t>(valeur & 0xFF)
  };

  uartEcran().write(
    trame,
    sizeof(trame)
  );
}

// ============================================================
// DEMANDE DE LECTURE D'UN VP
// ============================================================

void demanderLectureEcran(
  uint16_t vp,
  uint8_t nombreMots
) {
  uint8_t trame[7] = {
    0x5A,
    0xA5,
    0x04,
    0x83,

    static_cast<uint8_t>(vp >> 8),
    static_cast<uint8_t>(vp & 0xFF),

    nombreMots
  };

  uartEcran().write(
    trame,
    sizeof(trame)
  );
}

// ============================================================
// TRAITEMENT D'UNE VALEUR RECUE DE L'ECRAN
// ============================================================

static void traiterVPRecu(
  uint16_t vp,
  uint16_t valeur
) {
  xSemaphoreTake(
    mutexDonnees,
    portMAX_DELAY
  );

  // ----------------------------------------------------------
  // DEMARRAGE
  // ----------------------------------------------------------

  if (
    vp == VP_BOUTON_DEMARRAGE &&
    valeur == 1
  ) {
    donnees.demandeDemarrage = true;

    Serial.println(
      "[HMI] Demarrage demande"
    );
  }

  // ----------------------------------------------------------
  // ARRET
  // ----------------------------------------------------------

  else if (
    vp == VP_BOUTON_STOP &&
    valeur == 1
  ) {
    donnees.demandeStop = true;

    Serial.println(
      "[HMI] Arret demande"
    );
  }

  // ----------------------------------------------------------
  // ACQUITTEMENT
  // ----------------------------------------------------------

  else if (
    vp == VP_BOUTON_ACQUIT &&
    valeur == 1
  ) {
    donnees.demandeAcquittement = true;

    Serial.println(
      "[HMI] Acquittement demande"
    );
  }

  // ----------------------------------------------------------
  // SELECTION MATIERE
  // 0 = aucune
  // 1 = mil
  // 2 = mais
  // 3 = riz
  // ----------------------------------------------------------

  else if (vp == VP_SELECTION_MATIERE) {

    if (
      valeur >= MATIERE_NON_SELECTIONNEE &&
      valeur <= MATIERE_RIZ
    ) {
      donnees.matiereSelectionnee =
        static_cast<MatierePremiere>(
          valeur
        );

      Serial.printf(
        "[HMI] Matiere selectionnee : %u\n",
        valeur
      );
    }
    else {
      Serial.printf(
        "[HMI] Matiere invalide recue : %u\n",
        valeur
      );
    }
  }

  // ----------------------------------------------------------
  // SELECTION PRODUIT
  // 0 = aucun
  // 1 = arraw
  // 2 = thiakry
  // 3 = thiere
  // ----------------------------------------------------------

  else if (vp == VP_SELECTION_PRODUIT) {

    if (
      valeur >= PRODUIT_NON_SELECTIONNE &&
      valeur <= PRODUIT_THIERE
    ) {
      donnees.produitSelectionne =
        static_cast<ProduitFinal>(
          valeur
        );

      Serial.printf(
        "[HMI] Produit selectionne : %u\n",
        valeur
      );
    }
    else {
      Serial.printf(
        "[HMI] Produit invalide recu : %u\n",
        valeur
      );
    }
  }

  xSemaphoreGive(
    mutexDonnees
  );
}

// ============================================================
// RECEPTION DES TRAMES DGUS
// ============================================================

void gererReceptionEcran() {

  static uint8_t buffer[32];
  static uint8_t indexBuffer = 0;

  while (uartEcran().available()) {

    uint8_t octet =
      static_cast<uint8_t>(
        uartEcran().read()
      );

    // --------------------------------------------------------
    // SYNCHRONISATION ENTETE 5A A5
    // --------------------------------------------------------

    if (
      indexBuffer == 0 &&
      octet != 0x5A
    ) {
      continue;
    }

    if (
      indexBuffer == 1 &&
      octet != 0xA5
    ) {
      indexBuffer = 0;
      continue;
    }

    // --------------------------------------------------------
    // PROTECTION CONTRE DEPASSEMENT BUFFER
    // --------------------------------------------------------

    if (
      indexBuffer >= sizeof(buffer)
    ) {
      indexBuffer = 0;
      continue;
    }

    buffer[indexBuffer++] = octet;

    // --------------------------------------------------------
    // ATTENDRE AU MOINS ENTETE + LONGUEUR
    // --------------------------------------------------------

    if (indexBuffer < 3) {
      continue;
    }

    uint8_t longueur =
      buffer[2];

    uint8_t tailleTotale =
      3 + longueur;

    // --------------------------------------------------------
    // TRAME TROP GRANDE
    // --------------------------------------------------------

    if (
      tailleTotale >
      sizeof(buffer)
    ) {
      Serial.println(
        "[HMI] Trame DGUS trop grande"
      );

      indexBuffer = 0;
      continue;
    }

    // --------------------------------------------------------
    // TRAME PAS ENCORE COMPLETE
    // --------------------------------------------------------

    if (
      indexBuffer <
      tailleTotale
    ) {
      continue;
    }

    // --------------------------------------------------------
    // COMMANDE 0x83 = DONNEE VP RECUE
    // --------------------------------------------------------

    if (
      buffer[3] == 0x83 &&
      longueur >= 6
    ) {

      uint16_t vpRecu =
        (
          static_cast<uint16_t>(
            buffer[4]
          ) << 8
        ) |
        buffer[5];

      uint8_t nombreMots =
        buffer[6];

      if (
        nombreMots >= 1 &&
        tailleTotale >= 9
      ) {

        uint16_t valeur =
          (
            static_cast<uint16_t>(
              buffer[7]
            ) << 8
          ) |
          buffer[8];

        traiterVPRecu(
          vpRecu,
          valeur
        );
      }
    }

    // --------------------------------------------------------
    // PREPARER LA PROCHAINE TRAME
    // --------------------------------------------------------

    indexBuffer = 0;
  }
}

#endif