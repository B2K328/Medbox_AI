#ifndef SENSORS_MODULE_H
#define SENSORS_MODULE_H

#include <Wire.h>
#include <MAX30105.h>           // Bibliothèque SparkFun pour le MAX30102
#include "spo2_algorithm.h"     // Algorithme d'extraction officiel de Maxim Integrated
#include <Adafruit_MLX90614.h>   // Bibliothèque Adafruit pour le MLX90614
#include <Adafruit_SSD1306.h>   // Pour mettre à jour l'OLED pendant les étapes
#include "config.h"

// Si BUZZER_PIN n'est pas encore mis dans config.h, on le définit ici par sécurité
#ifndef BUZZER_PIN
#define BUZZER_PIN 13
#endif

// Accès à l'instance globale de l'écran OLED définie dans display_module.h
extern Adafruit_SSD1306 display;

// Création des instances pour chaque capteur
MAX30105 maxSensor;
Adafruit_MLX90614 mlxSensor = Adafruit_MLX90614();

// Tampons de stockage requis pour l'algorithme.
// !! BUFFER_SIZE DOIT rester à 100 !! La fonction maxim_heart_rate_and_oxygen_saturation()
// utilise des tableaux internes de taille FIXE (100), codés en dur dans spo2_algorithm.h.
// Envoyer plus de 100 échantillons fait écrire la fonction hors de ces tableaux internes,
// ce qui corrompt la mémoire et provoque un crash (Guru Meditation / LoadProhibited).
// Pour compenser une fenêtre courte, on multiplie les CYCLES de 100 échantillons (voir
// NUM_MEASURE_CYCLES plus bas) plutôt que d'agrandir un seul cycle.
#define BUFFER_SIZE 100
uint32_t irBuffer[BUFFER_SIZE];  
uint32_t redBuffer[BUFFER_SIZE]; 

// Variables de sortie pour l'algorithme de Maxim
int8_t spo2Valid;   
int8_t bpmValid;    
int32_t n_spo2;      
int32_t n_heart_rate;

// Bornes physiologiques plausibles pour filtrer les mesures aberrantes
// (l'algorithme Maxim peut marquer "valide" un résultat malgré un signal bruité)
const float BPM_MIN_PLAUSIBLE  = 40.0;
const float BPM_MAX_PLAUSIBLE  = 200.0;
const float SPO2_MIN_PLAUSIBLE = 70.0;
const float SPO2_MAX_PLAUSIBLE = 100.0;

// Nombre de cycles de mesure successifs pour dégager une valeur médiane robuste
// (compense la contrainte des 100 échantillons/cycle imposée par la librairie Maxim)
const int NUM_MEASURE_CYCLES = 5;

// ============================================================================
// FONCTION UTILITAIRE POUR FAIRE BIP
// ============================================================================
void beep(int durationMs) {
    digitalWrite(BUZZER_PIN, HIGH); // Allume le buzzer
    delay(durationMs);              // Attend la durée demandée
    digitalWrite(BUZZER_PIN, LOW);  // Éteint le buzzer
}

// ============================================================================
// INITIALISATION DES CAPTEURS ET DU BUZZER
// ============================================================================
void initSensors() {
    // Configuration de la broche du buzzer en sortie
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW); // Assure que le buzzer est éteint au départ

    if (!mlxSensor.begin()) {
        Serial.println("❌ MLX90614 non trouve !");
        while (1);
    }

    if (!maxSensor.begin(Wire, I2C_SPEED_FAST)) { 
        Serial.println("❌ MAX30102 non trouve !");
        while (1);
    }

    // Configuration standard du MAX30102
    byte ledBrightness = 60;  // Remis à 60 : la valeur 100 saturait le capteur (DC trop
                               // fort), ce qui écrasait le signal pulsatile (AC) et
                               // provoquait un échec total de détection (-999).
    byte sampleAverage = 4;   // Lisse le signal brut au niveau matériel
    byte ledMode = 2;         
    int sampleRate = 100;     
    int pulseWidth = 411;     
    int adcRange = 4096;      

    maxSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    Serial.println("✅ Capteurs et Buzzer initialises !");
}

