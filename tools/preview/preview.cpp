// Previews van het scherm met voorbeelddata.
// Gebruik: preview <week|maand|luisteren|voorstel|notitie|accu-laag|slaapstand|wakker-worden|melding>  > opdrachten.jsonl
#include <cstring>

#include "../../esphome/components/ink/ink_kalender.h"

using namespace esphome::display;

static const char *const VOORBEELD =
    "2026-10-01|-|Geen stroom\n"
    "2026-10-02|-|Spullen wegbrengen\n"
    "2026-10-03|-|Loek afzwemmen\n"
    "2026-10-05|09:30|Sanne pilates\n"
    "2026-10-05|17:00|⚽ Ties voetbaltraining\n"
    "2026-10-05|-|🎂 Elise jarig\n"
    "2026-10-07|-|Ties & Loek voetbalmiddag\n"
    "2026-10-08|-|Sanne ontwikkeldag\n"
    "2026-10-08|16:00|🏊 Loek zwemles\n"
    "2026-10-09|-|Verjaardag Faye\n"
    "2026-10-09|18:30|Ties extra training\n"
    "2026-10-10|11:00|Verjaardag Sylvia\n"
    "2026-10-10|-|Voetbal?\n"
    "2026-10-12|-|✈️ Vakantie\n"
    "2026-10-13|-|Vakantie\n"
    "2026-10-14|-|Vakantie\n"
    "2026-10-15|-|Efteling\n"
    "2026-10-16|-|Efteling\n"
    "2026-10-19|-|Studiedag Jeroen & Marilon\n"
    "2026-10-22|-|Loek logeerpartij\n"
    "2026-10-25|-|Sanne naar Fiona\n"
    "2026-10-26|12:00|Sanne coach\n"
    "2026-10-26|20:30|Sanne Ferdi\n"
    "2026-10-27|20:15|Sanne vergadering\n"
    "2026-10-30|-|Dieuwke weekend\n"
    "2026-11-02|08:30|🦷 Tandarts\n";

int main(int argc, char **argv) {
  const char *scenario = argc > 1 ? argv[1] : "week";

  BaseFont klein{"klein", 0}, normaal{"normaal", 1}, vet{"vet", 2}, kop{"kop", 3}, groot{"groot", 4};
  ink::Fonts f{&klein, &normaal, &vet, &kop, &groot, false};

  ink::Staat &s = ink::staat();
  s.vandaag = {2026, 10, 5};
  s.tijd_geldig = true;
  s.data_ontvangen = true;
  s.temperatuur = 21.3f;
  s.vochtigheid = 54.0f;
  s.accu = 87.0f;
  s.afspraken = ink::lees_afspraken(VOORBEELD);
  s.notities = ink::lees_notities(
      "todo.notities|a1|Cadeautje Faye\n"
      "todo.notities|a2|Weekend kamperen regelen\n"
      "todo.notities|a3|Brief naar juf\n"
      "todo.boodschappen|b1|Batterijen kopen\n"
      "todo.boodschappen|b2|Melk\n");
  s.notities[2].afgevinkt = true;
  s.ruw_weer =
      "2026-10-05|partlycloudy|17|9\n"
      "2026-10-06|rainy|14|10\n"
      "2026-10-07|cloudy|15|8\n"
      "2026-10-08|sunny|18|7\n"
      "2026-10-09|lightning-rainy|16|11\n"
      "2026-10-10|fog|12|6\n"
      "2026-10-11|snowy|3|-2\n";
  s.weer = ink::lees_weer(s.ruw_weer);
  s.knoppen[0] = {"Woonkamer", true};
  s.knoppen[1] = {"Keuken", false};
  s.knoppen[2] = {"Tuin", false};
  s.knoppen[3] = {"Alles uit", false};

  if (!std::strcmp(scenario, "maand")) {
    s.scherm = ink::SCHERM_MAAND;
  } else if (!std::strcmp(scenario, "luisteren")) {
    s.status = ink::LUISTEREN;
  } else if (!std::strcmp(scenario, "voorstel")) {
    s.status = ink::VOORSTEL;
    s.voorstel = {"Verjaardag oma Sylvia", "2026-10-10", "11:00", "13:00",
                  "zaterdag elf uur verjaardag oma sylvia tot een uur", "afspraak"};
  } else if (!std::strcmp(scenario, "notitie")) {
    s.status = ink::VOORSTEL;
    s.voorstel = {"Cadeautje voor Faye kopen", "", "", "", "notitie cadeautje voor faye kopen", "notitie"};
  } else if (!std::strcmp(scenario, "slaapstand")) {
    s.slaap = ink::SLAAPT;
  } else if (!std::strcmp(scenario, "wakker-worden")) {
    s.slaap = ink::WORDT_WAKKER;
    s.wek_stap = 1;
  } else if (!std::strcmp(scenario, "accu-laag")) {
    s.accu = 12.0f;
    s.verbonden = false;
  } else if (!std::strcmp(scenario, "melding")) {
    s.status = ink::MELDING;
    s.melding = "Ik hoorde \"doe de lampen uit\", maar dat is geen afspraak.";
  } else if (!std::strcmp(scenario, "dag")) {
    s.scherm = ink::SCHERM_MAAND;
    s.dag_gekozen = {2026, 10, 9};
    s.status = ink::DAG;
  } else if (!std::strcmp(scenario, "details")) {
    for (size_t i = 0; i < s.afspraken.size(); i++)
      if (s.afspraken[i].titel.find("zwemles") != std::string::npos)
        ink::open_details(s, static_cast<int>(i));
    ink::zet_details(s, "ja", "Loek zwemles", "2026-10-08", "16:00", "2026-10-08", "16:45", "Zwembad De Hoornse Vaart",
                     "Diploma C. Zwemkleding, handdoek en de pas meenemen. Ophalen bij de achteringang, "
                     "ouders mogen de laatste tien minuten kijken.",
                     "Sanne en Jeroen");
  }

  Display it;
  ink::teken(it, s, f, ink::VOL);
  return 0;
}
