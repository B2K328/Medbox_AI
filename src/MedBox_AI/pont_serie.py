#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ============================================================================
# pont_serie.py — Pont série entre l'ESP32 (MedBox AI) et la base MySQL
#
# Écoute le port série, intercepte les lignes commençant par "MEDBOX;",
# applique la logique d'enrôlement (badge connu / inconnu) et insère la mesure.
# Les autres lignes (logs du boîtier) sont ignorées.
#
# Format attendu :  MEDBOX;uid;age;bpm;spo2;temperature;confiance;classe
#
# Lancement :  python3 pont_serie.py
# Arrêt     :  Ctrl+C
# ============================================================================

import sys
import time
import serial          # fourni par python3-serial
import pymysql         # fourni par python3-pymysql

# ----------------------------------------------------------------------------
# 1. CONFIGURATION — à ajuster selon ton installation
# ----------------------------------------------------------------------------
SERIAL_PORT = "/dev/ttyUSB0"   # Port de l'ESP32 (voir : ls /dev/ttyUSB* /dev/ttyACM*)
BAUD_RATE   = 115200           # Doit correspondre au Serial.begin() du boîtier

DB_CONFIG = {
    "host":        "localhost",
    "user":        "root",
    "password":    "",
    "database":    "medbox",
    "unix_socket": "/opt/lampp/var/mysql/mysql.sock",  # socket MySQL de XAMPP
    "charset":     "utf8mb4",
}

PREFIXE = "MEDBOX;"            # Seules les lignes commençant par ceci sont traitées
NB_CHAMPS = 8                 # MEDBOX + 7 valeurs -> 8 morceaux après découpage

# ----------------------------------------------------------------------------
# 2. CONNEXION À LA BASE
# ----------------------------------------------------------------------------
def connecter_base():
    """Ouvre une connexion MySQL. Réessaie jusqu'à réussite."""
    while True:
        try:
            conn = pymysql.connect(**DB_CONFIG, autocommit=False)
            print("[Base] Connexion MySQL établie.")
            return conn
        except pymysql.MySQLError as e:
            print(f"[Base] Connexion impossible ({e}). Nouvel essai dans 3 s...")
            time.sleep(3)

# ----------------------------------------------------------------------------
# 3. TRAITEMENT D'UNE LIGNE MEDBOX
# ----------------------------------------------------------------------------
def traiter_mesure(conn, ligne):
    """
    Découpe une ligne MEDBOX, applique la logique d'enrôlement et insère la mesure.
    Renvoie un message de compte-rendu (str) à afficher dans le terminal.
    """
    # Découpage : MEDBOX;uid;age;bpm;spo2;temp;confiance;classe
    morceaux = ligne.split(";")

    if len(morceaux) != NB_CHAMPS:
        return f"IGNORÉE (format inattendu, {len(morceaux)} champs) : {ligne}"

    # morceaux[0] == "MEDBOX", on extrait le reste
    try:
        uid         = morceaux[1].strip()
        age         = int(float(morceaux[2]))
        bpm         = float(morceaux[3])
        spo2        = float(morceaux[4])
        temperature = float(morceaux[5])
        confiance   = float(morceaux[6])
        classe      = int(morceaux[7])
    except ValueError as e:
        return f"IGNORÉE (valeur illisible : {e}) : {ligne}"

    # Contrôles minimaux de cohérence
    if uid == "":
        return f"IGNORÉE (uid vide) : {ligne}"
    if classe < 0 or classe > 18:
        return f"IGNORÉE (classe hors bornes : {classe}) : {ligne}"

    try:
        with conn.cursor() as cur:
            # --- Le badge existe-t-il déjà ? ---
            cur.execute(
                "SELECT id_patient FROM patient WHERE uid_nfc = %s", (uid,)
            )
            row = cur.fetchone()

            if row:
                # Badge CONNU : on réutilise le patient existant
                id_patient = row[0]
                etat = "connu"
            else:
                # Badge INCONNU : création d'un patient en attente de finalisation
                cur.execute(
                    "INSERT INTO patient (uid_nfc, nom, age, statut) "
                    "VALUES (%s, NULL, %s, 'EN_ATTENTE')",
                    (uid, age)
                )
                id_patient = cur.lastrowid
                etat = "nouveau"

            # --- Insertion de la mesure (confiance stockée, temp_brute/cycles laissés NULL) ---
            cur.execute(
                "INSERT INTO mesure "
                "(bpm, spo2, temperature, confiance, id_patient, id_pathologie) "
                "VALUES (%s, %s, %s, %s, %s, %s)",
                (bpm, spo2, temperature, confiance, id_patient, classe)
            )

        conn.commit()
        return (f"ENREGISTRÉE  patient {etat} (id={id_patient}, uid={uid}) "
                f"| BPM={bpm:.0f} SpO2={spo2:.0f}% Temp={temperature:.1f}°C "
                f"| classe={classe} conf={confiance*100:.1f}%")

    except pymysql.MySQLError as e:
        conn.rollback()
        return f"ERREUR base (mesure non enregistrée) : {e}"

# ----------------------------------------------------------------------------
# 4. BOUCLE PRINCIPALE
# ----------------------------------------------------------------------------
def main():
    print("=" * 60)
    print("  Pont série MedBox AI  —  ESP32  ->  MySQL")
    print("=" * 60)

    # Connexion à la base d'abord
    conn = connecter_base()

    # Ouverture du port série
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except serial.SerialException as e:
        print(f"[Série] Impossible d'ouvrir {SERIAL_PORT} : {e}")
        print("        Vérifie le port (ls /dev/ttyUSB* /dev/ttyACM*) et les "
              "permissions (groupe dialout).")
        conn.close()
        sys.exit(1)

    print(f"[Série] En écoute sur {SERIAL_PORT} à {BAUD_RATE} bauds.")
    print("        (Ctrl+C pour arrêter)\n")

    try:
        while True:
            # Lecture d'une ligne (jusqu'au retour à la ligne)
            brute = ser.readline()
            if not brute:
                continue  # timeout : rien reçu, on reboucle

            # Décodage tolérant (on ignore les octets non-UTF8 éventuels)
            ligne = brute.decode("utf-8", errors="ignore").strip()
            if not ligne:
                continue

            # Seules les lignes commençant par le préfixe nous intéressent
            if ligne.startswith(PREFIXE):
                horodatage = time.strftime("%H:%M:%S")
                compte_rendu = traiter_mesure(conn, ligne)
                print(f"[{horodatage}] {compte_rendu}")
            # else: ligne de log ordinaire du boîtier -> ignorée silencieusement

    except KeyboardInterrupt:
        print("\n[Arrêt] Interruption demandée. Fermeture propre...")
    finally:
        ser.close()
        conn.close()
        print("[Arrêt] Port série et base fermés. À bientôt.")

if __name__ == "__main__":
    main()