// ============================================================================
// LECTURE DES DONNÉES AVEC RETOUR SONORE ET TRANSITION
// ============================================================================
bool readMedicalSensors(float &calculatedBPM, float &calculatedSpO2, float &calculatedTemp) {
    
    // ---- Etape 1 : Attente active d'une présence humaine (MLX90614) ----
    Serial.println("⏳ En attente d'un patient devant le capteur de temperature...");

    // Instruction affichée à l'écran pendant l'attente de détection de température
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 8);
    display.println("[ MESURE TEMP. ]");
    display.drawFastHLine(0, 18, 128, SSD1306_WHITE);
    display.setCursor(0, 28);
    display.println("Approchez le dos");
    display.setCursor(0, 40);
    display.println("de la main du");
    display.setCursor(0, 52);
    display.println("capteur...");
    display.display();
    
    float tempCheck = mlxSensor.readObjectTempC();
    
    // CORRECTION : On met à jour la même variable "tempCheck" sans redéclarer "float"
    while (tempCheck < 32.0) {
        delay(200);
        tempCheck = mlxSensor.readObjectTempC();
    }
    
    // La détection de présence se fait sur la valeur BRUTE (32°C = peau détectée).
    // La valeur STOCKÉE et envoyée à l'IA est corrigée via l'offset de calibration,
    // sinon l'IA reçoit toujours ~32°C et classe systématiquement en hypothermie sévère.
    calculatedTemp = tempCheck + TEMP_OFFSET_CALIBRATION;
    Serial.print("🌡️ Patient detecte ! Temperature : "); 
    Serial.print(calculatedTemp); Serial.println(" C");
    
    // 🔔 PREMIER BIP : Température acquise avec succès
    beep(150);

    // 🕒 --- ÉTAPE DE TRANSITION DE 3 SECONDES ---
    Serial.println("🕒 Pause de transition... Préparez-vous à poser le doigt.");
    
    // Mise à jour de l'OLED pour informer le patient
    display.clearDisplay();
    display.setTextSize(1);
    
    display.setCursor(0, 0);
    display.println("[ MESURE TEMP. OK ]");
    
    display.setCursor(0, 16);
    display.print("Temp : "); display.print(calculatedTemp, 1); display.println(" C");
    
    display.drawFastHLine(0, 28, 128, SSD1306_WHITE);
    
    display.setCursor(0, 36);
    display.println("Preparez votre doigt");
    display.setCursor(0, 48);
    display.println("sur le capteur...");
    display.display();

    // Attente de 3 secondes
    delay(3000); 

    // ---- Etape 2 : Attente que le doigt soit posé ----
    Serial.println("📥 Placez et maintenez le doigt. Capture des donnees en cours...");
    
    display.clearDisplay();
    display.setCursor(0, 16);
    display.println(" Posez votre doigt ");
    display.setCursor(0, 32);
    display.println("  sur le capteur  ");
    display.display();

    // Attente active que le doigt soit posé (détection par le rayon infrarouge)
    while (maxSensor.getIR() < 50000) {
        delay(100);
    }

    // ---- Etape 3 : Plusieurs cycles de mesure pour fiabiliser le résultat ----
    // Une seule fenêtre d'1 seconde est très sensible au bruit/mouvement (d'où les
    // valeurs aberrantes type 187 bpm). On capture plusieurs cycles, on écarte les
    // valeurs physiologiquement improbables, puis on garde la MÉDIANE (plus robuste
    // qu'une moyenne face à une valeur extrême isolée).
    float bpmReadings[NUM_MEASURE_CYCLES];
    float spo2Readings[NUM_MEASURE_CYCLES];
    int validCount = 0;

    for (int cycle = 0; cycle < NUM_MEASURE_CYCLES; cycle++) {

        // Une SEULE mise à jour d'écran au début du cycle (pas pendant la capture !)
        // Chaque display.display() bloque le bus I2C ~20-30ms, ce qui perturbe la
        // régularité temporelle du signal MAX30102 et fausse le calcul du BPM.
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 10);
        display.print("Mesure du pouls...");
        display.setCursor(0, 24);
        display.print("Cycle "); display.print(cycle + 1);
        display.print(" / "); display.println(NUM_MEASURE_CYCLES);
        display.setCursor(0, 44);
        display.println("Ne bougez pas le doigt");
        display.display();

        for (int i = 0; i < BUFFER_SIZE; i++) {
            while (maxSensor.available() == false) {
                maxSensor.check(); 
            }

            redBuffer[i] = maxSensor.getRed();
            irBuffer[i] = maxSensor.getIR();
            maxSensor.nextSample(); 
        }

        // Calcul mathématique du BPM et SpO2 pour ce cycle
        maxim_heart_rate_and_oxygen_saturation(
            irBuffer, BUFFER_SIZE, redBuffer, &n_spo2, &spo2Valid, &n_heart_rate, &bpmValid
        );

        float cycleBPM = (float)n_heart_rate;
        float cycleSpO2 = (float)n_spo2;

        bool physiologicallyPlausible =
            (cycleBPM >= BPM_MIN_PLAUSIBLE && cycleBPM <= BPM_MAX_PLAUSIBLE) &&
            (cycleSpO2 >= SPO2_MIN_PLAUSIBLE && cycleSpO2 <= SPO2_MAX_PLAUSIBLE);

        if (bpmValid == 1 && spo2Valid == 1 && physiologicallyPlausible) {
            bpmReadings[validCount] = cycleBPM;
            spo2Readings[validCount] = cycleSpO2;
            validCount++;

            Serial.print("   Cycle "); Serial.print(cycle + 1);
            Serial.print(" -> BPM: "); Serial.print(cycleBPM);
            Serial.print(" | SpO2: "); Serial.print(cycleSpO2); Serial.println("% (retenu)");
        } else {
            Serial.print("   Cycle "); Serial.print(cycle + 1);
            Serial.print(" -> BPM: "); Serial.print(cycleBPM);
            Serial.print(" | SpO2: "); Serial.print(cycleSpO2);
            Serial.println("% (rejeté : bruit ou hors plage physiologique)");
        }
    }

    // Écran de fin de mesure rapide pendant le calcul final
    display.clearDisplay();
    display.setCursor(0, 24);
    display.println("   Calcul en cours...  ");
    display.display();

    // ---- Etape 4 : Validation et calcul de la médiane ----
    // Il faut au moins 3 cycles valides sur 5 (majorité) pour dégager une médiane fiable
    if (validCount >= 3) {
        // Tri simple (peu de valeurs, une bulle suffit) pour extraire la médiane
        for (int a = 0; a < validCount - 1; a++) {
            for (int b = 0; b < validCount - a - 1; b++) {
                if (bpmReadings[b] > bpmReadings[b + 1]) {
                    float tmp = bpmReadings[b];
                    bpmReadings[b] = bpmReadings[b + 1];
                    bpmReadings[b + 1] = tmp;
                }
                if (spo2Readings[b] > spo2Readings[b + 1]) {
                    float tmp = spo2Readings[b];
                    spo2Readings[b] = spo2Readings[b + 1];
                    spo2Readings[b + 1] = tmp;
                }
            }
        }

        int medianIndex = validCount / 2;
        calculatedBPM = bpmReadings[medianIndex];
        calculatedSpO2 = spo2Readings[medianIndex];

        Serial.print("❤️ BPM (médiane): "); Serial.print(calculatedBPM);
        Serial.print(" | 💧 SpO2 (médiane): "); Serial.print(calculatedSpO2); Serial.println("%");
        
        // 🔔 SECOND BIP : Deux bips rapides pour célébrer le succès !
        beep(100);
        delay(80);
        beep(100);
        
        return true; 
    } 
    
    // Si moins de 2 cycles sur 3 sont valides, la mesure est jugée trop instable
    Serial.println("⚠️ Mesure instable (trop peu de cycles valides), veuillez laisser le doigt immobile. On recommence...");
    return false; 
}

