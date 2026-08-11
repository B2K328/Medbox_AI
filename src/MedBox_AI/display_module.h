#ifndef DISPLAY_MODULE_H
#define DISPLAY_MODULE_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// Définition des dimensions de l'écran OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 // Partage la broche de reset de l'ESP32

// Instance globale de l'écran (Adresse I2C par défaut souvent 0x3C)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =========================================================================
// 1. DICTIONNAIRE DES DIAGNOSTICS (Correspondance des classes de l'IA)
// =========================================================================
// Tableau de pointeurs de caractères pour convertir l'index de l'IA en texte.
// L'ordre doit correspondre EXACTEMENT à l'encodage de ton modèle en Python.
const char* const diagnosticLabels[19] = {
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

// =========================================================================
// 2. INITIALISATION DE L'ÉCRAN
// =========================================================================
bool initDisplayModule() {
    // Initialisation de la communication I2C (par défaut broches SDA/SCL de l'ESP32)
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println("Erreur : Impossible de trouver l'écran OLED (Vérifie l'adresse I2C) !");
        return false;
    }
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("  MedBox AI Ready  ");
    display.display();
    
    Serial.println("Module d'affichage OLED initialise avec succes !");
    return true;
}

// =========================================================================
// 3. AFFICHAGE DES MESURES EN TEMPS RÉEL
// =========================================================================
// Affiche les constantes pendant la phase de scan/mesure des capteurs
void displayLiveMetrics(const PatientData &patient) {
    display.clearDisplay();
    display.setTextSize(1);
    
    // En-tête rapide
    display.setCursor(0, 0);
    display.print("--- MESURES LIVE ---");
    
    // Données du patient passées par référence constante (lecture seule)
    display.setCursor(0, 16);
    display.print("Age :  "); display.print(patient.age); display.println(" ans");
    
    display.setCursor(0, 28);
    display.print("BPM :  "); display.println(patient.bpm, 1);
    
    display.setCursor(0, 40);
    display.print("SpO2 : "); display.print(patient.spo2, 1); display.println(" %");
    
    display.setCursor(0, 52);
    display.print("Temp : "); display.print(patient.temperature, 1); display.println(" C");
    
    display.display(); // Envoi effectif de la mémoire tampon à l'écran
}

// =========================================================================
// 4. AFFICHAGE DU RESULTAT DE L'IA
// =========================================================================
// Reçoit le numéro de classe retourné par runInference() et l'affiche proprement
void displayDiagnosticResult(int diagnosticIndex) {
    display.clearDisplay();
    
    // Titre encadré
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("[ RESULTAT DIAGNOSTIC ]");
    
    display.setCursor(0, 16);
    display.print("Classe IA : "); display.println(diagnosticIndex);
    
    // Séparation visuelle
    display.drawFastHLine(0, 26, 128, SSD1306_WHITE);
    
    // Affichage du texte associé
    display.setTextSize(1);
    display.setCursor(0, 34);
    
    // Sécurité au cas où l'IA renverrait un index hors des limites du tableau (erreur -1 par exemple)
    if (diagnosticIndex >= 0 && diagnosticIndex < 19) {
        display.println(diagnosticLabels[diagnosticIndex]);
    } else {
        display.println("Erreur Analyse IA");
    }
    
    display.display();
}

// =========================================================================
// 5. ÉCRAN DE VEILLE / FIN DE SESSION
// =========================================================================
void displaySplashWait() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 24);
    display.println("  Veuillez scanner  ");
    display.println("   un badge NFC...  ");
    display.display();
}

#endif // DISPLAY_MODULE_H