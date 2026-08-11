#include <Wire.h>
#include "config.h"
#include "nfc_module.h"
#include "sensors_module.h"
#include "ia_inference.h"
#include "display_module.h"

// ============================================================================
// DEFINITION DES ETATS DE LA MACHINE
// ============================================================================


// Variable d'état courante (On commence par l'attente NFC)
MedBoxState currentState = STATE_WAIT_NFC;

// ============================================================================
// ALLOCATION MEMOIRE UNIQUE (SRAM)
// ============================================================================
// L'ESP32 réserve ici l'espace mémoire pour stocker les structures de données
PatientData currentPatient;

// Variable de contrôle pour le timing de l'affichage du diagnostic
unsigned long displayStartTime = 0;
const unsigned long RESULT_TIMEOUT = 10000; // Affichage du diagnostic pendant 10 secondes

// ============================================================================
// SETUP : INITIALISATION MATÉRIELLE
// ============================================================================
void setup() {
    Serial.begin(115200);
    while (!Serial); // Optionnel : attend l'ouverture du moniteur série
    
    Serial.println("--- DEMARRAGE DE MEDBOX AI ---");

    // 1. Initialisation du bus I2C (Écran, Capteurs, PN532 selon ton câblage)
    Wire.begin();

    // 2. Initialisation de chaque bloc modulaire
    if (!initDisplayModule()) {
        Serial.println("❌ Erreur critique : Ecran OLED non fonctionnel.");
        while(1);
    }
    
    initSensors();          // Initialise MLX90614, MAX30102 et le Buzzer
    initNFC();        // Initialise le lecteur PN532
    
    initTinyML();

    Serial.println("🚀 MedBox AI est totalement operationnel !");
    displaySplashWait(); // Affiche "Veuillez scanner un badge NFC..."
}

