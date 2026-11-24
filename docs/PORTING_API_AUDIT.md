# Audit M0 — pokeemerald vers PC natif

Méthode : la skill « nds-decomp-pc-port » (port HeartGold), adaptée à la GBA.
Chiffres relevés le 2026-11-24 sur pret/pokeemerald (archive `pokeemerald-master.zip`).

## 1. Différences GBA / DS qui changent la méthode

| Sujet | Port DS (HeartGold) | pokeemerald (GBA) |
|---|---|---|
| Compilateur d'origine | mwccarm (propriétaire) | agbcc (libre, dans `tools/agbcc` à part) ou GCC « modern » |
| SDK | NitroSDK + NitroSystem (à recompiler) | aucun : le jeu parle directement aux registres |
| Données du jeu | extraites de la ROM du joueur (NitroFS) | **toutes dans le dépôt** (PNG, JSON, .s, .mid, .wav) → aucune ROM nécessaire |
| Fichiers | NitroFS → fichiers | pas de système de fichiers : tout est lié dans l'exécutable via `INCBIN` (outil `preproc`) |
| Overlays | oui, reset doux par sections | non |
| 3D | rastériseur logiciel | aucune |
| Son | pilote ARM7 réécrit | moteur m4a (`src/m4a.c` en C + mixeur `src/m4a_1.s` en ARM, 1916 lignes) |
| Sauvegarde | CARD → `save.bin` | Flash 128 Kio (`agb_flash*.c`) → `save.bin` |
| Horloge | RTC DS | RTC Seiko via GPIO (`siirtc.c`) |
| Sans-fil | WM → « radio vide » | câble link / adaptateur sans-fil (librfu) → « câble débranché » |

Ce qui reste identique : carte mémoire native aux vraies adresses, base fixe basse,
pile sous 4 Gio, Clang, matching préservé par `#ifdef PLATFORM_PC`, moteur 2D logiciel.
Le moteur 2D DS est un sur-ensemble de celui de la GBA : il est réutilisable.

## 2. Carte mémoire à réserver (adresses natives)

| Zone | Adresse | Taille | Usage dans le jeu |
|---|---|---|---|
| EWRAM | 0x02000000 | 256 Kio | `EWRAM_DATA` (708 variables), tas `gHeap` |
| IWRAM | 0x03000000 | 32 Kio | `.bss`, `COMMON_DATA`, `IWRAM_DATA` (44 fichiers), tampon `IntrMain_Buffer` |
| E/S | 0x04000000 | 1 Kio | `REG_*` : 144 registres distincts, 2412 références |
| Palettes | 0x05000000 | 1 Kio | `PLTT` |
| VRAM | 0x06000000 | 96 Kio | `VRAM` |
| OAM | 0x07000000 | 1 Kio | `OAM` |
| ROM | 0x08000000 | — | données liées dans l'exe ; seul `libisagbprn.c` (débogage) adresse la ROM en dur |
| Flash | 0x0E000000 | 128 Kio | remplacée par HLE (`save.bin`) |

Les en-têtes `include/gba/io_reg.h` et `defines.h` utilisent déjà ces adresses en
dur : une fois la mémoire réservée, ils marchent sans modification.

## 3. Points d'accès au matériel

- **Appels BIOS** (`libagbsyscall`, 43 fonctions) — à réécrire en C :
  `LZ77UnCompVram` 93×, `LZ77UnCompWram` 72×, `RLUnCompVram` 17×, `CpuSet` 10×,
  `ObjAffineSet` 6×, `RLUnCompWram` 5×, `CpuFastSet` 3×, `BgAffineSet` 3×,
  `VBlankIntrWait`, `Sqrt`, `SoftReset`, `RegisterRamReset` 2×, `Div`, `ArcTan2`,
  `MultiBoot` 1×, plus les `SoundDriver*`/`MusicPlayer*` du BIOS (non utilisés par le jeu, m4a est en ROM).
- **DMA** : macros `Dma*` de `include/gba/macro.h` (≈ 170 appels). Elles écrivent dans
  `REG_DMAxSAD/DAD/CNT` : sous `PLATFORM_PC`, une écriture d'activation doit exécuter la
  copie. Choix : les macros appellent `PC_DmaSet()` (copie immédiate, ou différée pour
  les DMA H-Blank/V-Blank/son).
