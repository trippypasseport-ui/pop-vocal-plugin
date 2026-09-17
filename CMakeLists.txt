cmake_minimum_required(VERSION 3.22)

project(PopVocal VERSION 0.1.0)

# --- Récupération de JUCE ---
# Pour un usage interne, FetchContent est le plus simple : pas besoin
# de gérer un submodule Git à la main. Première configuration = premier
# téléchargement (~200 Mo), les configurations suivantes utilisent le cache.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4          # fixe une version connue plutôt que "master"
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# --- Déclaration du plugin ---
juce_add_plugin(PopVocal
    COMPANY_NAME        "DemStudio"          # à personnaliser
    PLUGIN_MANUFACTURER_CODE Dstd            # 4 caractères, premier en majuscule
    PLUGIN_CODE         Pvoc                 # 4 caractères, unique par plugin
    FORMATS              AU                  # usage interne Logic -> AU seul
                                              # (ajoute VST3 ici plus tard si besoin
                                              #  d'un autre DAW : FORMATS AU VST3)
    PRODUCT_NAME         "Pop Vocal"
    IS_SYNTH              FALSE
    NEEDS_MIDI_INPUT      FALSE
    NEEDS_MIDI_OUTPUT     FALSE
    IS_MIDI_EFFECT        FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE            # installe direct dans ~/Library/Audio/Plug-Ins/Components
)

target_sources(PopVocal
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(PopVocal
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(PopVocal
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
