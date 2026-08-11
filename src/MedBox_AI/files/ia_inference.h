#ifndef IA_INFERENCE_H
#define IA_INFERENCE_H

// Inclusions requises pour l'écosystème embarqué
#include "model_medbox.h" 
#include <tflm_esp32.h>
#include <eloquent_tinyml.h>

#include "config.h"

// Variables globales de l'interpréteur
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

alignas(16) uint8_t tensorArena[TENSOR_ARENA_SIZE];

// Initialisation de l'interpréteur de bas niveau
void initTinyML() {
    // 1. Chargement du modèle depuis le tableau généré
    model = tflite::GetModel(model_medbox_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.println("❌ Erreur : Version du schema de modele incompatible !");
        while (1);
    }

    // 2. Déclaration d'un résolveur mutable pour les 2 couches du modèle de classification
    static tflite::MicroMutableOpResolver<2> resolver;
    resolver.AddFullyConnected();
    resolver.AddSoftmax();

    // 3. Instanciation de l'interpréteur
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensorArena, TENSOR_ARENA_SIZE
    );
    interpreter = &static_interpreter;

    // 4. Allocation des tenseurs (La constante de succès dans l'enum tflm_esp32 est kTfLiteOk)
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("❌ Echec de l'allocation des tenseurs !");
        while (1);
    }

    input = interpreter->input(0);
    output = interpreter->output(0);
    
    Serial.println("✅ TensorFlow Lite Micro pret. Modele charge avec succes !");
}

// ============================================================================
// AJOUT DE LA FONCTION DE SCALING MANQUANTE
// ============================================================================
float applyZScoreScaling(float rawValue, float mean, float stdDev) {
    if (stdDev == 0.0f) return 0.0f; // Évite une division par zéro fatale
    return (rawValue - mean) / stdDev;
}

// ============================================================================
// EXÉCUTION DE L'INFÉRENCE LOCALE AVEC SCALING DYNAMIQUE
// ============================================================================
// ============================================================================
// EXÉCUTION DE L'INFÉRENCE LOCALE AVEC SCALING DYNAMIQUE
// ============================================================================
int runLocalInference(const PatientData &patient, float &outConfidence) {
    // On applique le calcul avec les constantes exactes de ton config.h : MEAN_XXX et STD_XXX
    input->data.f[0] = applyZScoreScaling(patient.age, MEAN_AGE, STD_AGE);
    input->data.f[1] = applyZScoreScaling(patient.bpm, MEAN_BPM, STD_BPM);
    input->data.f[2] = applyZScoreScaling(patient.spo2, MEAN_SPO2, STD_SPO2);
    input->data.f[3] = applyZScoreScaling(patient.temperature, MEAN_TEMP, STD_TEMP);

    unsigned long startMicro = micros();
    TfLiteStatus invoke_status = interpreter->Invoke();
    unsigned long endMicro = micros();

    // Vérification du statut d'exécution
    if (invoke_status != kTfLiteOk) {
        Serial.println("❌ Erreur lors de l'execution de l'inference (Invoke) !");
        return -1;
    }

    Serial.print("⚡ Inference local executee en : ");
    Serial.print(endMicro - startMicro);
    Serial.println(" microsecondes.");

    int predictedClass = 0;
    float maxConfidence = -1.0;

    for (int i = 0; i < NUM_CLASSES; i++) {
        float confidence = output->data.f[i];
        if (confidence > maxConfidence) {
            maxConfidence = confidence;
            predictedClass = i;
        }
    }

    Serial.print("🎯 Label predit (Index) : "); Serial.print(predictedClass);
    Serial.print(" | Confiance : "); Serial.print(maxConfidence * 100.0, 2); Serial.println("%");

    // On renvoie la confiance au code appelant (via le parametre de sortie)
    outConfidence = maxConfidence;

    return predictedClass;
}

#endif // IA_INFERENCE_H