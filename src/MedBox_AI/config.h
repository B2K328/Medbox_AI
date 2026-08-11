#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// 1. DÉFINITION DES ÉTATS DE LA MACHINE (STATE MACHINE)
// ============================================================================
enum MedBoxState {
    STATE_INIT,          // Initialisation des périphériques (OLED, NFC, Capteurs) et de TF Lite
    STATE_WAIT_NFC,      // Attente active du scan du badge patient (extraction de l'âge)
    STATE_READ_SENSORS,  // Capture et stabilisation des données (MAX30102 et MLX90614)
    STATE_SCALING,       // Normalisation mathématique des données par Z-Score
    STATE_INFERENCE, 
    STATE_SHOW_RESULT    // Calcul TinyML local et affichage du diagnostic précis
};

// ============================================================================
// 2. STRUCTURE DE DONNÉES DU PATIENT
// ============================================================================
struct PatientData {
    // Identité (UID du badge NFC, en hexadecimal) - sert d'identifiant patient
    String uid = "";

    // Données brutes (Raw Data collectées)
    float age = 0.0;
    float bpm = 0.0;
    float spo2 = 0.0;
    float temperature = 0.0;

    // Confiance du modele sur la classe predite (0.0 a 1.0)
    float confiance = 0.0;

    // Données normalisées prêtes pour le modèle TinyML (Inputs)
    float age_scaled = 0.0;
    float bpm_scaled = 0.0;
    float spo2_scaled = 0.0;
    float temperature_scaled = 0.0;
};

// ============================================================================
// 3. CONSTANTES DE NORMALISATION Z-SCORE (scaler_medbox.pkl)
// ============================================================================
// Formule mathématique appliquée : (Valeur - Moyenne) / Écart-type
// L'ordre des features respecte strictement le dataset : [Age, BPM, SpO2, Temperature]

const float MEAN_AGE = 40.76118421;
const float STD_AGE  = 24.57877655;

const float MEAN_BPM = 100.52855263;
const float STD_BPM  = 35.53269770;

const float MEAN_SPO2 = 93.34250000;
const float STD_SPO2  = 6.04989639;

const float MEAN_TEMP = 37.05082895;
const float STD_TEMP  = 1.83151845;

// ============================================================================
// 4. PARAMÈTRES TENSORFLOW LITE MICRO
// ============================================================================
const int TENSOR_ARENA_SIZE = 8 * 1024; // 8 Ko alloués en SRAM pour les calculs de l'IA
const int NUM_CLASSES = 19;             // Les 19 pathologies/états du dataset (0 à 18 en C++)

// ============================================================================
// 5. LISTE DES PATHOLOGIES ET ÉTATS NORMAUX (INDEXÉS DE 0 À 18)
// ============================================================================
// Tableau de correspondance pour traduire l'index de sortie de l'IA en texte clair.
// Correspond exactement aux 19 classes (1 à 19) de ton fichier Excel.
// Les accents ont été retirés pour éviter les bugs d'affichage sur l'écran OLED.

const char* const PATHOLOGY_NAMES[NUM_CLASSES] = {
    "Adulte Normal",                  // Index 0 -> Classe 1
    "Enfant Normal",                  // Index 1 -> Classe 2
    "Senior Normal",                  // Index 2 -> Classe 3
    "Var. Physiologique Legere",      // Index 3 -> Classe 4
    "Febrilicule (Alerte)",           // Index 4 -> Classe 5
    "Tachycardie Legere (Stress)",    // Index 5 -> Classe 6
    "Bradycardie Legere",             // Index 6 -> Classe 7
    "Hypoxie Legere",                 // Index 7 -> Classe 8
    "Dysthemie Thermique (Froid)",    // Index 8 -> Classe 9
    "Tachycardie Infantile",          // Index 9 -> Classe 10
    "Fievre Elevee (Infection)",      // Index 10 -> Classe 11
    "Hypoxie Severe (Detresse)",      // Index 11 -> Classe 12
    "Tachycardie Severe",             // Index 12 -> Classe 13
    "Bradycardie Severe (Syncope)",   // Index 13 -> Classe 14
    "Choc Septique",                  // Index 14 -> Classe 15
    "Hypothermie Severe",             // Index 15 -> Classe 16
    "Detresse Respi. Aigue",          // Index 16 -> Classe 17
    "Infection Pediatrique Grave",    // Index 17 -> Classe 18
    "Choc Critique (Instable)"        // Index 18 -> Classe 19
};

// ============================================================================
// 6. CALIBRATION THERMIQUE (MLX90614 : temperature de peau -> temperature corporelle)
// ============================================================================
// Offset ajoute a la lecture brute du MLX90614 pour approximer la temperature centrale.
// Calibre a partir de 8 mesures comparatives (MLX90614 vs thermometre de reference).
// Offset moyen mesure : 3.98 °C (ecart-type des mesures : 0.83 °C -> marge d'erreur
// realiste a attendre autour de +/- 0.8 °C, en raison de variations de distance/angle
// entre les prises de mesure).
// !! A RE-CALIBRER avec davantage de sujets/mesures pour ameliorer la precision. !!
const float TEMP_OFFSET_CALIBRATION = 3.98f;

// ============================================================================
// 7. MODE SIMULATION MANUELLE (TEST DE L'IA SANS CAPTEURS PHYSIQUES)
// ============================================================================
// Passe a "true" pour saisir Age/BPM/SpO2/Temperature manuellement via le Moniteur
// Serie au lieu de lire les capteurs physiques. Permet de tester toutes les classes
// de l'IA (fievre, hypoxie, choc...) sans avoir a reproduire physiquement chaque cas.
#define MANUAL_INPUT_MODE false

#endif // CONFIG_H