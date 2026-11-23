# Love Market pour ESP32-S3 (N16R8)

Portage complet de l'application **Love Market** (Node.js/Express/EJS) vers un
firmware autonome exécuté directement sur un **ESP32-S3 N16R8** (16 Mo flash,
8 Mo PSRAM). L'ESP32 crée son propre réseau WiFi : vos téléphones s'y connectent
et utilisent l'application, sans aucun serveur ni connexion internet.

## Fonctionnalités portées (parité complète)

- Connexion / déconnexion (2 comptes : Alice et Sylvain)
- Tableau de bord : niveaux de couple, crédits, dons de crédits, bons cadeaux
- Boutique du partenaire : achat immédiat + négociation avec contre-propositions en chaîne
- Mes services : création (catégorie + icône), modification, suppression
- Demandes personnalisées (`/request-service`) et propositions (`/offer-service`)
  avec option "Service bonus" (+30 %)
- Négociations groupées avec badge de notifications, contre-offres, retour d'expérience 😊/😐/😞
- Événements spéciaux : création admin, acceptation, complétion
- Bons cadeaux : génération (code à 4 chiffres unique), page imprimable, utilisation avec animation
- Panneau admin : crédits, seuils des niveaux de couple, événements, bons cadeaux
- **Sauvegarde / restauration** : export et import du `data.json` depuis la page admin
  (format identique à la version Node.js — vous pouvez migrer vos données existantes)

## Différences assumées par rapport à la version Node.js

| Aspect | Node.js | ESP32 |
|---|---|---|
| Hébergement | PC + Node | ESP32 autonome (hotspot WiFi) |
| Adresse | http://localhost:3000 | http://192.168.4.1 |
| Persistance | `data.json` sur disque | `data.json` dans LittleFS (flash) |
| Sessions | `express-session` | cookie signé par jeton, en RAM (périssent au redémarrage) |
| Horodatage | horloge système | **heure de compilation du firmware + uptime** (pas de NTP en mode hotspot) |
| Section "bon cadeau" du dashboard | affichée 2 fois (bug d'origine) | affichée 1 fois |

> ⚠️ Les dates affichées (historique, bons cadeaux) sont donc approximatives :
> elles repartent de l'heure du dernier téléversement du firmware. La fenêtre
> glissante de 30 jours du niveau de couple reste cohérente (horloge monotone
> pendant l'uptime).

## Installation (Arduino IDE 2.x)

### 1. Support des cartes ESP32
- Fichier → Préférences → **URL de gestionnaire de cartes supplémentaires** :
  `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- Outils → Type de carte → Gestionnaire de cartes → installer **esp32 by Espressif Systems** (version ≥ 3.0 conseillée).

### 2. Bibliothèque requise
- Croquis → Inclure une bibliothèque → Gérer les bibliothèques → installer :
  **ArduinoJson** de Benoit Blanchon (version 7.x).

Aucune autre bibliothèque : le serveur web utilise `WebServer.h` inclus dans le
support ESP32, et `LittleFS.h` est fourni. Tout le CSS/HTML est **embarqué dans
le firmware** — pas besoin d'uploader un système de fichiers.

### 3. Réglages de la carte (Outils)
| Réglage | Valeur |
|---|---|
| Type de carte | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled (conseillé pour les logs série) |
| Flash Size | **16MB** |
| Partition Scheme | **Custom** (utilise le fichier `partitions.csv` inclus dans le dossier du sketch) |
| PSRAM | **OPI PSRAM** *(le N16R8 a une PSRAM octale — indispensable)* |
| CPU Frequency | 240MHz |

> ℹ️ Pourquoi « Custom » ? Les schemes officiels 16 Mo fournissent une partition
> **FAT** (incompatible avec LittleFS) et le core ESP32 3.x cherche la partition
> FS par son **nom** `spiffs`. Le fichier `partitions.csv` fourni crée donc une
> partition nommée `spiffs` avec le sous-type `littlefs` (9,9 Mo).

### 4. Téléversement
1. Ouvrir `lovemarket_esp32.ino` dans l'IDE Arduino (le dossier contient aussi
   `model.h/.cpp`, `ui.h/.cpp`, `pages.h`, `pages_main.cpp`, `pages_negot.cpp`,
   `pages_admin.cpp` — l'IDE les compile automatiquement).
2. Brancher l'ESP32-S3 en USB, sélectionner le port, cliquer **Téléverser**.
3. Ouvrir le moniteur série à 115200 bauds : l'adresse IP du hotspot s'affiche.

## Utilisation

1. Sur un téléphone : WiFi → se connecter au réseau **`LoveMarket`**
   (mot de passe par défaut : **`lovecredits`** — modifiable en haut du fichier
   `lovemarket_esp32.ino`, variables `AP_SSID` / `AP_PASS`).
2. Ouvrir **http://192.168.4.1** dans le navigateur.
3. Se connecter : identifiant `Alice` (mot de passe `171088`) ou `Sylvain`
   (mot de passe `31051989`) — identiques à la version Node.

## Migration de vos données existantes

- Sur la version Node : ouvrir `lovemarket/data.json`, copier tout le contenu.
- Sur l'ESP32 : page **Admin → Sauvegarde des données → Importer**, coller le
  JSON et valider. Le format est identique (mêmes clés).
- Bouton **Exporter** pour récupérer les données de l'ESP32 en JSON.

> L'import passe par un formulaire HTTP limité à ~16 Ko : si votre `data.json`
> dépasse, allégez l'historique des offres avant l'import.

## Structure du projet

```
lovemarket_esp32/
├── lovemarket_esp32.ino   Configuration WiFi AP, serveur, toutes les routes
├── partitions.csv         Table de partitions 16 Mo (app 3+3 Mo, LittleFS 9,9 Mo)
├── model.h / model.cpp    Modèle de données, persistance LittleFS, sessions, temps
├── ui.h / ui.cpp          Layout HTML, nav, échappement, style.css + favicon embarqués
├── pages.h                Déclarations des pages
├── pages_main.cpp         Login, dashboard, boutique, mes services, édition
├── pages_negot.cpp        Négociations, demandes, propositions, historique de couple
├── pages_admin.cpp        Admin, événements spéciaux, bon cadeau imprimable
└── README.md              Ce fichier
```

## Dépannage

- **Croquis trop volumineux** : vérifier le Partition Scheme (Custom + partitions.csv).
- **PSRAM non détectée** : régler PSRAM = "OPI PSRAM" (c'est la config N16R8).
- **Port série qui disparaît/change de numéro (COM7 → COM8...)** : c'est le comportement
  de l'USB-CDC après un reset de la carte — resélectionner le port dans l'IDE.
- **Page inaccessible** : vérifier que le téléphone est bien sur le WiFi `LoveMarket`
  et que le moniteur série affiche l'IP 192.168.4.1.
- **Données perdues après re-flash** : LittleFS est préservé par un simple
  téléversement, mais un effacement complet de la flash les supprime. Exporter
  régulièrement via la page Admin.
