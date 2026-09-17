# PopVocal — plugin AU interne

Vocal strip mono-compresseur, portage de l'algorithme "Pop" d'Airwindows
(licence MIT, voir THIRD-PARTY-LICENSE-airwindows.txt) vers un plugin
Audio Unit autonome, via JUCE.

## Statut

Code écrit et structuré, **jamais compilé ni testé à l'oreille**.
À valider avant tout usage en production — voir "Vérifications à faire"
plus bas.

## Build (macOS, Xcode requis)

```bash
cd PopVocalPlugin
cmake -B build -G Xcode
cmake --build build --config Release
```

La première configuration télécharge JUCE via FetchContent (~200 Mo,
une seule fois, mis en cache dans `build/_deps`).

`COPY_PLUGIN_AFTER_BUILD TRUE` dans le CMakeLists installe automatiquement
le `.component` dans `~/Library/Audio/Plug-Ins/Components/` après le build.
Redémarre Logic Pro (ou fais Contrôle > Réanalyser les plug-ins) pour
qu'il apparaisse dans la liste des Audio Units.

## Signature de code

Pour un usage local (pas de distribution), un simple Apple ID développeur
gratuit suffit — Xcode signe automatiquement avec un certificat de
développement local. Aucun compte payant (99$/an) n'est nécessaire tant
que le plugin reste sur ta machine.

## Vérifications à faire avant usage en production

1. **Ça compile ?** Premier test brut — erreurs de syntaxe, API JUCE
   mal utilisée, etc.
2. **`auval` passe ?** — `auval -v aufx Pvoc Dstd` en ligne de commande
   valide que macOS reconnaît le plugin comme un AU correct.
3. **Ça charge dans Logic sans crash ?**
4. **Comparaison à l'oreille avec le Pop original d'Airwindows** —
   même signal vocal, mêmes réglages d'Intensity. Le portage dans
   `Source/PopCompressor.h` a été réécrit à la main à partir du code
   source original ; toute divergence de son signale une erreur de
   portage à corriger.
5. **Test de silence total en entrée** — vérifier l'absence de bruit
   de fond, de clics au bypass, ou de dérive numérique sur un signal
   nul prolongé.

## Structure

```
PopVocalPlugin/
├── CMakeLists.txt
├── THIRD-PARTY-LICENSE-airwindows.txt   (obligation licence MIT)
└── Source/
    ├── PopCompressor.h      (le DSP — cœur de l'algo)
    ├── PluginProcessor.h/.cpp
    └── PluginEditor.h/.cpp   (3 sliders : Intensity, Output, Mix)
```

## Prochaines étapes possibles

- EQ paramétrique (voir référence modEQ mentionnée précédemment)
- Reverb via `juce::dsp::Reverb`
- Presets sauvegardés (programState au-delà du simple apvts)
