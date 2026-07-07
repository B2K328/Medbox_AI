# 🩺 MedBox AI

**MedBox AI** est un boîtier médical intelligent et autonome basé sur l’**Edge AI**, conçu pour exécuter un diagnostic embarqué en temps réel sur **ESP32**, sans dépendance au cloud.

---

## 🎯 Objectif du projet

Ce projet de fin d’année (Licence 2 Informatique) met en œuvre une chaîne TinyML complète :
- acquisition de constantes vitales via capteurs,
- normalisation locale des données,
- inférence embarquée avec **TensorFlow Lite Micro**,
- affichage instantané d’un résultat de classification OMS.

---

## 🧩 Architecture matérielle (ESP32 + capteurs)

| Composant | Rôle | Données exploitées |
|---|---|---|
| ESP32 (Xtensa dual-core, 240 MHz) | Calcul embarqué + orchestration | Exécution de la machine à états et inférence TFLM |
| Mémoire ESP32 | Environnement contraint | 4 Mo Flash, 320 Ko SRAM |
| MAX30102 | Oxymètre / cardiofréquencemètre | BPM, SpO2 |
| MLX90614 (IR) | Température sans contact | Température corporelle |
| Badge NFC | Identification patient | Âge du patient |
| Écran OLED | Restitution locale | Constantes + diagnostic |

---

## ⚙️ Flux logique logiciel (Machine à états)

1. **Initialisation** : I2C, écran, capteurs, TensorFlow Lite Micro.  
2. **Attente NFC** : détection d’un badge, puis lecture de l’**âge** du patient.  
3. **Acquisition capteurs** : lecture de **BPM**, **SpO2** (MAX30102) et **température** (MLX90614).  
4. **Normalisation Z-Score** des 4 variables (Âge, BPM, SpO2, Température) avec les constantes du scaler d’entraînement (`scaler_medbox.pkl`).  
5. **Inférence locale TFLM** : alimentation du tenseur d’entrée, exécution du modèle, extraction de l’index de pathologie (classification OMS).  
6. **Restitution** : affichage immédiat du diagnostic + constantes sur OLED, puis réinitialisation du cycle.

---

## 🧮 Normalisation mathématique (Z-Score)

La transformation appliquée à chaque variable d’entrée est :

\[
x_{\mathrm{scaled}} = \frac{x - \mu}{\sigma}
\]

où :
- \(x\) : valeur brute mesurée,
- \(\mu\) : moyenne issue du jeu d’entraînement,
- \(\sigma\) : écart-type issu du jeu d’entraînement.

---

## 📊 Performances observées (tests réels)

| Indicateur | Résultat |
|---|---|
| Accuracy du modèle (Python) | **95,63 %** |
| Temps d’inférence sur ESP32 | **126 à 136 µs** |
| Utilisation Flash | **27 %** |
| Variables globales (SRAM) | **9 %** |
| Tensor Arena allouée | **8 Ko** |

### ✅ Cas clinique simulé validé

Pour un patient avec :
- **125 BPM** (tachycardie),
- **85 % SpO2** (hypoxie sévère),
- **39,5 °C** (hyperthermie),

le modèle embarqué détecte immédiatement l’**ID Pathologie OMS : 17** en **126 µs**.

---

## 🔐 Sécurité & confidentialité des données médicales

> [!IMPORTANT]
> **Traitement local uniquement (Edge AI).**  
> Les données patient sont traitées directement sur l’ESP32, sans envoi vers un service cloud pendant l’inférence.

> [!WARNING]
> Ce prototype est un système académique de démonstration.  
> Tout usage en contexte réel de soins nécessite conformité réglementaire, validation clinique et gouvernance sécurité adaptées.

---

## 📁 Structure de projet recommandée

```text
Medbox_AI/
├── src/                 # Firmware ESP32 (machine à états, drivers, logique TinyML)
├── model/               # Modèle TFLite + artefacts de normalisation (extraction scaler)
├── docs/                # Schémas, protocole de test, documentation technique
└── README.md
```

---

## 🚀 Guide rapide : dépendances & téléversement

### 1) Préparer l’environnement
- Installer **Arduino IDE** (ou PlatformIO).
- Ajouter la prise en charge **ESP32** dans le gestionnaire de cartes.

### 2) Installer les bibliothèques nécessaires
- `TensorFlowLite_ESP32`
- `Adafruit MLX90614`
- (et bibliothèques capteurs/affichage requises par le firmware : MAX30102, NFC, OLED)

### 3) Compiler et téléverser
1. Sélectionner la carte ESP32 et le port série.
2. Compiler le projet.
3. Téléverser le firmware.
4. Ouvrir le moniteur série pour vérifier l’initialisation et les mesures.

---

## 👨‍💻 Auteur

Projet de fin d’année — **Licence 2 Informatique**  
**MedBox AI** : Boîtier médical intelligent autonome avec inférence TinyML embarquée.
