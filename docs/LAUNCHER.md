# Intégration avec le launcher (à lire avant de toucher au choix de langue)

**Décision de l'utilisateur (2026-11-25)** : le choix de la langue se fera
**dans son launcher**, pas dans le jeu. Il enverra le code du launcher plus
tard. Ne pas ajouter de menu de langue dans le jeu : exposer seulement des
réglages simples que le launcher pilote, et adapter cette page quand le code du
launcher arrive. Même décision pour HeartGold / SoulSilver
(`pokeheartgold-pc/docs/pc_port/LAUNCHER.md`).

## Ce que le jeu offre déjà au launcher

| Quoi | Comment | Remarques |
|---|---|---|
| Langue | variable d'environnement `PEM_LANG=en\|fr` | priorité sur `config.ini` |
| | `config.ini` → `[game] language = auto\|en\|fr` | `auto` = français si la ROM française est trouvée |
| ROM française | variable `PEM_ROM=chemin.gba` ou `[game] rom = chemin` | sinon : un `*.gba` à côté de l'exe ou dans le dossier courant |
| Sauvegarde | `PEM_SAVE=fichier` | défaut `save.bin` à côté de l'exe ; même sauvegarde pour les deux langues |
| Fenêtre | `PEM_SCALE=n`, `[video] scale`, `fullscreen` | |

L'exe ne prend pas d'arguments en ligne de commande : tout passe par
l'environnement ou `config.ini` (l'ajout d'arguments `--lang`/`--rom` est
simple si le launcher le préfère).

## Comment marche le français

- L'exe contient le jeu anglais complet (construit depuis la décomp).
- Le français est lu **au lancement** dans la ROM française du joueur
  (« Pokémon Version Émeraude (France) », révision 0, code `BPEF`, CRC-32
  `A3FDCCB1`) ; une autre ROM est refusée et le jeu reste en anglais (le
  journal l'indique : `lang: …`).
- Positions des textes : `src/platform_pc/lang/fr_bpef.tsv` (aucun texte de
  Nintendo) ; ~93 % des textes et 11 tables de noms. Le reste, et les textes
  dessinés dans les images (titre), restent en anglais.
- Pour savoir quelle langue a été appliquée : ligne `[pem] lang: French from …`
  ou `lang: English` sur la sortie standard.

## Ce qui ne doit jamais passer par le launcher

La ROM du joueur : il la fournit lui-même. Un launcher qui distribue la ROM
distribue le jeu.
