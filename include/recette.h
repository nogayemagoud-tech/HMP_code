#ifndef RECETTE_H
#define RECETTE_H

enum MatierePremiere {
  MATIERE_NON_SELECTIONNEE,
  MATIERE_MIL,
  MATIERE_MAIS,
  MATIERE_RIZ
};

enum ProduitFinal {
  PRODUIT_NON_SELECTIONNE,
  PRODUIT_ARRAW,
  PRODUIT_THIAKRY,
  PRODUIT_THIERE
};

float calculerQuantiteEau_5kg(
  MatierePremiere matiere,
  ProduitFinal produit,
  float humidite
);

float calculerDebitCible_mLparMin(
  MatierePremiere matiere,
  ProduitFinal produit,
  float humidite,
  float debitDoseuse_kgMin
);

#endif