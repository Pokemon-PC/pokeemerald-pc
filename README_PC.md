# Pokémon Émeraude — version PC native

Exécutable PC compilé depuis la décompilation pret/pokeemerald, sans émulateur.
La ROM GBA se construit toujours avec `make` comme avant (sources inchangées :
tout le code PC est dans `src/platform_pc/` ou sous `#ifdef PLATFORM_PC`).

Pas besoin de ROM : toutes les données du jeu sont dans ce dépôt.

## Prérequis (Windows)

- **Clang** (llvm-mingw), **CMake** ≥ 3.20, **Ninja**
- **GNU binutils + make** (WinLibs : fournit `as` et `mingw32-make`) — l'assembleur GNU
  est nécessaire pour les données du jeu, celui de LLVM ne suffit pas
- **Python 3**

SDL2, zlib et libpng sont téléchargés et compilés automatiquement par CMake.

## Prérequis (Linux)

```
sudo apt install clang cmake ninja-build python3 binutils make libasound2-dev libpulse-dev libx11-dev libxext-dev libgl-dev
```

Mêmes commandes de construction (avec `python3`) ; l'exe s'appelle `build-pc/pokeemerald`.
macOS (Intel) est prévu dans le code mais pas encore testé.

## Construire

```
cmake -S src/platform_pc/tools -B build-tools -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-tools
python src/platform_pc/scripts/gen_assets.py -j 16
cmake -B build-pc -G Ninja -DCMAKE_C_COMPILER=clang
cmake --build build-pc
```

1. les outils de la décomp (`gbagfx`, `preproc`, `mapjson`…) ;
2. les données (graphismes, cartes, musique : ~10 min la première fois) ;
3. le jeu : `build-pc/pokeemerald.exe`.

Tests : `cd build-pc && ctest` (couche matériel, démarrage avec son, monde + combat,
menus + sauvegarde + reprise, premier acte de l'histoire jusqu'au labo).

Paquet à distribuer : `python src/platform_pc/scripts/package.py` → `dist/pokeemerald-pc-<commit>.zip`
(exe + README ; jamais de sauvegarde, de ROM ni de config).

## Jouer

Lancer `pokeemerald.exe` depuis un dossier où il peut écrire : la sauvegarde
`save.bin` (128 Kio, même format que la cartouche) et l'horloge `save.rtc` y sont créées.

| GBA | Clavier |
|---|---|
| A / B | X / Z |
| Start / Select | Entrée / Retour arrière |
| Croix | Flèches |
| L / R | A / S |
| Plein écran | F11 ou Alt+Entrée |

Touches, taille de fenêtre, plein écran et volume se règlent dans **`config.ini`**,
créé à côté de l'exe au premier lancement (noms de touches SDL, plusieurs par bouton
séparés par des virgules).

Une manette reconnue par SDL fonctionne aussi (disposition Nintendo).
Le jeu démarre en **mono** comme l'original (Options → Son → Stéréo).

## Variables d'environnement (tests et débogage)

| Variable | Effet |
|---|---|
| `PEM_SCALE=n` | taille de la fenêtre (×3 par défaut) |
| `PEM_MUTE=1` | sans son |
| `PEM_SAVE=f`, `PEM_RTC=f` | autres fichiers de sauvegarde / d'horloge |
| `PEM_HEADLESS=1`, `PEM_FAST=1` | sans fenêtre, sans limite de vitesse |
| `PEM_EXIT_AFTER=N` | quitter après N images |
| `PEM_INPUT=400-404:START,1700-7400/25:A` | touches scriptées (`/P` = appui toutes les P images) |
| `PEM_SHOT=900:titre.bmp` | capture d'une image |
| `PEM_WAV=son.wav` | enregistrer le son |
| `PEM_TRACE=1` | journal des changements de carte / position / contrôle |
| `PEM_TEST=givemon@N,battle@N` | donner un Gobou, lancer un combat sauvage |
| `PEM_REGS=N`, `PEM_PEEK=ADDR:LEN` | registres, VRAM, tâches ; mémoire brute (adresses de `nm`) |
| `PEM_HANG_EXIT=1` | quitter si le jeu se bloque (le chien de garde affiche la pile) |
| `PEM_WATCH=0xADDR` (+`PEM_WATCH_LEN`, `PEM_WATCH_ALL`) | point d'arrêt matériel en écriture |

Plantages et blocages s'affichent avec une pile d'adresses ;
`python src/platform_pc/scripts/symbolize.py journal.txt` les traduit en fonctions/lignes.

## Particularités

- Les lectures/écritures à travers un pointeur nul, inoffensives sur GBA (l'adresse 0 est
  le BIOS), sont reproduites comme sur GBA et signalées une fois dans le journal
  (`[pem:nullread]`) : ce sont des bugs du jeu d'origine.
- Pas de câble link ni d'adaptateur sans-fil : les menus correspondants répondent
  « pas de connexion », comme une cartouche seule.

Journal détaillé du portage : [PORTING_ROADMAP.md](PORTING_ROADMAP.md).
