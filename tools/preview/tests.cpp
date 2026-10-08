// Eenheidstests voor de datum- en parse-logica in ink_kalender.h.
#include <cassert>
#include <cstdio>

#include "../../esphome/components/ink/ink_kalender.h"

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
  assert(lijst[0].tijd.empty() && lijst[0].titel == "Elise jarig");  // hele dag eerst
  assert(lijst[1].tijd == "09:30" && lijst[1].titel == "Pilates");
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
  assert(neem_weer_over(g, "b", weer_vak(dag, 17, 6), true) && std::string(g.tekst) == "b");  // ververst toch
  assert(neem_weer_over(g, "a", weer_vak(dag, 17, 6), true));  // terug naar a voor de rest van de test
  assert(neem_weer_over(g, "b", weer_vak(dag, 18, 6)));    // nieuw tijdvak
  assert(neem_weer_over(g, "c", weer_vak(dag + 1, 6, 6))); // nieuwe dag
  assert(neem_weer_over(g, "", weer_vak(dag + 1, 6, 6)));  // weer uitgezet: meteen weg
  assert(neem_weer_over(g, "d", weer_vak(dag + 1, 6, 6))); // en meteen weer terug
  assert(neem_weer_over(g, "e", -1));                      // tijd onbekend: overnemen
  assert(neem_weer_over(g, std::string(500, 'x') + "\n", 0) && g.tekst[0] == '\0');  // te lang

  // Weer van voorbije dagen deze week blijft bewaard
  {
    const int wo = dagnummer({2026, 10, 7});  // woensdag
    WeerGeheugen g{};
    assert(neem_weer_over(g, "2026-10-05|sunny|17|9\n2026-10-06|rainy|14|10\n2026-10-07|cloudy|15|8", weer_vak(wo - 2, 6, 6)));
    assert(neem_weer_over(g, "2026-10-07|fog|13|7\n2026-10-08|sunny|18|7", weer_vak(wo, 6, 6)));
    assert(std::string(g.tekst) ==
           "2026-10-05|sunny|17|9\n2026-10-06|rainy|14|10\n2026-10-07|fog|13|7\n2026-10-08|sunny|18|7");
    // Nieuwe week (maandag): het verleden van vorige week valt weg
    assert(neem_weer_over(g, "2026-10-12|sunny|16|8", weer_vak(wo + 5, 6, 6)));
    assert(std::string(g.tekst) == "2026-10-12|sunny|16|8");
    // Weer uitgezet: alles weg
    assert(neem_weer_over(g, "", weer_vak(wo + 5, 7, 6)) && g.tekst[0] == '\0');
  }

  // Afgevinkte notities: blijven even staan op hun plek, daarna weg
  {
    KlaarGeheugen g{};
    const int64_t t0 = 1790000000;
    auto ha = lees_notities("todo.a|u1|Melk\ntodo.a|u2|Brood\ntodo.a|u3|Kaas\n");
    onthoud_klaar(g, ha[1], 1, t0);
    // HA heeft hem nog als open: doorgestreept op dezelfde plek
    auto l = met_klaar(ha, g, t0 + 5, 60);
    assert(l.size() == 3 && l[1].uid == "u2" && l[1].afgevinkt && !l[0].afgevinkt);
    // HA laat hem weg: komt terug op zijn oude plek
    auto zonder = lees_notities("todo.a|u1|Melk\ntodo.a|u3|Kaas\n");
    auto l2 = met_klaar(zonder, g, t0 + 60, 60);
    assert(zelfde_notities(l, l2));
    // Na 60 minuten weg
    auto l3 = met_klaar(zonder, g, t0 + 3600, 60);
    assert(l3.size() == 2 && !g.n[0].gebruikt);
    // Tijd onbekend: niet laten verlopen
    onthoud_klaar(g, ha[2], 2, t0);
    assert(met_klaar(zonder, g, 0, 60).size() == 2 && met_klaar(zonder, g, 0, 60)[1].afgevinkt);
    // Terugzetten
    vergeet_klaar(g, ha[2]);
    assert(!met_klaar(zonder, g, t0, 60)[1].afgevinkt);
    // Plek voorbij het einde: achteraan; vol geheugen: oudste eruit
    onthoud_klaar(g, ha[2], 9, t0);
    assert(met_klaar({}, g, t0, 60).size() == 1);
    for (int i = 0; i < 9; i++) {
      Notitie n{"todo.a", "x" + std::to_string(i), "t", false};
      onthoud_klaar(g, n, 0, t0 + 10 + i);
    }
    assert(met_klaar({}, g, t0 + 20, 60).size() == 8);
    // Lange tekst: netjes afgekapt, niet midden in een UTF-8-teken
    Notitie lang{"todo.a", "l", std::string(94, 'a') + "é", false};
    onthoud_klaar(g, lang, 0, t0 + 30);
    for (const auto &k : g.n)
      if (std::string(k.uid) == "l")
        assert(std::string(k.tekst) == std::string(94, 'a'));
  }

  // Aanraken van notities: rij eronder, dichtstbijzijnde kolom
  assert(notitie_op(MARGE + 20, NOTITIE_TOP + 5) == 0);
  assert(notitie_op(MARGE + 20, NOTITIE_TOP + NOTITIE_REGEL + 5) == 1);
  assert(notitie_op(MARGE + NOTITIE_KOL_B + 20, NOTITIE_TOP + 5) == NOTITIE_PER_KOLOM);  // tussenruimte, rechterhelft
  assert(notitie_op(MARGE + NOTITIE_KOL_B + 5, NOTITIE_TOP + 5) == 0);                  // tussenruimte, linkerhelft
  assert(notitie_op(MARGE + 20, NOTITIE_TOP - 30) == -1);
  assert(notitie_op(MARGE + 20, NOTITIE_TOP + NOTITIE_PER_KOLOM * NOTITIE_REGEL) == -1);

  // Details van een afspraak
  {
    Staat s;
    s.afspraken = lees_afspraken("2026-10-08|16:00|Zwemles 🏊\n2026-10-09|-|Weekend weg");
    assert(s.afspraken[0].ruw == "Zwemles 🏊");
    assert(s.afspraken[0].id.empty());
    auto met_id = lees_afspraken("2026-10-08~3f9a2c1b7d4e|16:00|Zwemles\n2026-10-09~|-|Leeg");
    assert(met_id.size() == 2 && met_id[0].id == "3f9a2c1b7d4e" && met_id[0].datum == (Datum{2026, 10, 8}));
    assert(met_id[0].tijd == "16:00" && met_id[0].titel == "Zwemles" && met_id[1].id.empty());
    open_details(s, 0);
    assert(s.status == DETAILS && !s.details.geladen && !s.details.van_dag && s.details.begin == "16:00");
    assert(details_wanneer(s.details) == "Donderdag 8 oktober  ·  16:00");
    zet_details(s, "ja", "Zwemles", "2026-10-08", "16:00", "2026-10-08", "16:45", "Zwembad", "", "Gezin");
    assert(s.details.geladen && s.details.locatie == "Zwembad");
    assert(details_wanneer(s.details) == "Donderdag 8 oktober  ·  16:00 – 16:45");
    zet_details(s, "nee", "", "", "", "", "", "", "", "");  // tweede antwoord: genegeerd
    assert(s.details.fout.empty());
    Details d;
    d.datum = "2026-10-09";
    d.eind_datum = "2026-10-11";
    assert(details_wanneer(d) == "Vrijdag 9 oktober t/m zondag 11 oktober  ·  hele dag");
    d.begin = "18:00";
    d.eind = "12:00";
    assert(details_wanneer(d) == "Vrijdag 9 oktober 18:00 – zondag 11 oktober 12:00");
    // Niet gevonden
    s.status = DAG;
    open_details(s, 1);
    assert(s.details.van_dag && met_terug(s));
    zet_details(s, "nee", "", "", "", "", "", "", "", "");
    assert(s.details.geladen && !s.details.fout.empty());
    // Antwoord na sluiten: genegeerd
    s.status = RUST;
    zet_details(s, "ja", "X", "2026-10-08", "", "", "", "", "", "");
    assert(s.status == RUST);
  }

  // Aanraken van afspraken: in het vak, of tot 20 pixels ernaast in dezelfde kolom
  {
    Staat s;
    vakken() = {{100, 200, 240, 60, AFSPRAAK0 + 0}, {100, 272, 240, 60, AFSPRAAK0 + 1}};
    assert(raak(s, 150, 230) == AFSPRAAK0);
    assert(raak(s, 150, 300) == AFSPRAAK0 + 1);
    assert(raak(s, 150, 345) == AFSPRAAK0 + 1);  // 14 px eronder
    assert(raak(s, 150, 400) == GEEN);           // te ver
    assert(raak(s, 400, 230) == GEEN);           // andere kolom
    s.status = MELDING;
    assert(raak(s, 150, 230) != AFSPRAAK0);  // onder een venster: niet
    vakken().clear();
  }

  // Spaarstand: slaapduur (elke 30 min, 's nachts 23-6 door tot 6:00)
  assert(slaapduur_ms(14, 0, 30, 23, 6) == 30u * 60 * 1000);
  assert(slaapduur_ms(23, 0, 30, 23, 6) == 7u * 60 * 60 * 1000);
  assert(slaapduur_ms(2, 30, 30, 23, 6) == 210u * 60 * 1000);
  assert(slaapduur_ms(6, 0, 30, 23, 6) == 30u * 60 * 1000);
  assert(slaapduur_ms(-1, 0, 30, 23, 6) == 30u * 60 * 1000);  // tijd onbekend
  assert(slaapduur_ms(14, 0, 120, 23, 6) == 120u * 60 * 1000);  // standaard: elke 2 uur
  assert(slaapduur_ms(22, 30, 120, 23, 6) == 120u * 60 * 1000); // wordt 0:30 wakker, slaapt dan door tot 6:00
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
  b.ruw_weer = a.ruw_weer;
  a.notities = b.notities = lees_notities("todo.a|u1|Melk");
  assert(inhoud_hash(a) == inhoud_hash(b));
  b.notities[0].afgevinkt = true;
  assert(inhoud_hash(a) != inhoud_hash(b));

  // Sortering: per dag hele dag eerst, dan op tijd (ongeacht de volgorde van HA)
  {
    auto l = lees_afspraken("2026-10-09~a|08:00|C\n2026-10-08~z|19:00|B\n2026-10-08~b|08:00|A\n2026-10-08~c|-|Hele dag");
    assert(l.size() == 4 && l[0].titel == "Hele dag" && l[1].titel == "A" && l[2].titel == "B" && l[3].titel == "C");
  }

  // Alleen het verschil verversen: veranderde dagkolom en/of notities
  {
    Staat w;
    w.vandaag = {2026, 10, 8};  // donderdag
    w.afspraken = lees_afspraken("2026-10-06|10:00|Tandarts\n2026-10-08|16:00|Zwemles");
    const auto voor = week_hashes(w);
    ververs_mask() = 0;
    ververs_gebied() = {};
    ververs_verschil(w, w.vandaag, voor, false);
    assert(ververs_mask() == 0);  // niets veranderd: niets tekenen
    w.afspraken = lees_afspraken("2026-10-06|10:00|Tandarts\n2026-10-08|16:00|Zwemles\n2026-10-08|19:00|Eten");
    ververs_verschil(w, w.vandaag, voor, false);
    assert(ververs_mask() == GEBIED);
    const Gebied g = ververs_gebied();
    assert(g.x0 == MARGE + 3 * WEEK_KOL - 5 && g.x1 == g.x0 + WEEK_KOL);  // alleen donderdag
    assert(g.y0 == WEEK_Y && g.y1 == WEEK_EIND);
    ververs_verschil(w, w.vandaag, voor, true);  // en de notities erbij
    assert(ververs_gebied().x0 == 0 && ververs_gebied().y1 == NOTITIE_EIND + 2);
    ververs_mask() = 0;
    ververs_verschil(w, {2026, 10, 7}, voor, false);  // nieuwe dag: alles
    assert(ververs_mask() & VOL);
    ververs_mask() = 0;
    ververs_gebied() = {};
  }

  // Week/maand wisselen: alleen tussen kop en knoppenbalk
  ververs_inhoud();
  assert(ververs_mask() == GEBIED && ververs_gebied().y0 == KOP_H + 3 && ververs_gebied().y1 <= BALK_Y);
  ververs_mask() = 0;
  ververs_gebied() = {};

  // UTF-8
  const std::string s = "café";
  assert(utf8_terug(s, s.size()) == 3);

  std::puts("alle tests geslaagd");
  return 0;
}
