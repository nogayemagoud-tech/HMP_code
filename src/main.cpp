#include <Arduino.h>

// Handles des tâches
TaskHandle_t hSecurite, hAcquisition, hCalcul, hCommande, hHmi, hMqtt, hBufferOffline, hWatchdog;

// Prototypes (corps vide pour l'instant — on les remplira étape par étape)
void Task_Securite(void *pv) {
  for (;;) {
    Serial.println("Task_Securite running");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void Task_Acquisition_Capteurs(void *pv) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void Task_Calcul_Humidification(void *pv) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void Task_Commande_Actionneurs(void *pv) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void Task_HMI(void *pv) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void Task_Communication_MQTT(void *pv) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void Task_Buffer_Offline(void *pv) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void Task_Watchdog(void *pv) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("HMP - demarrage FreeRTOS");

  // Priorités conformes à la section 7 du document (plus le chiffre est haut, plus c'est prioritaire)
  xTaskCreatePinnedToCore(Task_Securite,              "Securite",   4096, NULL, 8, &hSecurite,     1);
  xTaskCreatePinnedToCore(Task_Commande_Actionneurs,  "Commande",   4096, NULL, 7, &hCommande,     1);
  xTaskCreatePinnedToCore(Task_Acquisition_Capteurs,  "Acquisition",4096, NULL, 6, &hAcquisition,  1);
  xTaskCreatePinnedToCore(Task_Calcul_Humidification, "Calcul",     4096, NULL, 5, &hCalcul,       1);
  xTaskCreatePinnedToCore(Task_HMI,                   "HMI",        4096, NULL, 4, &hHmi,          0);
  xTaskCreatePinnedToCore(Task_Communication_MQTT,    "MQTT",       4096, NULL, 3, &hMqtt,         0);
  xTaskCreatePinnedToCore(Task_Buffer_Offline,        "Buffer",     4096, NULL, 2, &hBufferOffline,0);
  xTaskCreatePinnedToCore(Task_Watchdog,              "Watchdog",   2048, NULL, 1, &hWatchdog,     0);
}

void loop() {
  vTaskDelete(NULL); // tout tourne dans les tâches FreeRTOS, plus rien à faire ici
}