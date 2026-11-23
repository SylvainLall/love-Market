# Love Market V1 pour ESP32-S3 (N16R8)

Application web « envies » pour un couple, hébergée sur un **ESP32-S3 N16R8**
(16 Mo flash, 8 Mo PSRAM). L'ESP32 crée son propre réseau WiFi : vos téléphones
s'y connectent et utilisent l'application, sans aucun serveur ni internet.

**Philosophie : « Tes envies. Mes envies. Nos plaisirs. »**

Love Market n'est pas une boutique de prestations ni un système de récompenses :
c'est un outil de communication et de complicité centré sur les envies et le
plaisir de faire plaisir.

## Le concept V1

1. **Chacun publie ses envies** — titre, description facultative, catégorie,
   valeur en crédits (0 à 4 : la récompense que gagnera le réalisateur),
   date ou période souhaitée facultative.
2. **Le partenaire découvre** les envies publiées et peut les accepter
   librement — aucune obligation, aucune contre-partie imposée.
3. **Le réalisateur signale** sa réalisation (« J'ai réalisé cette envie ! »).
4. **Seul le bénéficiaire confirme** (« Oui, c'est fait ! ») : les crédits sont
   alors attribués **une seule fois**, au réalisateur, et journalisés.
5. Les envies réalisées rejoignent **Nos moments**, l'album du couple.

Règles de crédits (symboliques, jamais une somme d'argent) :
- Chacun commence avec 120 crédits.
- Aucun crédit n'est dépensé pour publier ou accepter une envie.
- Les crédits ne sont attribués qu'à la confirmation, une seule fois.
- Une envie à 0 crédit est aussi précieuse qu'une autre (et ne génère rien).
- Jamais négatif, jamais de transfert direct, pas de pénalité, pas de dette.

Progression du couple : le compteur augmente d'un point à chaque envie
**confirmée comme réalisée** (les deux sens confondus). Niveaux par défaut :
Coloc 0 · Complice 5 · Équipe 12 · Connecté 20 · In Love 35 — seuils
configurables dans l'administration.

## Ce qui a disparu par rapport à la V0 (boutique)

Boutique, achat immédiat, négociations et contre-propositions, paiements entre
partenaires, bons cadeaux, dons de crédits, Super Services, événements
spéciaux : tout a été retiré. Les données historiques de l'ancienne version
sont archivées (voir Migration ci-dessous) et restent exportables.

## Fiabilité des données (priorité absolue)

- **Sauvegarde atomique** : chaque écriture passe par `/data.tmp`, est relue
  pour vérification, puis remplace `/data.json` — la version précédente est
  conservée dans `/data.bak`.
- **Restauration au démarrage** : si `/data.json` est absent ou corrompu
  (coupure électrique pendant une sauvegarde), il est restauré depuis
  `/data.bak`.
- **Journal des crédits** (`creditTransactions`) : toute attribution ou
  ajustement administratif est inscrit avec auteur, montant, motif et date.
- **Mots de passe hachés** : SHA-256 + sel aléatoire (jamais en clair dans le
  data.json). Les mots de passe fixes (Alice = `Poupoune`,
  Sylvain = `31051989`) sont réappliqués au démarrage.
- **Gardes serveur** : acceptation uniquement si l'envie est disponible,
  confirmation uniquement par le bénéficiaire, suppression uniquement avant
  acceptation — les doubles soumissions sont sans effet.

## Migration depuis l'ancienne version (V0)

Au premier démarrage, si l'ESP32 contient un ancien `data.json` (format
boutique avec `services`/`offers`) :

1. Sauvegarde brute intégrale : `/data_v0_backup.json`.
2. Archivage des données commerciales (services, offres, bons cadeaux,
   événements) : `/legacy.json` — exportable depuis **Admin → Données →
   Exporter l'archive V0**.
3. Conservation des utilisateurs, rôles et **soldes de crédits**.
4. Écriture du nouveau `data.json` V1 (aucune envie créée automatiquement).

## Installation (Arduino IDE 2.x)

### 1. Support des cartes ESP32
- Fichier → Préférences → **URL de gestionnaire de cartes supplémentaires** :
  `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- Outils → Type de carte → Gestionnaire de cartes → installer **esp32 by
  Espressif Systems** (version ≥ 3.0 conseillée).

### 2. Bibliothèque requise
- **ArduinoJson** de Benoit Blanchon (version 7.x). Aucune autre bibliothèque :
  `WebServer.h` et `LittleFS.h` sont fournis par le support ESP32, tout le
  CSS/HTML est embarqué dans le firmware.

### 3. Réglages de la carte (Outils)
| Réglage | Valeur |
|---|---|
| Type de carte | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled (conseillé pour les logs série) |
| Flash Size | **16MB** |
| Partition Scheme | **Custom** (utilise `partitions.csv` inclus) |
| PSRAM | **OPI PSRAM** *(N16R8 = PSRAM octale — indispensable)* |
| CPU Frequency | 240MHz |

> ℹ️ Le fichier `partitions.csv` crée une partition nommée `spiffs` avec le
> sous-type `littlefs` (9,9 Mo) : le core ESP32 3.x cherche la partition FS
> par son nom `spiffs`, et le scheme officiel 16 Mo fournit du FAT
> (incompatible LittleFS). L'avertissement esptool sur ce nom est bénin.

### 4. Compilation en ligne de commande (facultatif)
```
"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" ^
  compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=custom,CDCOnBoot=cdc,CPUFreq=240" ^
  <chemin\du\sketch>
```
(voir `dev.bat` de la version Node pour un exemple de script ; le téléversement
se fait via `upload -p COMx` — le numéro COM peut changer après un reset.)

## Utilisation

1. Téléphone → WiFi → réseau **`PPmark`** (clé : **`Poupoune`** — modifiable
   en haut de `lovemarket_esp32.ino`, variables `AP_SSID` / `AP_PASS`).
2. Ouvrir **http://192.168.4.1**.
3. Choisir son profil (Alice ou Sylvain) puis saisir son mot de passe.
4. Navigation (barre basse) : **Accueil · Mes envies · Ses envies ·
   Nos échanges · Nos moments**. L'administration n'apparaît que pour
   Sylvain (icône ⚙️ en haut à droite) et reste inaccessible à Alice,
   contrôle effectué côté serveur.

Notifications : badge discret sur la rubrique concernée (Ses envies /
Nos échanges / Nos moments) ; la LED intégrée respire en **bleu** pour
Sylvain, **rose** pour Alice, en alternance si les deux ont des notifications.

## Modèle de données (`data.json`, LittleFS)

| Clé | Contenu |
|---|---|
| `users` | id, passwordHash (sel$sha256), role, displayName, credits |
| `wishes` | envies : auteur, titre, description, catégorie, credits (0-4), desiredDate, status (`open`/`accepted`/`completed`/`cancelled`), acceptedBy, horodatages, memoryComment, readBy |
| `creditTransactions` | journal : userId, amount, type (`wish_completed`/`admin_adjustment`), wishId, description, date |
| `notifications` | userId, type, wishId, message, createdAt, readAt |
| `categories` | catégories d'envies (gérées dans l'admin) |
| `relationshipLevels` | niveaux : name, threshold (envies confirmées), icon |
| `schemaVersion` | 1 |

Un exemple complet et importable est fourni : **`data_example.json`**
(mots de passe : Alice `Poupoune`, Sylvain `31051989`).

## Structure du projet

```
lovemarket_esp32/
├── lovemarket_esp32.ino   Configuration WiFi AP, serveur, routes du cycle de vie
├── partitions.csv         Table de partitions 16 Mo (app 3+3 Mo, LittleFS 9,9 Mo)
├── model.h / model.cpp    Modèle V1, persistance atomique, migration V0, SHA-256, sessions
├── ui.h / ui.cpp          Layout (nav basse), CSS palette ivoire/bordeaux, favicon
├── pages.h                Déclarations des pages
├── pages_main.cpp         Login, accueil, Mes envies, formulaire d'envie, 404
├── pages_partner.cpp      Ses envies, Nos échanges, Nos moments
├── pages_admin.cpp        Administration (crédits journalisés, niveaux, catégories, resets)
├── data_example.json      Exemple de data.json compatible V1 (importable)
└── README.md              Ce fichier
```

## Checklist de validation (tests sur téléphone)

- [ ] Alice et Sylvain se connectent chacun avec leur profil.
- [ ] Créer une envie de 0 à 4 crédits ; la modifier ; la supprimer.
- [ ] Le partenaire découvre l'envie (badge « Ses envies ») et l'accepte.
- [ ] Une deuxième acceptation est refusée (envie n'est plus disponible).
- [ ] Le réalisateur signale la réalisation ; seul le bénéficiaire peut confirmer.
- [ ] À la confirmation : crédits attribués une seule fois, visibles dans le journal admin.
- [ ] Abandon / annulation : aucun crédit, l'envie redevient disponible ou disparaît.
- [ ] Nos moments s'alimente (album + commentaires).
- [ ] Badges de notifications + LED bleu/rose.
- [ ] Admin inaccessible à Alice (403 direct aussi).
- [ ] Export/import JSON depuis l'admin.
- [ ] Redémarrage de l'ESP32 : données conservées.

## Dépannage

- **Croquis trop volumineux** : vérifier le Partition Scheme (Custom + partitions.csv).
- **PSRAM non détectée** : régler PSRAM = "OPI PSRAM" (config N16R8).
- **Port série qui change de numéro (COM7 → COM8...)** : comportement de
  l'USB-CDC après un reset — resélectionner le port.
- **Page inaccessible** : vérifier que le téléphone est sur le WiFi `PPmark`
  et que le moniteur série affiche l'IP 192.168.4.1.
- **Données perdues après re-flash** : un simple téléversement préserve
  LittleFS, mais un effacement complet les supprime — exporter régulièrement
  via l'admin.
- **PC qui quitte le réseau PPmark** : Windows bascule automatiquement vers
  un WiFi avec internet ; tester depuis le téléphone.