- **Interruptions** : `gIntrTable` (main.c) + `IntrMain` copié en IWRAM (crt0.s).
  HLE : boucle à 59,7275 Hz qui appelle VCount/H-Blank/V-Blank/Timer/Série dans l'ordre du matériel.
- **asm en ligne** : `script_vm.c` (`svc 2`, HALT), `m4a.c` (`swi 0x2A`), `libisagbprn.c`,
  `librfu_intr.c`, `multiboot.c` (NAKED) → équivalents C sous `PLATFORM_PC`.
- **asm** : `crt0.s` (démarrage, IntrMain), `m4a_1.s` (mixeur son), `libgcnmultiboot.s`
  (GameCube), `rom_header.s` → réécrits en C ou remplacés.

## 4. Données en assembleur (risque 64 bits majeur)

`data/*.s` est assemblé par GNU as. Deux familles :

1. **Flux d'octets (bytecode)** lus octet par octet, pointeurs codés sur 4 octets
   (`T1_READ_PTR`) : scripts d'événements, de combat, d'IA, d'animations
   (`battle_anim_scripts.s` 395 `.4byte`, `battle_scripts_1.s` 214…).
   Avec la base fixe basse, des pointeurs 32 bits restent valides : on peut les
   assembler **tels quels** avec l'assembleur intégré de Clang (relocations 32 bits absolues).
2. **Tables typées lues comme des structures C** — tailles fausses en 64 bits, à convertir en C :
   - `gScriptCmdTable` (`data/script_cmd_table.inc`), `gSpecials` (`data/specials.inc`) :
     tableaux de pointeurs de fonction ;
   - en-têtes de carte, connexions, événements (`data/maps/*/header.inc`, `events.inc`,
     `connections.inc`, `data/layouts/layouts.inc`) générés par `mapjson` → ajouter une
     sortie C à `mapjson` (ou un script de conversion, cf. `asm_data_to_c.py`) ;
   - `data/maps.s`, `map_events.s`, `sound_data.s` (tables de chansons `struct Song`).

## 5. Risques 64 bits repérés

- Pointeurs rangés dans des entiers : `SetWordTaskArg/GetWordTaskArg` (pointeur coupé
  en deux `s16`), `(u32)&…` — ≈ 41 cas → couverts par base basse + pile basse.
- `sizeof` de structures à pointeurs utilisées comme données brutes (sauvegarde,
  Cadeau Mystère, échanges link) → à scanner (classe « taille DS en dur » de la skill).
- `STATIC_ASSERT` sur la taille des blocs de sauvegarde : doivent rester justes (pas de pointeur dedans).
- Code écrit pour `int` 32 bits / `long` 32 bits : `long` = 64 bits sous Linux/macOS (LP64),
  32 bits sous Windows (LLP64) → tester les deux.
- Lectures au-delà de la fin de tables (tolérées par la ROM GBA, pas par l'exe).

## 6. Communication (à neutraliser proprement)

35 fichiers : `link.c`, `link_rfu_*.c`, `librfu_*.c`, `union_room*.c`, `trade.c`,
`record_mixing.c`, `mystery_gift*.c`, `ereader_*.c`, `multiboot.c`, `berry_fix_*`, etc.
Stratégie « câble débranché » : le registre série ne répond jamais, l'adaptateur sans-fil
n'est pas détecté → les menus échouent avec les messages d'origine du jeu.

## 7. Chaîne de génération des données

Outils natifs (`tools/`, 11 programmes C/C++) : `gbagfx` (libpng), `preproc`, `mapjson`,
`jsonproc`, `mid2agb`, `wav2agb`, `scaninc`, `ramscrgen`, `bin2c`, `gbafix`, `rsfont`.
Pipeline C d'origine : `cpp | preproc (INCBIN, chaînes _("…") via charmap.txt) | cc1`.
Côté PC : `clang -E | preproc | clang -x cpp-output` (265 `INCBIN` dans `src/`).

Machine actuelle : Clang 23 (llvm-mingw), CMake 4.4, Ninja, GCC (WinLibs),
`mingw32-make`, Python 3.12. Pas de libpng → récupérée par CMake. Pas de devkitARM :
le build GBA ne peut pas être vérifié ici ; on vérifie le matching au niveau des sources.