// ============================================================================
// LOOP : MACHINE À ÉTATS PRINCIPALE
// ============================================================================
void loop() {
    
    switch (currentState) {
        
        // --------------------------------------------------------------------
        // ÉTAT 1 : ATTENTE DU BADGE NFC (POUR L'ÂGE)
        // --------------------------------------------------------------------
        case STATE_WAIT_NFC: {
            float extractedAge = 0.0;
            String extractedUID = "";
            
            // On vérifie de manière non-bloquante si une carte se présente
            if (checkForNFCBadge(extractedAge, extractedUID)) {
                // Une carte a été lue avec succès ! On stocke l'âge et l'UID dans la RAM
                currentPatient.age = extractedAge;
                currentPatient.uid = extractedUID;
                
                Serial.print("👤 Patient identifie. Age recupere : ");
                Serial.print(currentPatient.age);
                Serial.println(" ans.");
                
                // Confirmation visuelle immédiate à l'écran (sinon l'OLED reste
                // bloqué sur "Veuillez scanner..." jusqu'à la fin de la mesure des capteurs)
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 16);
                display.println("[ BADGE RECONNU ]");
                display.setCursor(0, 32);
                display.print("Age : ");
                display.print((int)currentPatient.age);
                display.println(" ans");
                display.setCursor(0, 48);
                display.println("Preparation...");
                display.display();

                // Confirmation sonore : un bip court signale la reconnaissance du badge
                beep(150);

                delay(2000); // Laisse 2 secondes pour lire la confirmation à l'écran

                // Transition : On passe immédiatement à la lecture des capteurs
                currentState = STATE_READ_SENSORS;
            }
            break;
        }

        // --------------------------------------------------------------------
        // ÉTAT 2 : CAPTURE BLOQUANTE DES CONSTANTES VITALES
        // --------------------------------------------------------------------
        case STATE_READ_SENSORS: {
            float temp = 0.0;
            float bpm = 0.0;
            float spo2 = 0.0;

            bool captureSuccess;

            #if MANUAL_INPUT_MODE
                // Mode test : saisie manuelle via le Moniteur Serie (pas de capteurs physiques)
                captureSuccess = readManualSensorInputs(bpm, spo2, temp);
            #else
                // Mode normal : capture bloquante sur la température puis le pouls.
                // Dès que la présence est validée, elle prend 100 points cardiaques.
                captureSuccess = readMedicalSensors(bpm, spo2, temp);
            #endif

            if (captureSuccess) {
                // Si l'algorithme valide la qualité du signal, on enregistre
                currentPatient.temperature = temp;
                currentPatient.bpm = bpm;
                currentPatient.spo2 = spo2;

                // On rafraîchit l'OLED pour afficher un récapitulatif des données brutes
                displayLiveMetrics(currentPatient);
                delay(2000); // Laisse 2 secondes pour lire les constantes à l'écran

                // Transition : Tout est en mémoire, on passe à l'IA
                currentState = STATE_INFERENCE;
            } else {
                // Si la mesure a échoué (doigt qui bouge), la fonction renvoie false.
                // On réinitialise l'écran d'attente des capteurs et on retente la lecture.
                displayLiveMetrics(currentPatient); 
            }
            break;
        }

        // --------------------------------------------------------------------
        // ÉTAT 3 : APPLICATION DU SCALING ET INFERENCE DE L'IA
        // --------------------------------------------------------------------
        case STATE_INFERENCE: {
            Serial.println("🧠 Lancement de l'inference TinyML locale...");
            
            // Exécute le Z-Score et invoque l'interpréteur TensorFlow Lite Micro
            float confiance = 0.0;
            int predictedClass = runLocalInference(currentPatient, confiance);
            
            if (predictedClass >= 0) {
                // On memorise la confiance dans la structure patient
                currentPatient.confiance = confiance;

                // ----------------------------------------------------------------
                // TRANSMISSION SERIE VERS LE PONT PYTHON (remontee vers la base)
                // Format : MEDBOX;uid;age;bpm;spo2;temperature;confiance;classe
                // Le prefixe "MEDBOX;" permet au pont d'ignorer les autres messages.
                // Cette ligne est imprimee "dans le vide" si aucun pont n'ecoute :
                // le boitier fonctionne exactement pareil sans lui (remontee opportuniste).
                // ----------------------------------------------------------------
                Serial.print("MEDBOX;");
                Serial.print(currentPatient.uid);            Serial.print(";");
                Serial.print(currentPatient.age, 0);         Serial.print(";");
                Serial.print(currentPatient.bpm, 1);         Serial.print(";");
                Serial.print(currentPatient.spo2, 1);        Serial.print(";");
                Serial.print(currentPatient.temperature, 2); Serial.print(";");
                Serial.print(currentPatient.confiance, 4);   Serial.print(";");
                Serial.println(predictedClass);

                // Envoi de l'index prédit à l'écran OLED
                displayDiagnosticResult(predictedClass);
                
                // Enregistrement du moment de l'affichage pour gérer le timeout
                displayStartTime = millis();
                
                // Transition : On bascule sur l'affichage fixe du diagnostic
                currentState = STATE_SHOW_RESULT;
            } else {
                // Sécurité en cas d'erreur de calcul de l'interpréteur
                Serial.println("❌ Échec de l'analyse. Retour à la case départ.");
                displaySplashWait();
                currentState = STATE_WAIT_NFC;
            }
            break;
        }

        // --------------------------------------------------------------------
        // ÉTAT 4 : AFFICHAGE DU RÉSULTAT ET FIN DE SESSION
        // --------------------------------------------------------------------
        case STATE_SHOW_RESULT: {
            // L'écran affiche le diagnostic. On attend passivement pendant 10 secondes
            // pour laisser le temps de lire le verdict de l'OMS.
            if (millis() - displayStartTime >= RESULT_TIMEOUT) {
                Serial.println("🔄 Session terminee. Reset des donnees et attente nouveau patient.");
                
                // Remise à zéro de la structure pour le prochain patient
                currentPatient = PatientData(); 
                
                // Retour à l'écran d'accueil
                displaySplashWait();
                currentState = STATE_WAIT_NFC;
            }
            break;
        }
    }
}