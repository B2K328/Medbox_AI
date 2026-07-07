# 🩺 MedBox AI  
### *Un boîtier médical intelligent et autonome utilisant l’Edge AI*  

[![Plateforme](https://img.shields.io/badge/Plateforme-ESP32-blue)](#)  
[![IA embarquée](https://img.shields.io/badge/IA-TensorFlow%20Lite%20Micro-orange)](#)  
[![Licence académique](https://img.shields.io/badge/Projet-L2%20Informatique-success)](#)

---

## 📌 Présentation

**MedBox AI** est un projet de fin d’année (Licence 2 Informatique) visant à concevoir un **boîtier médical autonome** capable de réaliser un **diagnostic local en temps réel** à partir de constantes physiologiques.  

Le système embarque un modèle de classification exécuté directement sur un **ESP32** via **TensorFlow Lite Micro (TFLM)**, sans dépendance au cloud.

---

## 🎯 Objectifs du projet

- Concevoir une chaîne complète de **mesure → traitement → inférence → affichage** sur microcontrôleur.
- Exploiter l’**Edge AI** pour un diagnostic ultra-rapide et local.
- Garantir une architecture compatible avec les contraintes mémoire/CPU d’un ESP32.
- Démontrer la faisabilité d’un système médical intelligent à faible coût matériel.

---

## 🧠 Architecture embarquée (ESP32)

L’environnement d’exécution est fortement contraint :

| Élément | Spécification |
|---|---|
| Microcontrôleur | ESP32 |
| CPU | Xtensa double-cœur @ 240 MHz |
| Flash | 4 Mo |
| SRAM | 320 Ko |
| Cadre IA | TensorFlow Lite Micro (TFLM) |
| Allocation Tensor Arena | **8 Ko** (SRAM) |

---

## 🧩 Composants matériels

| Composant | Rôle |
|---|---|
| ESP32 | Unité centrale de contrôle et d’inférence |
| MAX30102 | Mesure du rythme cardiaque (BPM) et de la SpO2 |
| MLX90614 (IR) | Mesure de la température corporelle sans contact |
| Lecteur NFC | Lecture de l’âge patient depuis badge NFC |
| Écran OLED | Affichage des constantes et du diagnostic |

---

## 🔄 Flux logique du système (Machine à États)

Le firmware suit une séquence déterministe en **6 étapes** :

1. **Initialisation**
   - Bus I2C
   - Écran OLED
   - Capteurs
   - Moteur TensorFlow Lite Micro

2. **Attente NFC**
   - Scrutation active d’un badge
   - Lecture de l’**âge patient**

3. **Acquisition biométrique**
   - **MAX30102** : BPM + SpO2  
   - **MLX90614** : Température corporelle

4. **Prétraitement / Normalisation**
   - Transformation Z-Score des 4 variables :  
     **Âge, BPM, SpO2, Température**
   - Paramètres issus du scaler d’entraînement : `scaler_medbox.pkl`

5. **Inférence TinyML locale**
   - Injection du vecteur normalisé dans le tenseur d’entrée
   - Exécution du modèle TFLM sur ESP32
   - Récupération de l’index de pathologie (classification OMS)

6. **Restitution**
   - Affichage instantané des constantes + diagnostic sur OLED
   - Réinitialisation du cycle

---

## 🧮 Normalisation mathématique (Z-Score)

Avant inférence, chaque variable \(x\) est normalisée par :

\[
x_{\text{scaled}} = \frac{x - \mu}{\sigma}
\]

où :
- \(\mu\) = moyenne issue du dataset d’entraînement  
- \(\sigma\) = écart-type issu du dataset d’entraînement  

Cette étape garantit la cohérence entre les données capteurs en production et le domaine statistique appris par le modèle.

---

## ⚡ Performances mesurées (tests réels)

| Indicateur | Résultat |
|---|---|
| Accuracy du modèle (Python) | **95,63 %** |
| Temps d’inférence ESP32 | **126 à 136 µs** |
| Utilisation Flash | **27 %** |
| Variables globales | **9 %** |
| Tensor Arena SRAM | **8 Ko** |

### 🧪 Cas clinique simulé validé
Pour un patient simulé en **tachycardie + hypoxie sévère** :  
- 125 BPM  
- 85 % SpO2  
- 39.5 °C  

➡️ Le modèle embarqué détecte immédiatement **l’ID Pathologie OMS : 17** en **126 µs**.

---

## 🔒 Sécurité & confidentialité des données médicales

> [!IMPORTANT]  
> **MedBox AI fonctionne en Edge AI local** : l’inférence est réalisée directement sur l’ESP32, sans cloud.

> [!NOTE]  
> Cette architecture réduit l’exposition des données sensibles (constantes vitales), limite les dépendances réseau et améliore la résilience du système.

> [!WARNING]  
> Ce prototype est un **dispositif académique** et ne remplace pas un diagnostic médical professionnel.  
> Toute utilisation clinique réelle nécessite validation réglementaire, protocoles médicaux et conformité légale.

---

## 📁 Structure de projet recommandée

```text
Medbox_AI/
├─ src/                 # Code embarqué ESP32 (machine à états, capteurs, inférence)
│  ├─ main.ino
│  ├─ sensors/
│  ├─ nfc/
│  ├─ display/
│  └─ inference/
├─ model/               # Artefacts IA (modèle TFLite, paramètres de scaling)
│  ├─ medbox_model.tflite
│  ├─ model_data.h
│  └─ scaler_medbox.pkl
├─ docs/                # Documentation technique, schémas, rapport
│  ├─ architecture.md
│  ├─ bench_results.md
│  └─ images/
├─ data/                # Jeux de données / exports de test (si applicable)
├─ scripts/             # Scripts d’entraînement / conversion (Python)
└─ README.md
```

---

## 🛠️ Dépendances logicielles

À installer dans l’IDE Arduino (ou PlatformIO selon votre workflow) :

- **TensorFlowLite_ESP32** (TensorFlow Lite Micro pour ESP32)
- **Adafruit MLX90614 Library**
- **Librairie MAX30102** compatible ESP32
- **Librairie NFC** correspondant au module utilisé
- **Librairie écran OLED** (ex. SSD1306 selon le matériel)

---

## 🚀 Guide de démarrage rapide

### 1) Préparer l’environnement
1. Installer **Arduino IDE** (ou PlatformIO).
2. Ajouter la carte **ESP32** via le Boards Manager.
3. Installer les bibliothèques listées ci-dessus.

### 2) Préparer le projet
1. Cloner le dépôt :
   ```bash
   git clone https://github.com/B2K328/Medbox_AI.git
   cd Medbox_AI
   ```
2. Vérifier la présence :
   - du modèle embarqué (`.tflite` ou `model_data.h`)
   - des constantes de normalisation (issues de `scaler_medbox.pkl`)

### 3) Compiler & téléverser
1. Ouvrir `src/main.ino`.
2. Sélectionner la carte ESP32 et le port série.
3. Compiler puis téléverser.
4. Ouvrir le moniteur série pour observer :
   - l’acquisition capteur
   - les valeurs normalisées
   - le résultat d’inférence

---

## 🧪 Validation fonctionnelle

Checklist de validation recommandée :

- [ ] Initialisation I2C / capteurs / écran réussie  
- [ ] Lecture NFC de l’âge valide  
- [ ] Acquisition BPM / SpO2 / Température cohérente  
- [ ] Normalisation Z-Score conforme aux constantes d’entraînement  
- [ ] Inférence < 200 µs stable  
- [ ] Affichage OLED correct (constantes + ID OMS)

---

## 📚 Périmètre académique

Ce projet illustre une démarche d’ingénierie embarquée complète :

- conception d’une **pipeline TinyML** de bout en bout,
- adaptation aux contraintes d’un **microcontrôleur basse ressource**,
- intégration **capteurs + IA + IHM embarquée**.

---

## 👨‍💻 Auteur

**B2K328**  
Projet de fin d’année — **Licence 2 Informatique**  
Repository : [github.com/B2K328/Medbox_AI](https://github.com/B2K328/Medbox_AI)

---

## 📄 Licence

À préciser selon votre choix (`MIT`, `Apache-2.0`, ou licence académique spécifique).