// ============================================================================
// MODE SIMULATION - SAISIE MANUELLE DES CONSTANTES VIA LE MONITEUR SERIE
// ============================================================================
// Permet de tester n'importe quelle classe de l'IA (fievre, hypoxie, choc...)
// sans reproduire physiquement chaque cas avec les capteurs.
void clearSerialBuffer() {
    while (Serial.available() > 0) Serial.read();
}

bool readManualSensorInputs(float &calculatedBPM, float &calculatedSpO2, float &calculatedTemp) {
    Serial.println();
    Serial.println("🧪 [MODE SIMULATION] Saisie manuelle des constantes vitales.");

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.println("[ MODE SIMULATION ]");
    display.setCursor(0, 32);
    display.println("Saisie via le");
    display.setCursor(0, 44);
    display.println("Moniteur Serie...");
    display.display();

    Serial.print("   BPM (ex: 80) : ");
    while (Serial.available() == 0) { delay(10); }
    calculatedBPM = Serial.parseFloat();
    clearSerialBuffer();
    Serial.println(calculatedBPM);

    Serial.print("   SpO2 %% (ex: 97) : ");
    while (Serial.available() == 0) { delay(10); }
    calculatedSpO2 = Serial.parseFloat();
    clearSerialBuffer();
    Serial.println(calculatedSpO2);

    Serial.print("   Temperature C (ex: 37.0) : ");
    while (Serial.available() == 0) { delay(10); }
    calculatedTemp = Serial.parseFloat();
    clearSerialBuffer();
    Serial.println(calculatedTemp);

    Serial.println("✅ Valeurs simulees enregistrees.");
    return true;
}

#endif // SENSORS_MODULE_H