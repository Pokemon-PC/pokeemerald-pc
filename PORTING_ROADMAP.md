# Feuille de route — Pokémon Émeraude en exécutable PC natif

Objectif : un `.exe` Windows (puis Linux/macOS) compilé depuis la décomp pret,
sans émulateur, avec CMake + Clang + SDL2 + OpenGL, en gardant la ROM GBA
« matching » (`make compare` doit toujours donner `pokeemerald.gba: OK`).

Méthode : celle du port HeartGold (`pokeheartgold-master`), adaptée à la GBA.
Audit détaillé : [docs/PORTING_API_AUDIT.md](docs/PORTING_API_AUDIT.md).

## Règles

1. **ROM GBA matching** : tout changement hors de `src/platform_pc/` est sous
   `#ifdef PLATFORM_PC` (code d'origine dans `#else`). Le `Makefile` GBA ignore
   `src/platform_pc/`.
2. **Aucune sauvegarde ni ROM dans le dépôt.** Les données du jeu viennent du
   dépôt pret lui-même (PNG, JSON…) : pas besoin de ROM du joueur.
3. **Le code du jeu reste le code du jeu** : on émule le matériel GBA.
4. **Prouver avant d'affirmer** : capture d'image, journal ou autotest.

## Décisions

| Sujet | Choix | Raison |
|---|---|---|
| Compilateur PC | Clang (llvm-mingw sous Windows) | même que HGSS ; `-fms-extensions`, base fixe via lld |
| Mémoire | EWRAM/IWRAM/E/S/PLTT/VRAM/OAM réservées à leurs adresses GBA | les `REG_*` et `VRAM`/`OAM` du jeu marchent tels quels |
| Exécutable | base fixe 0x10000000, sans ASLR, pile sous 4 Gio | pointeurs 32 bits dans les scripts `.4byte` et les `u32` |
| Scripts `.s` (bytecode) | assemblés tels quels par Clang | pointeurs 32 bits valides grâce à la base basse |
| Tables `.s` typées (cartes, specials…) | converties en C | tailles de pointeurs 64 bits |
| BIOS | réécrit en C (`bios_pc.c`) | 43 fonctions, surtout décompression et copies |
| DMA | macros `Dma*` → `PC_DmaSet()` sous `PLATFORM_PC` | une écriture mémoire ne déclenche rien sur PC |
| Rendu | moteur 2D logiciel ligne par ligne (dérivé de HGSS), 240×160 | effets H-Blank du jeu (fondus, vagues, combats) |
| Son | mixeur m4a (`m4a_1.s`) réécrit en C, sortie SDL à 13379 Hz rééchantillonnée | m4a.c est déjà du C |
| Sauvegarde | Flash 128 Kio → `save.bin` | |
| Link / sans-fil | « câble débranché » | aucune partie réseau |
| Langue | anglais (dépôt US) | c'est ce que contient le dépôt |

## Jalons

| Jalon | Contenu | Critère de sortie | État |
|---|---|---|---|
| M0 Audit | inventaire matériel / asm / 64 bits | `docs/PORTING_API_AUDIT.md` | ✅ |
| M1 Squelette | CMake : outils natifs + données générées + HAL SDL2 | une image du jeu (PNG→4bpp→VRAM) affichée dans la fenêtre | ✅ |
| M2 Couche matériel | carte mémoire, base fixe, BIOS C, DMA, IRQ/VBlank, touches, Flash, RTC | autotest CTest vert | ✅ |
| M3 Tout le jeu | tout `src/` compilé avec Clang (via preproc), données `.s` assemblées/converties, link neutralisé | `AgbMain()` tourne jusqu'à l'écran titre sans plantage | ☐ |
| M4 2D | BG texte/affine, OAM, fenêtres, mélange, mosaïque, H-Blank/DMA H-Blank | intro + titre + monde affichés justes | ☐ |
| M5 Son | mixeur m4a en C + canaux PSG | musique de l'intro juste | ☐ |
| M6 Jeu & finitions | parties scriptées, bugs 64 bits, config, paquet | systèmes principaux testés en jeu | ☐ |
| M7 Multiplateforme | Linux/macOS, CI, vérification matching | CI verte 3 OS | ☐ |

## À faire par l'utilisateur (le moment venu)

- Vérifier le matching GBA pour de vrai : installer devkitARM (ou
  `binutils-arm-none-eabi`) et agbcc (libre, pret/agbcc) — demande d'accord avant
  tout téléchargement.
- Écoute du son, ressenti de la fluidité, manette réelle.
- Création d'un éventuel dépôt GitHub privé et lancement de la CI.

## Journal

- **2026-11-24** — Import du zip pret/pokeemerald sous git. Audit M0 (chiffres dans
  l'audit). Constats : pas de ROM nécessaire, pas d'overlay ni de 3D ; gros points
  durs = tables `.s` typées, mixeur `m4a_1.s`, DMA par macros, effets H-Blank.
  Début M1 : CMake des outils natifs.
  - Fait : `src/platform_pc/tools/CMakeLists.txt` compile les 11 outils avec Clang
    (libpng/zlib téléchargées par CMake) ; `make generated` passe avec `mingw32-make`.
  - En pause : `src/platform_pc/scripts/gen_assets.py` (≈ 8 200 fichiers à générer
    via les règles scaninc). Première passe interrompue à la demande de l'utilisateur.
  - Reprise : relancer `python src/platform_pc/scripts/gen_assets.py -j 16`,
    vérifier « données générées », puis HAL SDL2 + affichage d'une image du jeu.
- **2026-11-24 (reprise)** — Socle PC posé dans `src/platform_pc/` :
  - `hw_pc.c` : EWRAM/IWRAM/E/S/PLTT/VRAM/OAM réservées à leurs adresses GBA
    (avec la relance suspendue Windows du port HGSS) ; pile basse Linux/macOS.
  - `bios_pc.c` : CpuSet, CpuFastSet, LZ77/RL, Div, Sqrt, ArcTan2, BgAffineSet,
    ObjAffineSet, RegisterRamReset en C.
  - `gpu_pc.c` : moteur 2D logiciel ligne par ligne — modes 0 à 5, BG texte et
    affines, sprites normaux/affines, fenêtres 0/1/OBJ, mélange alpha et
    éclaircir/assombrir. Pas encore : mosaïque.
  - `CMakeLists.txt` racine : HAL + `pem_m1_view`, exe lié à 0x10000000 sans ASLR,
    en-têtes du jeu en `-iquote` (`include/malloc.h`, `strings.h` masqueraient ceux du système).
  - Le `Makefile` GBA exclut `src/platform_pc/` (seul changement hors du dossier PC).
  - **M1 atteint** : `pem_m1_view --shot` reproduit l'écran titre (logo affine en
    256 couleurs, nuages mélangés sur la silhouette de Rayquaza) avec les en-têtes et
    macros de registres du jeu. Piège vu : la palette du logo se convertit avec
    `-num_colors 224` (règle dédiée de `graphics_file_rules.mk`) ; passer par make.
  - Suite (M2) : DMA (`PC_DmaSet`), boucle VBlank/IRQ, touches, Flash, RTC, autotest CTest.
- **2026-11-24 — M2 atteint** (couche matériel, `src/platform_pc/`) :
  - `dma_pc.c` : 4 canaux ; immédiat, VBlank, HBlank avec répétition et
    destination « incrément + rechargement » (effets de ligne de `scanline_effect.c`) ;
    IRQ de fin de DMA. Le timing « spécial » (FIFO son) est laissé au son (M5).
  - `irq_pc.c` : 228 lignes par image à 59,7275 Hz. `pc_wait_vblank()` avance de
    là où on s'était arrêté jusqu'au début du VBlank : IRQ VCount, rendu de la
    ligne, DMA puis IRQ H-Blank, DMA puis IRQ V-Blank, dans l'ordre de priorité
    d'`IntrMain` (crt0.s). `PEM_FAST=1` enlève la cadence.
  - `video_pc.c` : fenêtre SDL, clavier (touches mGBA), manette SDL, plein écran
    F11/Alt+Entrée, `PEM_HEADLESS=1`, crochets de test (entrées, images).
  - `flash_pc.c` : remplace `agb_flash*.c` (mêmes symboles, MX29L010 128 Kio)
    sur `save.bin` (`PEM_SAVE`), écrit au VBlank via fichier temporaire + rename.
  - `rtc_pc.c` : remplace `siirtc.c` (GPIO dans la zone ROM, non émulable) :
    heure locale + décalage conservé dans `save.rtc` (`PEM_RTC`), en BCD.
  - `bios_pc.c` : + `VBlankIntrWait`/`IntrWait` = `pc_wait_vblank()`.
  - Jeu, sous `#ifdef PLATFORM_PC` : `include/gba/macro.h` (`DmaSet`/`DmaStop`
    → HAL, pointeurs 64 bits gardés), `src/main.c` (table d'IRQ donnée au HAL au
    lieu de copier `IntrMain` ; `WaitForVBlank` fait avancer le temps).
  - **Autotest** `ctest` (`pem_autotest`) : 59 vérifications vertes (carte mémoire,
    BIOS dont un vrai `.lz` du jeu, DMA immédiat et H-Blank ligne par ligne, IRQ
    et IME, touches, rendu/assombrissement, Flash + rechargement, RTC + batterie).
    Validé en cassant volontairement le rechargement DMA : 4 échecs détectés.
  - `scripts/check_gba_sources.py` : sources GBA identiques à l'import une fois le
    code PC retiré (0 différence) ; validé en injectant une modification non
    protégée dans `main.c` et dans le `Makefile` (détectées).
  - Données : `gen_assets.py` terminé (11 551 fichiers) ; le visualiseur M1 donne
    une image identique au pixel près avec les fichiers produits par `make`.
  - Suite (M3) : compiler tout `src/` avec Clang via `preproc`, données `.s`
    (bytecode assemblé tel quel, tables typées → C), `SoftReset`, link neutralisé.
