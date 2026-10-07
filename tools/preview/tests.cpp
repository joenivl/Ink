// Eenheidstests voor de datum- en parse-logica in ink_kalender.h.
#include <cassert>
#include <cstdio>

#include "../../esphome/ink_kalender.h"

using namespace ink;

int main() {
  // Datum-rekenwerk
  assert(dagnummer({1970, 1, 1}) == 0);
  assert(weekdag({1970, 1, 1}) == 3);   // donderdag
  assert(weekdag({2026, 10, 5}) == 0);  // maandag
  assert(weekdag({2026, 10, 4}) == 6);  // zondag
  for (int n = -1000; n < 30000; n += 7) {
    const Datum d = van_dagnummer(n);
    assert(dagnummer(d) == n);
  }
  assert((plus_dagen({2026, 12, 31}, 1) == Datum{2027, 1, 1}));
  assert((plus_dagen({2028, 2, 28}, 1) == Datum{2028, 2, 29}));
  assert((plus_dagen({2026, 3, 1}, -1) == Datum{2026, 2, 28}));

  // ISO-weeknummers
  assert(iso_week({2026, 10, 5}) == 41);
  assert(iso_week({2026, 1, 1}) == 1);
  assert(iso_week({2027, 1, 1}) == 53);  // vrijdag -> week 53 van 2026
  assert(iso_week({2024, 12, 30}) == 1);

  // Parsen
  auto lijst = lees_afspraken(
      "2026-10-05|09:30|Pilates\r\n"
      "2026-10-05|-|Elise jarig\n"
      "kapot\n"
      "2026-13-01|-|Ongeldige maand\n"
      "2026-10-06||Lege tijd\n"
      "2026-10-07|10:00|Titel met | streep");
  assert(lijst.size() == 4);
  assert(lijst[0].tijd == "09:30" && lijst[0].titel == "Pilates");
  assert(lijst[1].tijd.empty() && lijst[1].titel == "Elise jarig");
  assert(lijst[2].tijd.empty() && lijst[2].titel == "Lege tijd");
  assert(lijst[3].titel == "Titel met | streep");
  assert(lees_afspraken("").empty());

  // Iconen: bekende blijven, onbekende en emoji-hulptekens gaan weg
  assert(is_icoon(0x1F382) && is_icoon(0x23F0) && !is_icoon(0x1F1F3));
  assert(zonder_onbekende_iconen("🎂 Elise jarig") == "🎂 Elise jarig");
  assert(zonder_onbekende_iconen("✈️ Vakantie") == "✈ Vakantie");               // variatiekiezer FE0F
  assert(zonder_onbekende_iconen("🇳🇱 Koningsdag 🦄") == "Koningsdag");          // vlag, onbekend icoon
  assert(zonder_onbekende_iconen("👍🏽 Ok") == "👍 Ok");                          // huidskleur
  assert(zonder_onbekende_iconen("👨‍👩‍👧 Uitje") == "👨👩👧 Uitje");              // ZWJ-reeks
  assert(zonder_onbekende_iconen("Café – “thee” …") == "Café – “thee” …");
  assert(zonder_onbekende_iconen("kapot \xF0\x9F") == "kapot");                // afgebroken UTF-8
  assert(lees_afspraken("2026-10-05|-|🎉🎉 Feest ✨")[0].titel == "🎉🎉 Feest ✨");
  assert(lees_notities("todo.a|u1|🛒 Melk 🥛")[0].tekst == "🛒 Melk");

  auto regels = lees_regels("a\n\nb\r\n");
  assert(regels.size() == 2 && regels[1] == "b");

  auto notities = lees_notities("todo.a|u1|Melk\nlos\ntodo.b|u2|Brood | kaas\ntodo.c|u3|\n");
  assert(notities.size() == 3);
  assert(notities[0].lijst == "todo.a" && notities[0].uid == "u1" && notities[0].tekst == "Melk");
  assert(notities[1].uid.empty() && notities[1].tekst == "los");
  assert(notities[2].tekst == "Brood | kaas");

  // Weer
  auto weer = lees_weer(
      "2026-10-05|rainy|14|9\n"
      "2026-10-06|sunny|18|\n"
      "2026-10-07|iets-nieuws|12\n"
      "kapot|sunny|1|2\n"
      "2026-10-08|fog\n");
  assert(weer.size() == 3);
  assert(weer[0].icoon == REGEN && weer[0].max == "14" && weer[0].min == "9");
  assert(weer[1].icoon == ZON && weer[1].min.empty());
  assert(weer[2].icoon == GEEN_ICOON && weer[2].max == "12" && weer[2].min.empty());
  assert(weer_icoon("lightning-rainy") == ONWEER && weer_icoon("partlycloudy") == HALF_BEWOLKT);

  // Weer overnemen: meteen als er niets staat, daarna pas in een nieuw tijdvak
  const int dag = dagnummer({2026, 10, 5});
  assert(weer_vak(dag, 5, 6) == weer_vak(dag, 0, 6));
  assert(weer_vak(dag, 6, 6) != weer_vak(dag, 5, 6));
  assert(weer_vak(dag + 1, 0, 6) != weer_vak(dag, 18, 6));
  assert(weer_vak(dag, -1, 6) == -1);
  WeerGeheugen g{};
  assert(neem_weer_over(g, "a", weer_vak(dag, 12, 6)));
  assert(!neem_weer_over(g, "a", weer_vak(dag, 12, 6)));   // ongewijzigd
  assert(!neem_weer_over(g, "b", weer_vak(dag, 17, 6)));   // zelfde tijdvak (12-18)
  assert(std::string(g.tekst) == "a");
  assert(neem_weer_over(g, "b", weer_vak(dag, 18, 6)));    // nieuw tijdvak
  assert(neem_weer_over(g, "c", weer_vak(dag + 1, 6, 6))); // nieuwe dag
  assert(neem_weer_over(g, "", weer_vak(dag + 1, 6, 6)));  // weer uitgezet: meteen weg
  assert(neem_weer_over(g, "d", weer_vak(dag + 1, 6, 6))); // en meteen weer terug
  assert(neem_weer_over(g, "e", -1));                      // tijd onbekend: overnemen
  assert(neem_weer_over(g, std::string(500, 'x') + "\n", 0) && g.tekst[0] == '\0');  // te lang

  // Spaarstand: slaapduur (elke 30 min, 's nachts 23-6 door tot 6:00)
  assert(slaapduur_ms(14, 0, 30, 23, 6) == 30u * 60 * 1000);
  assert(slaapduur_ms(23, 0, 30, 23, 6) == 7u * 60 * 60 * 1000);
  assert(slaapduur_ms(2, 30, 30, 23, 6) == 210u * 60 * 1000);
  assert(slaapduur_ms(6, 0, 30, 23, 6) == 30u * 60 * 1000);
  assert(slaapduur_ms(-1, 0, 30, 23, 6) == 30u * 60 * 1000);  // tijd onbekend
  assert(slaapduur_ms(1, 0, 30, 0, 6) == 300u * 60 * 1000);   // nacht zonder middernacht-overgang

  // Inhoud-hash: verandert met de agenda en de datum, niet met de schermstatus
  Staat a, b;
  a.ruw_afspraken = b.ruw_afspraken = "2026-10-05|-|x";
  b.status = LUISTEREN;
  assert(inhoud_hash(a) == inhoud_hash(b));
  b.vandaag = {2026, 10, 6};
  assert(inhoud_hash(a) != inhoud_hash(b));
  b.vandaag = a.vandaag;
  b.ruw_weer = "2026-10-05|rainy|14|9";
  assert(inhoud_hash(a) != inhoud_hash(b));

  // UTF-8
  const std::string s = "café";
  assert(utf8_terug(s, s.size()) == 3);

  std::puts("alle tests geslaagd");
  return 0;
}
