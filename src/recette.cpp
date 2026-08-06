#include "recette.h"
#include <math.h>

#define BASE_REFERENCE_KG 5.0f

float calculerQuantiteEau_5kg(
  MatierePremiere matiere,
  ProduitFinal produit,
  float H
) {
  if (matiere == MATIERE_NON_SELECTIONNEE ||
      produit == PRODUIT_NON_SELECTIONNE) {
    return -1.0f;
  }

  switch (matiere) {
    case MATIERE_MIL:
      switch (produit) {
        case PRODUIT_ARRAW:
          return 1.3524f * H * H - 132.96f * H + 4037.9f;
        case PRODUIT_THIAKRY:
          return 1.4528f * H * H - 137.77f * H + 4001.0f;
        case PRODUIT_THIERE:
          return 1.3738f * H * H - 131.96f * H + 3771.8f;
        case PRODUIT_NON_SELECTIONNE:
          return -1.0f;
      }
      break;

    case MATIERE_MAIS:
      switch (produit) {
        case PRODUIT_ARRAW:
          return 4.0127f * H * H - 270.36f * H + 5651.2f;
        case PRODUIT_THIAKRY:
          return -2.2797f * H * H - 8.2058f * H + 2852.0f;
        case PRODUIT_THIERE:
          return -1.0f;
        case PRODUIT_NON_SELECTIONNE:
          return -1.0f;
      }
      break;

    case MATIERE_RIZ:
      // Formules non encore validées expérimentalement.
      return -1.0f;

    case MATIERE_NON_SELECTIONNEE:
      return -1.0f;
  }

  return -1.0f;
}

float calculerDebitCible_mLparMin(
  MatierePremiere matiere,
  ProduitFinal produit,
  float H,
  float debitDoseuse_kgMin
) {
  if (debitDoseuse_kgMin <= 0.0f || isnan(H)) {
    return -1.0f;
  }

  const float qBase5kg =
    calculerQuantiteEau_5kg(matiere, produit, H);

  if (qBase5kg < 0.0f) {
    return -1.0f;
  }

  return (qBase5kg / BASE_REFERENCE_KG) * debitDoseuse_kgMin;
}