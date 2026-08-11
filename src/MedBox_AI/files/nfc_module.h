#ifndef NFC_MODULE_H
#define NFC_MODULE_H

#include <Wire.h>
#include <Adafruit_PN532.h>
#include "config.h"

// ============================================================================
// CONFIGURATION DE LA SIMULATION (À modifier pour tes tests)
// ============================================================================
#define SIMULATION_MODE   false  // Met "true" pour simuler sans carte, "false" pour le vrai NFC
#define SIMULATED_AGE     75.0  // L'âge que tu veux envoyer pour tes tests de l'IA

// On n'utilise pas de broches physiques pour IRQ et RESET en I2C à 4 fils
#define PN532_IRQ   (0)
#define PN532_RESET (0) 

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET, &Wire);

// ============================================================================
// INITIALISATION DU LECTEUR NFC
// ============================================================================
void initNFC() {
    #if SIMULATION_MODE
        Serial.println("⚠️ [MODE SIMULATION ACTIVE] Le PN532 physique est ignoré.");
        return; // On ne démarre même pas le PN532 physique en mode simulation
    #endif

    nfc.begin();

    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.println("❌ PN532 non trouvé ! Vérifie tes branchements ou active le mode simulation.");
        while (1); 
    }
    
    Serial.print("✅ PN532 Détecté ! Firmware v"); 
    Serial.print((versiondata>>16) & 0xFF, DEC); 
    Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
    
    nfc.SAMConfig();
    Serial.println("⏳ En attente d'un badge NFC...");
}

// ============================================================================
// VÉRIFICATION ET LECTURE DE L'ÂGE
// ============================================================================
// Convertit l'UID (suite d'octets) en chaine hexadecimale lisible (ex: "A3F2C19B")
String uidToHex(uint8_t *uid, uint8_t uidLength) {
    String hex = "";
    for (uint8_t i = 0; i < uidLength; i++) {
        if (uid[i] < 0x10) hex += "0";      // zero de tete pour garder 2 caracteres
        hex += String(uid[i], HEX);
    }
    hex.toUpperCase();
    return hex;
}

bool checkForNFCBadge(float &extractedAge, String &extractedUID) {
    // --- BYPASS DE SIMULATION ---
    if (SIMULATION_MODE) {
        extractedAge = SIMULATED_AGE;
        extractedUID = "SIMULATION";  // UID fictif en mode simulation
        Serial.print("💻 [SIMULATION] Simulation de carte NFC. Âge envoyé : ");
        Serial.print(extractedAge);
        Serial.println(" ans.");
        
        delay(1000); // Petit délai pour simuler le temps de passage de la carte
        return true; // Renvoie 'true' immédiatement pour débloquer la suite de ton code !
    }

    // --- CODE RÉEL (S'exécute uniquement si SIMULATION_MODE est sur false) ---
    uint8_t success;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  
    uint8_t uidLength;                        
    
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);
    
    if (success) {
        Serial.println("\n🎯 Badge NFC détecté à proximité !");

        // On enregistre l'UID de la carte (identifiant unique physique) en hexadecimal.
        // C'est cet UID qui servira d'identifiant patient cote base de donnees.
        extractedUID = uidToHex(uid, uidLength);
        Serial.print("🆔 UID du badge : "); Serial.println(extractedUID);

        uint8_t data[16];
        bool readSuccess = false;
        String contenuTexte = "";

        if (uidLength == 7) {
            Serial.println("📱 Type de puce : NTAG / Ultralight");
            if (nfc.ntag2xx_ReadPage(4, data)) {
                readSuccess = true;
                for (int i = 0; i < 4; i++) {
                    if (isDigit((char)data[i])) {
                        contenuTexte += (char)data[i];
                    }
                }
                if (contenuTexte.length() == 0) {
                    uint8_t dataPage5[4];
                    if (nfc.ntag2xx_ReadPage(5, dataPage5)) {
                        for (int i = 0; i < 4; i++) {
                            if (isDigit((char)dataPage5[i])) {
                                contenuTexte += (char)dataPage5[i];
                            }
                        }
                    }
                }
            }
        } 
        else if (uidLength == 4) {
            Serial.println("💳 Type de puce : Mifare Classic 1K");

            // Authentification OBLIGATOIRE avant toute lecture d'un bloc Mifare Classic
            // (clé A par défaut, identique à celle utilisée lors de l'écriture)
            uint8_t keyA[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
            if (!nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keyA)) {
                Serial.println("⚠️ Echec authentification bloc 4 (lecture impossible).");
            } else if (nfc.mifareclassic_ReadDataBlock(4, data)) {
                readSuccess = true;
                if (data[0] > 0 && data[0] <= 120) {
                    extractedAge = (float)data[0];
                } else {
                    for (int i = 0; i < 4; i++) {
                        if (isDigit((char)data[i])) {
                            contenuTexte += (char)data[i];
                        }
                    }
                }
            }
        }

        if (contenuTexte.length() > 0) {
            extractedAge = (float)contenuTexte.toInt();
        }

        if (!readSuccess || extractedAge <= 0 || extractedAge > 120) {
            Serial.println("⚠️ Impossible de décoder l'âge. Valeur de secours appliquée (21 ans).");
            extractedAge = 21.0; 
        }

        Serial.print("📋 Lecture réussie. Âge enregistré : ");
        Serial.print(extractedAge);
        Serial.println(" ans.");
        
        return true; 
    }
    
    return false; 
}

#endif // NFC_MODULE_H