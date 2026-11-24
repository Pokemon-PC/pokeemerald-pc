# Pokémon Émeraude — version PC native / native PC port

[![PC port](https://github.com/Pokemon-PC/pokeemerald-pc/actions/workflows/pc.yml/badge.svg)](https://github.com/Pokemon-PC/pokeemerald-pc/actions/workflows/pc.yml)
[![GBA ROM](https://github.com/Pokemon-PC/pokeemerald-pc/actions/workflows/build.yml/badge.svg)](https://github.com/Pokemon-PC/pokeemerald-pc/actions/workflows/build.yml)

**Projet de fans, non affilié à Nintendo, Game Freak, The Pokémon Company ni
à [pret](https://pret.github.io/).** Le port PC a été écrit avec des agents IA :
n'envoyez rien d'ici à pret et ne les sollicitez pas à son sujet.

(English below)

## Français

Pokémon Émeraude qui tourne **nativement sur PC** (Windows, Linux), compilé à
partir de la décompilation [pret/pokeemerald](https://github.com/pret/pokeemerald),
**sans émulateur** : le code du jeu est compilé pour le PC, et seul le matériel
de la Game Boy Advance (affichage, son, sauvegarde, minuteries) est reproduit.

- **Aucune ROM nécessaire** pour jouer en anglais : tout le jeu est construit
  depuis ce dépôt.
- **Français** avec votre propre cartouche : posez la ROM de *votre* Pokémon
  Version Émeraude française à côté de l'exécutable ; ses textes remplacent les
  textes anglais au lancement (≈ 93 % des textes et tous les noms).
- **La ROM GBA d'origine se construit toujours à l'identique** : tout le code PC
  est dans `src/platform_pc/` ou sous `#ifdef PLATFORM_PC`, et la CI le prouve à
  chaque commit (`make compare` → `pokeemerald.gba: OK`).

### Jouer

| GBA | Clavier |
|---|---|
| A / B | X / Z |
| Start / Select | Entrée / Retour arrière |
| Croix | Flèches |
| L / R | A / S |
| Plein écran | F11 ou Alt+Entrée |

Une manette reconnue par SDL fonctionne aussi. Tout se règle dans `config.ini`,
créé à côté de l'exécutable au premier lancement : touches, taille de fenêtre,
plein écran, volume, langue (`[game] language = auto | en | fr`).
La sauvegarde `save.bin` a le format de la cartouche.

### Construire

Prérequis : Clang, CMake ≥ 3.20, Ninja, Python 3, GNU binutils (`as`) et make.
Windows : llvm-mingw + WinLibs ; Linux : paquets indiqués dans
[README_PC.md](README_PC.md).

```
cmake -S src/platform_pc/tools -B build-tools -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-tools
python src/platform_pc/scripts/gen_assets.py -j 16
cmake -B build-pc -G Ninja -DCMAKE_C_COMPILER=clang
cmake --build build-pc
cd build-pc && ctest
```

Résultat : `build-pc/pokeemerald(.exe)`. Paquet à distribuer :
`python src/platform_pc/scripts/package.py` (l'exécutable et la notice, jamais
de ROM ni de sauvegarde).

### État

Testé en jeu : intro, titre, nouvelle partie, premier acte complet (horloge,
rivale, route 101, choix du starter, premier combat, labo), combats, menus,
sac, carte de dresseur, équipe, résumé, sauvegarde et reprise, son et musique.
Cinq tests automatiques jouent le jeu sans fenêtre (démarrage avec son, monde +
combat, menus + sauvegarde, histoire, couche matériel), sous Windows et Linux.
Pas de câble link ni d'adaptateur sans fil (comme une cartouche seule).

### Documentation

- [README_PC.md](README_PC.md) : construire, jouer, options de test et de débogage
- [PORTING_ROADMAP.md](PORTING_ROADMAP.md) : journal détaillé du portage
- [docs/LAUNCHER.md](docs/LAUNCHER.md) : réglages pilotables par un lanceur (langue, ROM, sauvegarde)
- [INSTALL.md](INSTALL.md) : construire la ROM GBA (outils pret, inchangé)

### Mentions légales

Pokémon est une marque de Nintendo, Creatures Inc. et Game Freak. Ce dépôt ne
contient ni ROM ni données extraites d'une ROM ; le jeu en français lit la ROM
que *vous* possédez. Ne distribuez pas de ROM.

## English

Pokémon Emerald running **natively on PC** (Windows, Linux), built from the
[pret/pokeemerald](https://github.com/pret/pokeemerald) decompilation **without
an emulator**: the game code is compiled for the PC and only the Game Boy
Advance hardware (video, sound, save, timers) is reproduced.

- **No ROM needed** to play in English: the whole game is built from this repository.
- **French** with your own cartridge: put the ROM of *your* French Pokémon
  Emerald next to the executable; its text replaces the English text at start
  (≈ 93 % of the text and all names).
- **The original GBA ROM still builds byte-for-byte**: all PC code is in
  `src/platform_pc/` or under `#ifdef PLATFORM_PC`, checked by CI on every commit.

Controls: A/B = X/Z, Start/Select = Enter/Backspace, D-pad = arrows, L/R = A/S,
F11 = fullscreen; gamepads work. Settings in `config.ini` (created next to the
executable). Build: see the commands above, then `build-pc/pokeemerald(.exe)`.
Details: [README_PC.md](README_PC.md) (French), [PORTING_ROADMAP.md](PORTING_ROADMAP.md).

**Fan project, not affiliated with Nintendo, Game Freak, The Pokémon Company or
pret.** The PC port was written with AI agents: do not send anything from here
upstream to pret. No ROM or ROM data is included; do not distribute ROMs.
