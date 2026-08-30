#include "iosuilocalization.h"

#include "game/constants.h"
#include "gothic.h"

namespace {

using DeviceText = IosUiLocalization::DeviceSettingsText;
using PadText    = IosUiLocalization::PadDiagramText;

struct LanguageText {
  DeviceText device;
  PadText    pad;
  };

// GthFont maps one byte to one glyph in the active game codepage. Keep these
// literals out of UTF-8: RU is CP1251, PL/CZ are CP1250, and the western
// languages are CP1252. Adjacent literals terminate \xNN before an ASCII hex
// digit, which would otherwise be consumed by the C++ escape.
const LanguageText& textFor(ScriptLang language) {
  static const LanguageText en = {
    {"Device settings", "Frame rate", "Off",
     "Frame rate is controlled by the parent configuration", "Back"},
    {
      "Controller layout",
      "Draw bow / Block / Aim",
      "Left attack / Walk / Previous page",
      "Move / Turn",
      "Sneak",
      "Item quick-ring",
      "Quest log / Previous target",
      "Map / Next target",
      "Weapons / Magic ring",
      "Draw melee / Attack / Shoot / Cast",
      "Right attack / Look back / Next page",
      "Draw / sheathe weapon",
      "Melee special",
      "Jump / Climb",
      "Interact / Use",
      "Camera",
      "Target lock / Edit item ring",
      "Inventory",
      "Game menu",
    }
  };
  static const LanguageText de = {
    {"Ger\xE4teeinstellungen", "Bildrate", "Aus",
     "Bildrate wird von der \xFC" "bergeordneten Konfiguration gesteuert",
     "Zur\xFC" "ck"},
    {
      "Controller-Belegung",
      "Bogen ziehen / Blocken / Zielen",
      "Linker Angriff / Gehen / Vorige Seite",
      "Bewegen / Drehen",
      "Schleichen",
      "Gegenstands-Rad",
      "Tagebuch / Vorheriges Ziel",
      "Karte / N\xE4" "chstes Ziel",
      "Waffen- / Magie-Rad",
      "Nahkampf / Angriff / Schuss / Zauber",
      "Rechter Angriff / R\xFC" "ckblick / N\xE4" "chste Seite",
      "Waffe ziehen / wegstecken",
      "Nahkampf-Spezialangriff",
      "Springen / Klettern",
      "Interagieren / Benutzen",
      "Kamera",
      "Ziel fixieren / Gegenstands-Rad belegen",
      "Inventar",
      "Spielmen\xFC",
    }
  };
  static const LanguageText pl = {
    {"Ustawienia urz\xB9" "dzenia", "Limit FPS", "Wy\xB3\xB9" "czony",
     "Limit FPS jest ustawiony przez konfiguracj\xEA nadrz\xEA" "dn\xB9",
     "Powr\xF3t"},
    {
      "Uk\xB3" "ad kontrolera",
      "Dobycie \xB3uku / Blok / Celowanie",
      "Atak w lewo / Ch\xF3" "d / Poprzednia strona",
      "Ruch / Obr\xF3t",
      "Skradanie",
      "Ko\xB3o przedmiot\xF3w",
      "Dziennik zada\xF1 / Poprzedni cel",
      "Mapa / Nast\xEApny cel",
      "Ko\xB3o broni / Magii",
      "Bro\xF1 bia\xB3" "a / Atak / Strza\xB3 / Czar",
      "Atak w prawo / Spojrzenie wstecz / Nast\xEApna strona",
      "Dob\xB9" "d\x9F / schowaj bro\xF1",
      "Specjalny atak wr\xEA" "cz",
      "Skok / Wspinaczka",
      "Interakcja / U\xBFycie",
      "Kamera",
      "Blokada celu / Edycja ko\xB3" "a przedmiot\xF3w",
      "Ekwipunek",
      "Menu gry",
    }
  };
  static const LanguageText ru = {
    {"\xCD\xE0\xF1\xF2\xF0\xEE\xE9\xEA\xE8 \xF3\xF1\xF2\xF0\xEE\xE9\xF1\xF2\xE2\xE0",
     "\xD7\xE0\xF1\xF2\xEE\xF2\xE0 \xEA\xE0\xE4\xF0\xEE\xE2",
     "\xC2\xFB\xEA\xEB.",
     "\xD7\xE0\xF1\xF2\xEE\xF2\xE0 \xEA\xE0\xE4\xF0\xEE\xE2 \xE7\xE0\xE4\xE0\xED\xE0 \xF0\xEE\xE4\xE8\xF2\xE5\xEB\xFC\xF1\xEA\xEE\xE9 \xEA\xEE\xED\xF4\xE8\xE3\xF3\xF0\xE0\xF6\xE8\xE5\xE9",
     "\xCD\xE0\xE7\xE0\xE4"},
    {
      "\xD0\xE0\xF1\xEA\xEB\xE0\xE4\xEA\xE0 \xEA\xEE\xED\xF2\xF0\xEE\xEB\xEB\xE5\xF0\xE0",
      "\xCD\xE0\xF2\xFF\xED\xF3\xF2\xFC \xEB\xF3\xEA / \xC1\xEB\xEE\xEA / \xCF\xF0\xE8\xF6\xE5\xEB",
      "\xC0\xF2\xE0\xEA\xE0 \xF1\xEB\xE5\xE2\xE0 / \xD8\xE0\xE3 / \xCF\xF0\xE5\xE4. \xF1\xF2\xF0\xE0\xED\xE8\xF6\xE0",
      "\xC4\xE2\xE8\xE6\xE5\xED\xE8\xE5 / \xCF\xEE\xE2\xEE\xF0\xEE\xF2",
      "\xCA\xF0\xE0\xF1\xF2\xFC\xF1\xFF",
      "\xCA\xEE\xEB\xFC\xF6\xEE \xEF\xF0\xE5\xE4\xEC\xE5\xF2\xEE\xE2",
      "\xC6\xF3\xF0\xED\xE0\xEB / \xCF\xF0\xE5\xE4\xFB\xE4\xF3\xF9\xE0\xFF \xF6\xE5\xEB\xFC",
      "\xCA\xE0\xF0\xF2\xE0 / \xD1\xEB\xE5\xE4\xF3\xFE\xF9\xE0\xFF \xF6\xE5\xEB\xFC",
      "\xCE\xF0\xF3\xE6\xE8\xE5 / \xCA\xEE\xEB\xFC\xF6\xEE \xEC\xE0\xE3\xE8\xE8",
      "\xC4\xEE\xF1\xF2\xE0\xF2\xFC \xEE\xF0\xF3\xE6\xE8\xE5 / \xC0\xF2\xE0\xEA\xE0 / \xC2\xFB\xF1\xF2\xF0\xE5\xEB / \xC7\xE0\xEA\xEB\xE8\xED\xE0\xED\xE8\xE5",
      "\xC0\xF2\xE0\xEA\xE0 \xF1\xEF\xF0\xE0\xE2\xE0 / \xC2\xE7\xE3\xEB\xFF\xE4 \xED\xE0\xE7\xE0\xE4 / \xD1\xEB\xE5\xE4. \xF1\xF2\xF0\xE0\xED\xE8\xF6\xE0",
      "\xC4\xEE\xF1\xF2\xE0\xF2\xFC / \xF3\xE1\xF0\xE0\xF2\xFC \xEE\xF0\xF3\xE6\xE8\xE5",
      "\xCE\xF1\xEE\xE1\xE0\xFF \xE0\xF2\xE0\xEA\xE0",
      "\xCF\xF0\xFB\xE6\xEE\xEA / \xCF\xEE\xE4\xFA\xB8\xEC",
      "\xC2\xE7\xE0\xE8\xEC\xEE\xE4\xE5\xE9\xF1\xF2\xE2\xE8\xE5 / \xC8\xF1\xEF\xEE\xEB\xFC\xE7\xEE\xE2\xE0\xF2\xFC",
      "\xCA\xE0\xEC\xE5\xF0\xE0",
      "\xC7\xE0\xF5\xE2\xE0\xF2 \xF6\xE5\xEB\xE8 / \xCD\xE0\xF1\xF2\xF0\xEE\xE8\xF2\xFC \xEA\xEE\xEB\xFC\xF6\xEE \xEF\xF0\xE5\xE4\xEC\xE5\xF2\xEE\xE2",
      "\xC8\xED\xE2\xE5\xED\xF2\xE0\xF0\xFC",
      "\xCC\xE5\xED\xFE \xE8\xE3\xF0\xFB",
    }
  };
  static const LanguageText fr = {
    {"R\xE9glages de l'appareil", "Fr\xE9quence d'images", "D\xE9sactiv\xE9",
     "La configuration parente contr\xF4le la fr\xE9quence d'images", "Retour"},
    {
      "Disposition de la manette",
      "Arc / Parade / Vis\xE9" "e",
      "Attaque gauche / Marche / Page pr\xE9" "c\xE9" "dente",
      "D\xE9placement / Rotation",
      "Discr\xE9tion",
      "Roue rapide des objets",
      "Journal / Cible pr\xE9" "c\xE9" "dente",
      "Carte / Cible suivante",
      "Roue armes / magie",
      "Arme de m\xEAl\xE9" "e / Attaque / Tir / Sort",
      "Attaque droite / Vue arri\xE8re / Page suivante",
      "D\xE9gainer / rengainer",
      "Attaque sp\xE9" "ciale",
      "Saut / Escalade",
      "Interagir / Utiliser",
      "Cam\xE9ra",
      "Verrouiller cible / Modifier roue d'objets",
      "Inventaire",
      "Menu du jeu",
    }
  };
  static const LanguageText es = {
    {"Configuraci\xF3n del dispositivo", "Frecuencia de fotogramas", "Desactivado",
     "La configuraci\xF3n superior controla la frecuencia de fotogramas", "Atr\xE1s"},
    {
      "Distribuci\xF3n del mando",
      "Tensar arco / Bloquear / Apuntar",
      "Ataque izquierdo / Caminar / P\xE1gina anterior",
      "Mover / Girar",
      "Sigilo",
      "Rueda r\xE1pida de objetos",
      "Diario / Objetivo anterior",
      "Mapa / Objetivo siguiente",
      "Rueda de armas / magia",
      "Arma cuerpo a cuerpo / Atacar / Disparar / Hechizo",
      "Ataque derecho / Mirar atr\xE1s / P\xE1gina siguiente",
      "Sacar / guardar arma",
      "Ataque especial",
      "Saltar / Trepar",
      "Interactuar / Usar",
      "C\xE1mara",
      "Fijar objetivo / Editar rueda de objetos",
      "Inventario",
      "Men\xFA del juego",
    }
  };
  static const LanguageText it = {
    {"Impostazioni dispositivo", "Frequenza fotogrammi", "Disattivato",
     "La configurazione principale controlla la frequenza fotogrammi", "Indietro"},
    {
      "Mappatura controller",
      "Tendi arco / Parata / Mira",
      "Attacco sinistro / Cammina / Pagina precedente",
      "Movimento / Rotazione",
      "Modalit\xE0 furtiva",
      "Selezione rapida oggetti",
      "Diario / Bersaglio precedente",
      "Mappa / Bersaglio successivo",
      "Ruota armi / magia",
      "Arma da mischia / Attacco / Tiro / Incantesimo",
      "Attacco destro / Guarda indietro / Pagina successiva",
      "Estrai / riponi arma",
      "Attacco speciale",
      "Salta / Arrampicati",
      "Interagisci / Usa",
      "Telecamera",
      "Aggancia bersaglio / Modifica ruota oggetti",
      "Inventario",
      "Menu di gioco",
    }
  };
  static const LanguageText cz = {
    {"Nastaven\xED za\xF8\xEDzen\xED", "Sn\xEDmkov\xE1 frekvence", "Vypnuto",
     "Sn\xEDmkovou frekvenci \xF8\xED" "d\xED nad\xF8" "azen\xE1 konfigurace",
     "Zp\xECt"},
    {
      "Rozlo\x9E" "en\xED ovlada\xE8" "e",
      "Nat\xE1hnout luk / Kryt / M\xED\xF8" "en\xED",
      "\xDAtok zleva / Ch\xF9ze / P\xF8" "edchoz\xED strana",
      "Pohyb / Ot\xE1\xE8" "en\xED",
      "Pl\xED\x9E" "en\xED",
      "Rychl\xE9 kolo p\xF8" "edm\xECt\xF9",
      "Den\xEDk / P\xF8" "edchoz\xED c\xEDl",
      "Mapa / Dal\x9A\xED c\xEDl",
      "Kolo zbran\xED / magie",
      "Tasen\xED zbran\xEC / \xDAtok / St\xF8" "elba / Kouzlo",
      "\xDAtok zprava / Pohled zp\xECt / Dal\x9A\xED strana",
      "Tasen\xED / schov\xE1n\xED zbran\xEC",
      "Speci\xE1ln\xED \xFAtok",
      "Skok / \x8Aplh\xE1n\xED",
      "Interakce / Pou\x9Eit\xED",
      "Kamera",
      "Zamknout c\xEDl / Upravit kolo p\xF8" "edm\xECt\xF9",
      "Invent\xE1\xF8",
      "Hern\xED menu",
    }
  };

  switch(language) {
    case ScriptLang::DE: return de;
    case ScriptLang::PL: return pl;
    case ScriptLang::RU: return ru;
    case ScriptLang::FR: return fr;
    case ScriptLang::ES: return es;
    case ScriptLang::IT: return it;
    case ScriptLang::CZ: return cz;
    case ScriptLang::EN: return en;
    case ScriptLang::NONE:
    default:             return en;
    }
  }

}

ScriptLang IosUiLocalization::currentLanguage() {
  const int value = Gothic::settingsGetI("GAME","language");
  switch(ScriptLang(value)) {
    case ScriptLang::EN:
    case ScriptLang::DE:
    case ScriptLang::PL:
    case ScriptLang::RU:
    case ScriptLang::FR:
    case ScriptLang::ES:
    case ScriptLang::IT:
    case ScriptLang::CZ:
      return ScriptLang(value);
    case ScriptLang::NONE:
    default:
      return ScriptLang::EN;
    }
  }

const IosUiLocalization::DeviceSettingsText&
IosUiLocalization::deviceSettings(ScriptLang language) {
  return textFor(language).device;
  }

const IosUiLocalization::PadDiagramText&
IosUiLocalization::padDiagram(ScriptLang language) {
  return textFor(language).pad;
  }
