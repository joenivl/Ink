# Ink – gezinskalender op de reTerminal E1003

Doel: de whiteboard-planner op de koelkast vervangen door een Seeed reTerminal E1003
(10,3" e-paper), gekoppeld aan Home Assistant. Kalender tonen, HA bedienen en
afspraken toevoegen, liefst met spraak.

## Hardware (E1003)

| | |
|---|---|
| Scherm | 10,3" e-paper, 1404 × 1872, 16 grijstinten |
| Refresh | volledig scherm ~3 s, deel van het scherm 2–3 s (DU-modus, snel maar met ghosting) |
| Touch | capacitief (GT911) |
| SoC | ESP32-S3, 8 MB PSRAM, 32 MB flash, microSD |
| Audio | PDM-microfoon en een buzzer. **Geen speaker.** |
| Sensoren | temperatuur en luchtvochtigheid (SHT4x), RTC (PCF8563) |
| Accu | 3000 mAh (~6 maanden bij 1 refresh per dag) |

## Software-opties

1. **ESPHome (aanbevolen)**: wordt officieel ondersteund vanaf ESPHome 2026.7.0
   (display en touch). Volledige HA-integratie, en touchzones roepen HA-acties aan.
   - Voorbeeld van Seeed: `Seeed-Projects/esphome-reterminal-e1003-workspace`
     (bitmap als achtergrond, kleine DU-updates per regio, touchzones die HA-services aanroepen)
   - Voorbeeld van de community: `ar0v3r/reTerminal-E1003-ESPHome`
     (Google Calendar en weer via HA, deep sleep met een goed geoptimaliseerd stroomverbruik)
2. **SenseCraft HMI / Seeedash**: de standaardfirmware van Seeed, gemaakt met een
   drag-and-drop editor. Snel resultaat, maar beperkt.
3. **TRMNL (BYOD)**: TRMNL ondersteunt de E1003 (firmware 1.8.7 of nieuwer) en heeft
   een kant-en-klare kalender-plugin. Alleen weergave: geen HA-bediening en geen invoer.

## Architectuur

```
Google/iCloud/Local calendar ──► Home Assistant ──► ESPHome-API ──► E1003
                                     ▲    │
             spraak (PDM-mic) ───────┘    └─► calendar.create_event
```

- **Weergave**: kalenderdata komt uit HA (`calendar.get_events`). Het scherm wordt
  op het apparaat zelf getekend: weekoverzicht bovenaan, maandoverzicht onderaan,
  net als het huidige whiteboard. Een alternatief is een PNG laten renderen in HA,
  die het apparaat dan ophaalt met `online_image`. Dat geeft meer layoutvrijheid.
- **HA bedienen**: vaste touchzones (lampen, scènes, enzovoort). Rekening houden met
  ~0,5–3 s vertraging. Prima voor knoppen, niet geschikt voor schuifregelaars.
- **Afspraak via spraak**:
  1. Tik op "+ Afspraak". Het apparaat neemt op via `voice_assistant` (push-to-talk,
     een speaker is niet nodig).
  2. HA Assist-pipeline, STT: Nederlands werkt het best met HA Cloud (Nabu Casa).
     Lokaal Whisper kan ook, maar is traag of minder goed tenzij je sterke hardware hebt.
  3. De tekst gaat naar een LLM (AI Task of een conversation agent), die er
     `{titel, start, eind, wie}` van maakt.
  4. Het scherm toont "Za 10 okt 11:00 – Verjaardag Sylvia [Opslaan] [Annuleer]".
  5. Bij Opslaan volgt `calendar.create_event` (werkt in elk geval met Google Calendar
     en Local Calendar).

## Aandachtspunten

- Touch en spraak vragen dat het apparaat wakker blijft. Gebruik dan USB-C-voeding;
  de accu is alleen geschikt voor de modus "alleen weergave" met deep sleep.
- Een wake word dat altijd luistert (`micro_wake_word`) is mogelijk op de S3, maar
  kost stroom. Push-to-talk ligt meer voor de hand.
- DU-refresh geeft ghosting. Doe periodiek een volledige GC16-refresh, bijvoorbeeld
  's nachts of elk uur.
- Voor ESPHome is versie 2026.7.0 of nieuwer nodig.

## Open vragen

- Welke kalender gebruiken we: Google, iCloud of Outlook?
- Waar draait HA (Green, Pi of NUC), en hebben we Nabu Casa?
- Komt er een stopcontact of USB-kabel bij de koelkast?
- Welke HA-bediening willen we op het scherm?
