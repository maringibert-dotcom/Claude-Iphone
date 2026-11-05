# MASALA · epices audio

Synthé wavetable de poche pensé pour la **bass music** (dubstep, riddim, tribe, deep/dub, halftime),
dans la DA **epices audio** : orange `#F2541B`, noir `#1E1B16`, police Bricolage Grotesque, symbole aux deux
anneaux (repris dans les potards : un anneau et un point qui indique la valeur). Fichiers de marque dans `app/brand/`.

On fait son sound design sur le téléphone, on enregistre, et le preset arrive dans le plugin **VST3 / AU**
pour Ableton, Logic, FL Studio… grâce au compte epices.

```
┌──────────────────────┐   presets .masala (JSON)   ┌──────────────────────┐
│  Appli téléphone     │ ─────────────────────────▶ │  Serveur compte      │
│  app/  (PWA iPhone)  │ ◀───────────────────────── │  server/ (Node)      │
│  moteur : dsp.js     │                            └──────────┬───────────┘
└──────────────────────┘                                       │ code d'appairage
                                                               ▼  à 6 chiffres
                                                    ┌──────────────────────┐
                                                    │  Plugin VST3 / AU    │
                                                    │  plugin/ (JUCE)      │
                                                    │  moteur : Engine.cpp │
                                                    └──────────────────────┘
```

Le moteur sonore existe en deux versions **identiques** (JavaScript pour le téléphone, C++ pour le plugin).
`tests/parity.mjs` vérifie qu'un preset donne le même signal des deux côtés (écart max ≈ −75 dB sur toute la banque).

## Le synthé

| Bloc | Contenu |
|---|---|
| **Osc A / Osc B** | 6 wavetables (Basic, Growl, Vowel, Metal, Tribal, Digital) × 8 frames, position, unisson 7 voix, detune, stéréo, warp (Bend / Sync / Mirror) |
| **Sub / Bruit / FM** | sub sinus/triangle/carré à −1 ou −2 oct, bruit, FM de B vers A |
| **Filtre** | SVF LP24 / LP12 / BP / HP / Notch, drive, enveloppe, keytrack |
| **Enveloppes** | Env 1 (amplitude), Env 2 (modulation) |
| **LFO 1 / 2** | 6 formes, synchro tempo (4/1 → 1/32, triolets, pointées), retrig |
| **Matrice** | 8 slots source → destination (LFO, env, vélocité, 4 macros, mod wheel, note → cutoff, positions, pitch, FM, warp, niveaux, detune, FX…) |
| **FX** | Distorsion (Tube, Hard, Fold, Crush, Rectify), Squash (compression haute + basse façon OTT), Delay synchro ping-pong, Reverb |
| **Macros** | 4 macros nommées par preset, pilotables au pad XY sur le téléphone |

**25 presets d'usine** originaux : *Harissa Wobble*, *Paprika Growl*, *Chipotle Yoi*, *Piment Riddim*,
*Tambour Cumin*, *Transe Curcuma*, *Rituel Nomade*, *Bristol Sub*, *Poivre Noir Reese*, *Gingembre Neuro*,
*Safran Glitch*, *Zaatar Halftime*… Ils s'inspirent des esthétiques de la scène (wobbles, growls, rollers tribe,
subs deep/dub) sans copier le son d'un artiste.

## Lancer l'appli

```bash
npm start              # http://localhost:8787/app/  (appli + API compte)
```

Sur iPhone : ouvre l'URL du serveur dans **Safari** → Partager → **Sur l'écran d'accueil**.
L'appli s'ouvre alors en plein écran et marche hors-ligne (le compte demande du réseau).
⚠️ Désactive le mode silencieux de l'iPhone, sinon le Web Audio est muet.

Pour l'utiliser ailleurs qu'en local, déploie `server/` (Render, Fly.io, un VPS…) derrière HTTPS :
`PORT` et `DATA_DIR` se règlent par variables d'environnement. Les comptes sont stockés dans un
fichier JSON (mots de passe hachés en scrypt, jetons hachés) — suffisant pour démarrer, à remplacer
par une vraie base de données avant un lancement public.

## Synchroniser avec le plugin

1. Sur le téléphone : ☁︎ → **Créer un compte** (ou Connexion).
2. Enregistre tes presets : ils partent sur ton compte.
3. Dans le plugin : **Compte → Serveur** (l'URL de ton serveur), puis **Compte → Entrer le code du téléphone**.
4. Sur le téléphone : ☁︎ → **Générer un code** → tape les 6 chiffres dans le plugin.
5. Tes presets apparaissent dans le menu du plugin, rubrique *Compte epices (cloud)*.
   Les presets enregistrés dans le plugin repartent vers le téléphone à la synchro suivante.

Hors-ligne aussi : **Exporter fichier** (téléphone) produit un `.masala`, que le plugin importe
(menu des presets → *Importer un fichier .masala*). Le dossier des presets est
`Documents/epices/Masala/Presets`.

## Compiler le plugin (VST3 / AU / Standalone)

```bash
cmake -S plugin -B build -DCMAKE_BUILD_TYPE=Release   # télécharge JUCE 8.0.4
cmake --build build --config Release
```

- macOS : produit `Masala.vst3`, `Masala.component` (AU) et l'appli autonome. Copie-les dans
  `~/Library/Audio/Plug-Ins/VST3` et `~/Library/Audio/Plug-Ins/Components`.
- Windows : `Masala.vst3` → `C:\Program Files\Common Files\VST3`.
- La CI GitHub (`.github/workflows/build.yml`) compile macOS + Windows à chaque push et publie les binaires en artefacts.

## Structure

```
tools/params.mjs      ← source unique des 102 paramètres
tools/presets.mjs     ← banque d'usine
tools/build.mjs       → shared/params.json, shared/presets/factory.json, plugin/Source/ParamDefs.h
app/                  appli téléphone (PWA, sans dépendances)
server/server.mjs     compte, presets cloud, appairage (Node ≥ 18, sans dépendances)
plugin/               plugin JUCE (CMake)
tests/                rendu hors-ligne + parité JS ↔ C++
```

Après modification des paramètres ou des presets : `npm run build && npm test`.

## Pistes pour la suite

- **App Store** : la PWA est la voie la plus rapide pour tester sur iPhone. Pour une vraie appli iOS
  (et un **AUv3** utilisable dans GarageBand / Cubasis), le moteur C++ et `PluginEditor` se réutilisent
  tels quels dans une cible JUCE iOS/AUv3.
- Embarquer la police Bricolage Grotesque (licence OFL) dans le plugin : pour l'instant il utilise la police
  système, l'appli la charge depuis Google Fonts. Couleurs centralisées dans `app/css/style.css` (`:root`)
  et `epices::` dans `PluginEditor.h`.
- Import de wavetables utilisateur, oscillateur à formants, multibande pour le Squash.
