# Vector Suite — plug-in nativo per Adobe Illustrator, versione Windows.
#
# Su macOS il plug-in si compila con il progetto Xcode che sta in questa stessa
# cartella. Qui c'è la strada Windows: stesso codice C++, stesso catalogo,
# stesse icone, pannello Win32 al posto di quello AppKit.
#
#   cmake -S native/VectorSuiteNative -B build/win -A x64
#   cmake --build build/win --config Release
#
# Prodotto: build/win/Release/VectorSuiteNative.aip, che è una DLL con
# estensione .aip, come vuole Illustrator.
#
# L'SDK non è nel repository: va scaricato da Adobe e messo in
# sdk/Adobe Illustrator 2026 SDK/. Le istruzioni sono in sdk/README.md.

cmake_minimum_required(VERSION 3.20)
project(VectorSuiteNative CXX)

if(NOT WIN32)
  message(FATAL_ERROR
    "Questo CMakeLists serve la build Windows. Su macOS usa "
    "scripts/build-native.sh, che compila il progetto Xcode.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VS_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(VS_SDK "${VS_PROJECT_ROOT}/sdk/Adobe Illustrator 2026 SDK")

if(NOT EXISTS "${VS_SDK}/illustratorapi")
  message(FATAL_ERROR
    "Manca l'Adobe Illustrator SDK.\n"
    "  Atteso in: sdk/Adobe Illustrator 2026 SDK/\n"
    "  Non è incluso nel progetto perché la licenza Adobe non ne permette la\n"
    "  ridistribuzione. Scaricalo gratuitamente con un Adobe ID da:\n"
    "    https://developer.adobe.com/console/servicesandapis/ai\n"
    "  Le istruzioni complete sono in sdk/README.md.")
endif()

# --- Sorgenti ---------------------------------------------------------------
#
# Il codice dei moduli è C++ portabile e non cambia fra i due sistemi. Cambia
# solo il pannello: VectorSuitePanel.mm su macOS, VectorSuitePanelWin.cpp qui.

set(VS_SOURCES
  Source/VectorSuiteCatalog.cpp
  Source/VectorSuiteGeometry.cpp
  Source/VectorSuiteModules.cpp
  Source/VectorSuitePlugin.cpp
  Source/VectorSuiteSuites.cpp
  Source/VectorSuitePanelWin.cpp
)

set(VS_SDK_SOURCES
  "${VS_SDK}/samplecode/common/source/Main.cpp"
  "${VS_SDK}/samplecode/common/source/Plugin.cpp"
  "${VS_SDK}/samplecode/common/source/Suites.cpp"
  "${VS_SDK}/samplecode/common/source/AppContext.cpp"
  "${VS_SDK}/samplecode/common/source/IllustratorSDK.cpp"
  "${VS_SDK}/samplecode/common/source/SDKAboutPluginsHelper.cpp"
  "${VS_SDK}/illustratorapi/illustrator/AIAssert.cpp"
  "${VS_SDK}/illustratorapi/illustrator/IAIUnicodeString.cpp"
  "${VS_SDK}/illustratorapi/illustrator/IAIStringFormatUtils.cpp"
  "${VS_SDK}/illustratorapi/illustrator/IAIArtboards.cpp"
)

foreach(source IN LISTS VS_SDK_SOURCES)
  if(NOT EXISTS "${source}")
    message(FATAL_ERROR
      "L'SDK non ha il file atteso:\n  ${source}\n"
      "Controlla di aver scompattato la versione 2026 dell'SDK.")
  endif()
endforeach()

# --- PiPL -------------------------------------------------------------------
#
# Illustrator riconosce un plug-in dal blocco PiPL. Lo genera uno script Python
# dell'SDK, che va lanciato prima del compilatore di risorse.

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin.pipl"
  COMMAND "${Python3_EXECUTABLE}"
          "${VS_SDK}/tools/pipl/create_pipl.py"
          -input "[{\"name\":\"VectorSuiteNative\",\"entry_point\":\"PluginMain\"}]"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Genero il PiPL del plug-in"
  VERBATIM
)
add_custom_target(VectorSuitePipl
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/plugin.pipl")

# --- Destinazione -----------------------------------------------------------

add_library(VectorSuiteNative SHARED
  ${VS_SOURCES}
  ${VS_SDK_SOURCES}
  Resources/Win/VectorSuiteNative.rc
)
add_dependencies(VectorSuiteNative VectorSuitePipl)

target_include_directories(VectorSuiteNative PRIVATE
  Source
  Resources/Win
  "${VS_SDK}/samplecode/common/includes"
  "${VS_SDK}/illustratorapi/adm"
  "${VS_SDK}/illustratorapi/ate"
  "${VS_SDK}/illustratorapi/illustrator"
  "${VS_SDK}/illustratorapi/illustrator/actions"
  "${VS_SDK}/illustratorapi/illustrator/legacy"
  "${VS_SDK}/illustratorapi/pica_sp"
)

# Il compilatore di risorse cerca i file citati nel .rc lungo questi percorsi:
# gli SVG stanno in raw/, i PNG e la mappa in Win/, il PiPL nella cartella del
# progetto, VersionInfo.rc dentro l'SDK.
set_source_files_properties(Resources/Win/VectorSuiteNative.rc PROPERTIES
  COMPILE_FLAGS
    "/I\"${CMAKE_CURRENT_SOURCE_DIR}/Resources/Win\" \
     /I\"${CMAKE_CURRENT_SOURCE_DIR}/Resources/raw\" \
     /I\"${CMAKE_CURRENT_SOURCE_DIR}\" \
     /I\"${VS_SDK}/samplecode/common/win\" \
     /I\"${VS_SDK}/samplecode/common/includes\"")

target_compile_definitions(VectorSuiteNative PRIVATE
  WIN_ENV=1
  ISOLATION_AWARE_ENABLED=1
  UNICODE
  _UNICODE
  _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(VectorSuiteNative PRIVATE
  gdiplus
  comctl32
  user32
  gdi32
  advapi32
  ole32
  shlwapi
)

# Illustrator carica il plug-in dal punto d'ingresso PluginMain, che va
# esportato: il file .def dell'SDK dichiara esattamente quello.
set_target_properties(VectorSuiteNative PROPERTIES
  OUTPUT_NAME "VectorSuiteNative"
  SUFFIX ".aip"
  PREFIX ""
  LINK_FLAGS "/DEF:\"${VS_SDK}/samplecode/common/win/PluginMain.def\""
)

# L'SDK si aspetta due inclusioni forzate: pragma.h per le impostazioni del
# compilatore e IllustratorSDK.h come intestazione comune. Il progetto di
# esempio le usa come intestazione precompilata; qui bastano come inclusioni
# forzate, il risultato è lo stesso e la configurazione è più corta.
target_compile_options(VectorSuiteNative PRIVATE
  /FI"pragma.h"
  /FI"IllustratorSDK.h"
  /Zc:sizedDealloc-
  /bigobj
  /wd4996
)

install(TARGETS VectorSuiteNative
        RUNTIME DESTINATION "Plug-ins")
