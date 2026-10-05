#pragma once
// Ink – gezinskalender voor de Seeed reTerminal E1003.
//
// Alle layout en logica die niet aan ESPHome-componenten hangt, zodat
// ink-kalender.yaml klein blijft en dit bestand ook op de pc te testen en
// te previewen is (zie tools/preview).
//
// Scherm liggend: 1872 x 1404. Twee schermen, te wisselen met de tabs
// bovenin: de week (met notities eronder) en de maand. Onderaan altijd de
// knoppenbalk.

#ifdef INK_HOST
#include "esphome_stub.h"
#else
#include "esphome.h"
#include <ctime>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_attr.h>
#include <esp_sleep.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace ink {

using esphome::Color;
using esphome::display::BaseFont;
using esphome::display::Display;
using esphome::display::TextAlign;

// ---------------------------------------------------------------- maten ---

constexpr int B = 1872;  // breedte (liggend)
constexpr int H = 1404;  // hoogte

constexpr int MARGE = 26;
constexpr int BREED = B - 2 * MARGE;

constexpr int KOP_H = 96;

// Tabs "Week" / "Maand" in de kop
constexpr int TAB_Y = 18;
constexpr int TAB_H = 62;
constexpr int TAB_B = 190;
constexpr int TAB_X = 760;

// Weekscherm: week over de volle breedte, notities eronder
constexpr int WEEK_Y = 110;
constexpr int WEEK_KOL = BREED / 7;
constexpr int WEEK_KOP_H = 76;
constexpr int WEEK_EIND = 944;
constexpr int NOTITIE_Y = 958;
constexpr int NOTITIE_EIND = 1240;

// Maandscherm: volledig rooster
constexpr int MAAND_KOL = BREED / 7;
constexpr int GRID_Y = 116;
constexpr int GRID_EIND = 1240;

// Knoppenbalk
constexpr int BALK_Y = 1256;
constexpr int KNOP_Y = BALK_Y + 14;
constexpr int KNOP_H = 120;
constexpr int SPREEK_B = 620;
constexpr int KNOP_TUSSEN = 16;
constexpr int HA_KNOP_B = (BREED - SPREEK_B - 4 * KNOP_TUSSEN) / 4;

// Pop-upvenster, midden op het scherm
constexpr int VENSTER_B = 1120;
constexpr int VENSTER_H = 480;
constexpr int VENSTER_X = (B - VENSTER_B) / 2;
constexpr int VENSTER_Y = 360;
constexpr int VENSTER_KNOP_B = 440;
constexpr int VENSTER_KNOP_H = 104;
constexpr int VENSTER_KNOP_Y = VENSTER_Y + VENSTER_H - 144;

// Wat er opnieuw getekend moet worden (bitmasker).
constexpr uint8_t VOL = 1;      // alles, GC16 (mooi grijs, knippert)
constexpr uint8_t BALK = 2;     // alleen knoppenbalk, DU (snel)
constexpr uint8_t VENSTER = 4;  // alleen pop-upvenster, DU (snel)
constexpr uint8_t NOTITIES = 8; // alleen notitievakken, DU (snel)

const Color ZWART(0, 0, 0);
const Color WIT(255, 255, 255);
const Color GRIJS_LICHT(225, 225, 225);
const Color GRIJS(170, 170, 170);
const Color GRIJS_DONKER(100, 100, 100);

// ---------------------------------------------------------------- datum ---

struct Datum {
  int j{2000}, m{1}, d{1};
};

inline bool operator==(const Datum &a, const Datum &b) { return a.j == b.j && a.m == b.m && a.d == b.d; }

// Dagen sinds 1970-01-01 (algoritme van Howard Hinnant).
inline int dagnummer(const Datum &dt) {
  const int y = dt.j - (dt.m <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (dt.m + (dt.m > 2 ? -3 : 9)) + 2) / 5 + dt.d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int>(doe) - 719468;
}

inline Datum van_dagnummer(int z) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  const int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  const int y = static_cast<int>(yoe) + era * 400 + (m <= 2);
  return {y, m, d};
}

inline Datum plus_dagen(const Datum &dt, int n) { return van_dagnummer(dagnummer(dt) + n); }

// 0 = maandag ... 6 = zondag
inline int weekdag(const Datum &dt) { return ((dagnummer(dt) % 7) + 7 + 3) % 7; }

inline int iso_week(const Datum &dt) {
  const int donderdag = dagnummer(dt) - weekdag(dt) + 3;
  const int jan1 = dagnummer({van_dagnummer(donderdag).j, 1, 1});
  return (donderdag - jan1) / 7 + 1;
}

// "JJJJ-MM-DD" -> Datum
inline bool lees_datum(const std::string &s, Datum &uit) {
  int j, m, d;
  if (s.size() < 10 || std::sscanf(s.c_str(), "%4d-%2d-%2d", &j, &m, &d) != 3)
    return false;
  if (m < 1 || m > 12 || d < 1 || d > 31)
    return false;
  uit = {j, m, d};
  return true;
}

static const char *const DAG_KORT[] = {"Ma", "Di", "Wo", "Do", "Vr", "Za", "Zo"};
static const char *const DAG_LANG[] = {"Maandag", "Dinsdag", "Woensdag", "Donderdag",
                                       "Vrijdag", "Zaterdag", "Zondag"};
static const char *const MAAND[] = {"januari", "februari", "maart",     "april",   "mei",      "juni",
                                    "juli",    "augustus", "september", "oktober", "november", "december"};
static const char *const MAAND_KORT[] = {"jan", "feb", "mrt", "apr", "mei", "jun",
                                         "jul", "aug", "sep", "okt", "nov", "dec"};

inline std::string datum_lang(const Datum &dt) {
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%s %d %s", DAG_LANG[weekdag(dt)], dt.d, MAAND[dt.m - 1]);
  return buf;
}

// --------------------------------------------------------------- gegevens ---

struct Afspraak {
  Datum datum;
  std::string tijd;  // "HH:MM", leeg = hele dag
  std::string titel;
};

// Regels "JJJJ-MM-DD|HH:MM|titel" of "JJJJ-MM-DD|-|titel" (hele dag),
// gescheiden door '\n'. Zo stuurt de Home Assistant-blueprint ze.
inline std::vector<Afspraak> lees_afspraken(const std::string &tekst) {
  std::vector<Afspraak> uit;
  size_t pos = 0;
  while (pos < tekst.size()) {
    size_t eind = tekst.find('\n', pos);
    if (eind == std::string::npos)
      eind = tekst.size();
    const std::string regel = tekst.substr(pos, eind - pos);
    pos = eind + 1;

    const size_t a = regel.find('|');
    const size_t b = a == std::string::npos ? a : regel.find('|', a + 1);
    if (b == std::string::npos)
      continue;
    Afspraak af;
    if (!lees_datum(regel.substr(0, a), af.datum))
      continue;
    af.tijd = regel.substr(a + 1, b - a - 1);
    if (af.tijd == "-")
      af.tijd.clear();
    af.titel = regel.substr(b + 1);
    if (!af.titel.empty() && af.titel.back() == '\r')
      af.titel.pop_back();
    uit.push_back(af);
  }
  return uit;
}

inline std::vector<std::string> lees_regels(const std::string &tekst) {
  std::vector<std::string> uit;
  size_t pos = 0;
  while (pos < tekst.size()) {
    size_t eind = tekst.find('\n', pos);
    if (eind == std::string::npos)
      eind = tekst.size();
    std::string regel = tekst.substr(pos, eind - pos);
    pos = eind + 1;
    if (!regel.empty() && regel.back() == '\r')
      regel.pop_back();
    if (!regel.empty())
      uit.push_back(regel);
  }
  return uit;
}

struct Notitie {
  std::string lijst;  // todo-entiteit, bijv. "todo.notities"
  std::string uid;    // id van het item in die lijst
  std::string tekst;
  bool afgevinkt{false};
};

// Regels "todo.lijst|uid|tekst". Een regel zonder '|' is alleen tekst
// (dan kan hij niet afgevinkt worden).
inline std::vector<Notitie> lees_notities(const std::string &tekst) {
  std::vector<Notitie> uit;
  for (const auto &regel : lees_regels(tekst)) {
    Notitie n;
    const size_t a = regel.find('|');
    const size_t b = a == std::string::npos ? a : regel.find('|', a + 1);
    if (b == std::string::npos) {
      n.tekst = regel;
    } else {
      n.lijst = regel.substr(0, a);
      n.uid = regel.substr(a + 1, b - a - 1);
      n.tekst = regel.substr(b + 1);
    }
    if (!n.tekst.empty())
      uit.push_back(n);
  }
  return uit;
}

// ------------------------------------------------------------------ staat ---

enum Status : uint8_t { RUST, LUISTEREN, VERWERKEN, VOORSTEL, MELDING };

enum Zone : int {
  GEEN = -1,
  SPREEK = 0,
  KNOP1 = 1,  // KNOP1..KNOP4 = Home Assistant-knoppen
  OPSLAAN = 10,
  ANNULEER = 11,
  OK = 12,
  TAB_WEEK = 20,
  TAB_MAAND = 21,
  NOTITIE0 = 100,  // NOTITIE0 + i = notitie i
};

enum Scherm : uint8_t { SCHERM_WEEK, SCHERM_MAAND };

// Spaarstand: WAKKER = normaal; SLAAPT = balk toont "Slaapstand" (ook tijdens
// een stille wekker-ronde); WORDT_WAKKER = gebruiker heeft gewekt, wacht op HA.
enum Slaap : uint8_t { WAKKER, SLAAPT, WORDT_WAKKER };

struct Voorstel {
  std::string titel, datum, begin, eind, gehoord;
  std::string soort;  // "afspraak" of "notitie"
};

struct HaKnop {
  std::string naam;
  bool aan{false};
};

struct Staat {
  Datum vandaag;
  bool tijd_geldig{false};
  bool data_ontvangen{false};
  bool verbonden{true};
  std::string ruw_afspraken, ruw_notities;
  std::vector<Afspraak> afspraken;
  std::vector<Notitie> notities;
  Status status{RUST};
  Scherm scherm{SCHERM_WEEK};
  Voorstel voorstel;
  std::string melding;
  HaKnop knoppen[4];
  float temperatuur{NAN};
  float vochtigheid{NAN};
  float accu{NAN};  // procent
  Slaap slaap{WAKKER};
};

// Onder dit percentage waarschuwt het scherm om op te laden.
constexpr int ACCU_LAAG = 15;

inline Staat &staat() {
  static Staat s;
  return s;
}

inline uint8_t &ververs_mask() {
  static uint8_t m = 0;
  return m;
}

inline bool venster_open(const Staat &s) { return s.status == VOORSTEL || s.status == MELDING; }

struct Fonts {
  BaseFont *klein;    // maandvakjes
  BaseFont *normaal;  // afspraken
  BaseFont *vet;      // tijden, knoppen
  BaseFont *kop;      // dagkoppen
  BaseFont *groot;    // datum bovenaan
  // Voor lettertypes zonder vette variant (zoals Patrick Hand): vet, kop en
  // groot twee keer tekenen, 1 pixel verschoven.
  bool nep_vet{false};
};

// Fonts die als 'nep-vet' getekend worden; gezet door teken().
inline BaseFont *(&nep_vette_fonts())[3] {
  static BaseFont *f[3] = {nullptr, nullptr, nullptr};
  return f;
}

// ------------------------------------------------------------ tekst-hulp ---

inline int breedte(Display &it, BaseFont *f, const std::string &s) {
  if (s.empty())
    return 0;
  int x1, y1, w, h;
  it.get_text_bounds(0, 0, s.c_str(), f, TextAlign::TOP_LEFT, &x1, &y1, &w, &h);
  return w;
}

// Begin van het laatste UTF-8-teken vóór positie i.
inline size_t utf8_terug(const std::string &s, size_t i) {
  if (i == 0)
    return 0;
  i--;
  while (i > 0 && (static_cast<uint8_t>(s[i]) & 0xC0) == 0x80)
    i--;
  return i;
}

inline std::string afkappen(Display &it, BaseFont *f, const std::string &s, int max) {
  if (breedte(it, f, s) <= max)
    return s;
  std::string r = s;
  while (!r.empty()) {
    r.erase(utf8_terug(r, r.size()));
    while (!r.empty() && r.back() == ' ')
      r.pop_back();
    if (breedte(it, f, r + "…") <= max)
      return r + "…";
  }
  return "";
}

// Woordterugloop. De eerste regel mag een andere breedte hebben (bijv. als er
// een tijd voor staat). Wat niet in max_regels past, wordt afgekapt met '…'.
inline std::vector<std::string> omloop(Display &it, BaseFont *f, const std::string &s, int eerste_max, int max,
                                       int max_regels) {
  std::vector<std::string> regels;
  std::string huidig;
  size_t pos = 0;
  while (pos <= s.size()) {
    size_t spatie = s.find(' ', pos);
    if (spatie == std::string::npos)
      spatie = s.size();
    const std::string woord = s.substr(pos, spatie - pos);
    pos = spatie + 1;
    if (woord.empty())
      continue;
    const int grens = regels.empty() ? eerste_max : max;
    const std::string kandidaat = huidig.empty() ? woord : huidig + " " + woord;
    if (huidig.empty() && regels.empty() && eerste_max < max && breedte(it, f, woord) > eerste_max) {
      // Eerste woord past niet naast de tijd: begin op de volgende regel.
      regels.push_back("");
      huidig = woord;
    } else if (huidig.empty() || breedte(it, f, kandidaat) <= grens) {
      huidig = kandidaat;
    } else {
      regels.push_back(huidig);
      huidig = woord;
    }
  }
  if (!huidig.empty())
    regels.push_back(huidig);

  if (static_cast<int>(regels.size()) > max_regels) {
    std::string rest;
    for (size_t i = max_regels - 1; i < regels.size(); i++)
      rest += (rest.empty() ? "" : " ") + regels[i];
    regels.resize(max_regels);
    regels.back() = rest + " …";  // forceer afkapteken
  }
  for (size_t i = 0; i < regels.size(); i++)
    regels[i] = afkappen(it, f, regels[i], i == 0 ? eerste_max : max);
  return regels;
}

inline void tekst(Display &it, int x, int y, BaseFont *f, Color kleur, Color achter, TextAlign uitlijning,
                  const std::string &s) {
  it.print(x, y, f, kleur, uitlijning, s.c_str(), achter);
  for (BaseFont *v : nep_vette_fonts())
    if (v == f)
      it.print(x + 1, y, f, kleur, uitlijning, s.c_str(), achter);
}

inline void kader(Display &it, int x, int y, int w, int h, int dikte, Color kleur) {
  for (int i = 0; i < dikte; i++)
    it.rectangle(x + i, y + i, w - 2 * i, h - 2 * i, kleur);
}

// --------------------------------------------------------------- tekenen ---

inline int tab_x(int i) { return TAB_X + i * TAB_B; }

inline void teken_kop(Display &it, const Staat &s, const Fonts &f) {
  // Met een waarschuwing ernaast de korte datum, anders past het niet
  const bool waarschuwing_zichtbaar = !s.verbonden || !s.data_ontvangen;
  std::string datum = "Ink kalender";
  if (s.tijd_geldig && waarschuwing_zichtbaar) {
    char kort[24];
    std::snprintf(kort, sizeof(kort), "%s %d %s", DAG_KORT[weekdag(s.vandaag)], s.vandaag.d,
                  MAAND_KORT[s.vandaag.m - 1]);
    datum = kort;
  } else if (s.tijd_geldig) {
    datum = datum_lang(s.vandaag);
  }
  tekst(it, MARGE, 4, f.groot, ZWART, WIT, TextAlign::TOP_LEFT, datum);

  // Tabs
  static const char *const TABS[] = {"Week", "Maand"};
  for (int i = 0; i < 2; i++) {
    const bool actief = s.scherm == i;
    const Color a = actief ? ZWART : WIT;
    it.filled_rectangle(tab_x(i), TAB_Y, TAB_B, TAB_H, a);
    kader(it, tab_x(i), TAB_Y, TAB_B, TAB_H, 3, ZWART);
    tekst(it, tab_x(i) + TAB_B / 2, TAB_Y + 4, f.kop, actief ? WIT : ZWART, a, TextAlign::TOP_CENTER, TABS[i]);
  }

  // Rechts, van rechts naar links: accu, temperatuur, weeknummer
  char buf[48];
  int xr = B - MARGE;
  if (!std::isnan(s.accu)) {
    const int pct = static_cast<int>(std::lround(s.accu));
    const bool laag = pct <= ACCU_LAAG;
    std::snprintf(buf, sizeof(buf), "%d%%", pct);
    tekst(it, xr, 28, laag ? f.vet : f.normaal, laag ? ZWART : GRIJS_DONKER, WIT, TextAlign::TOP_RIGHT, buf);
    xr -= breedte(it, laag ? f.vet : f.normaal, buf) + 10;
    // Accu-icoontje: 46x24 met knopje rechts, vulling naar verhouding
    const int ix = xr - 52, iy = 36;
    const Color rand = laag ? ZWART : GRIJS_DONKER;
    kader(it, ix, iy, 46, 24, 2, rand);
    it.filled_rectangle(ix + 46, iy + 7, 4, 10, rand);
    const int vul = std::max(0, std::min(40, pct * 40 / 100));
    if (vul > 0)
      it.filled_rectangle(ix + 3, iy + 3, vul, 18, rand);
    xr = ix - 36;
    if (laag) {
      tekst(it, xr, 28, f.vet, ZWART, WIT, TextAlign::TOP_RIGHT, "opladen!");
      xr -= breedte(it, f.vet, "opladen!") + 36;
    }
  }
  if (!std::isnan(s.temperatuur)) {
    if (!std::isnan(s.vochtigheid))
      std::snprintf(buf, sizeof(buf), "%.1f °C  ·  %.0f%%", s.temperatuur, s.vochtigheid);
    else
      std::snprintf(buf, sizeof(buf), "%.1f °C", s.temperatuur);
    std::string t = buf;
    for (auto &c : t)
      if (c == '.')
        c = ',';
    tekst(it, xr, 28, f.normaal, GRIJS_DONKER, WIT, TextAlign::TOP_RIGHT, t);
    xr -= breedte(it, f.normaal, t) + 36;
  }
  if (s.tijd_geldig) {
    std::snprintf(buf, sizeof(buf), "Week %d", iso_week(s.vandaag));
    // Alleen als het naast de tabs past
    if (xr - breedte(it, f.kop, buf) > tab_x(2) + 20)
      tekst(it, xr, 20, f.kop, ZWART, WIT, TextAlign::TOP_RIGHT, buf);
  }
  if (waarschuwing_zichtbaar) {
    const std::string waarschuwing = !s.verbonden ? "Geen verbinding" : "Wacht op agenda…";
    const int w = breedte(it, f.vet, waarschuwing) + 28;
    const int x = TAB_X - w - 20;
    it.filled_rectangle(x, TAB_Y + 4, w, TAB_H - 8, ZWART);
    tekst(it, x + 14, TAB_Y + 10, f.vet, WIT, ZWART, TextAlign::TOP_LEFT, waarschuwing);
  }
  it.filled_rectangle(MARGE, KOP_H, BREED, 3, ZWART);
}

// Hoogte die een afspraak in de weekkolom inneemt (en tekent als teken=true).
inline int week_afspraak(Display &it, const Fonts &f, const Afspraak &a, int x, int y, int w, bool teken) {
  constexpr int REGEL = 30;
  if (a.tijd.empty()) {
    auto regels = omloop(it, f.normaal, a.titel, w - 16, w - 16, 2);
    const int h = static_cast<int>(regels.size()) * REGEL + 10;
    if (teken) {
      it.filled_rectangle(x, y, w, h, GRIJS_LICHT);
      for (size_t i = 0; i < regels.size(); i++)
        tekst(it, x + 8, y + 1 + i * REGEL, f.normaal, ZWART, GRIJS_LICHT, TextAlign::TOP_LEFT, regels[i]);
    }
    return h;
  }
  const int tijd_b = breedte(it, f.vet, a.tijd) + 10;
  auto regels = omloop(it, f.normaal, a.titel, w - tijd_b, w, 3);
  const int h = static_cast<int>(regels.size()) * REGEL;
  if (teken) {
    tekst(it, x, y - 4, f.vet, ZWART, WIT, TextAlign::TOP_LEFT, a.tijd);
    for (size_t i = 0; i < regels.size(); i++)
      tekst(it, x + (i == 0 ? tijd_b : 0), y - 4 + i * REGEL, f.normaal, ZWART, WIT, TextAlign::TOP_LEFT,
            regels[i]);
  }
  return h;
}

inline void teken_week(Display &it, const Staat &s, const Fonts &f) {
  const Datum maandag = plus_dagen(s.vandaag, -weekdag(s.vandaag));
  for (int i = 0; i < 7; i++) {
    const Datum dag = plus_dagen(maandag, i);
    const bool is_vandaag = s.tijd_geldig && dag == s.vandaag;
    const int x = MARGE + i * WEEK_KOL;
    const int w = WEEK_KOL - 10;

    const Color kop_achter = is_vandaag ? ZWART : GRIJS_LICHT;
    const Color kop_tekst = is_vandaag ? WIT : ZWART;
    it.filled_rectangle(x, WEEK_Y, w, WEEK_KOP_H, kop_achter);
    // Dagnaam groot, datum klein eronder
    tekst(it, x + 14, WEEK_Y - 2, f.kop, kop_tekst, kop_achter, TextAlign::TOP_LEFT, DAG_LANG[i]);
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%d %s", dag.d, MAAND[dag.m - 1]);
    tekst(it, x + 14, WEEK_Y + 40, f.klein, kop_tekst, kop_achter, TextAlign::TOP_LEFT, buf);

    if (i > 0)
      it.vertical_line(x - 5, WEEK_Y + WEEK_KOP_H + 8, WEEK_EIND - WEEK_Y - WEEK_KOP_H - 8, GRIJS);

    // Hele-dag-afspraken eerst (staan al vooraan door sortering in HA).
    std::vector<const Afspraak *> lijst;
    for (const auto &a : s.afspraken)
      if (a.datum == dag)
        lijst.push_back(&a);

    int y = WEEK_Y + WEEK_KOP_H + 12;
    const int bx = x + 6, bw = w - 10;
    for (size_t n = 0; n < lijst.size(); n++) {
      const int h = week_afspraak(it, f, *lijst[n], bx, y, bw, false);
      const bool laatste = n + 1 == lijst.size();
      if (y + h > WEEK_EIND - (laatste ? 0 : 34)) {
        char meer[16];
        std::snprintf(meer, sizeof(meer), "+%d meer", static_cast<int>(lijst.size() - n));
        tekst(it, bx, y - 4, f.vet, GRIJS_DONKER, WIT, TextAlign::TOP_LEFT, meer);
        break;
      }
      week_afspraak(it, f, *lijst[n], bx, y, bw, true);
      y += h + 12;
    }
  }
}

inline void teken_maand(Display &it, const Staat &s, const Fonts &f) {
  const Datum eerste{s.vandaag.j, s.vandaag.m, 1};
  const Datum start = plus_dagen(eerste, -weekdag(eerste));
  const Datum volgende{s.vandaag.m == 12 ? s.vandaag.j + 1 : s.vandaag.j, s.vandaag.m == 12 ? 1 : s.vandaag.m + 1, 1};
  const int dagen = dagnummer(volgende) - dagnummer(eerste);
  const int rijen = (weekdag(eerste) + dagen + 6) / 7;

  char buf[40];
  constexpr int DAGKOP_H = 40;
  for (int i = 0; i < 7; i++)
    tekst(it, MARGE + i * MAAND_KOL + 12, GRID_Y, f.vet, GRIJS_DONKER, WIT, TextAlign::TOP_LEFT, DAG_LANG[i]);

  const int top = GRID_Y + DAGKOP_H;
  const int rij_h = (GRID_EIND - top) / rijen;
  const int links = MARGE, rechts = MARGE + 7 * MAAND_KOL;
  for (int r = 0; r <= rijen; r++)
    it.horizontal_line(links, top + r * rij_h, rechts - links, r == 0 || r == rijen ? ZWART : GRIJS);
  for (int c = 0; c <= 7; c++)
    it.vertical_line(links + c * MAAND_KOL, top, rij_h * rijen, c == 0 || c == 7 ? ZWART : GRIJS);

  constexpr int REGEL = 34;
  for (int n = 0; n < rijen * 7; n++) {
    const Datum dag = plus_dagen(start, n);
    const int x = links + (n % 7) * MAAND_KOL;
    const int y = top + (n / 7) * rij_h;
    const bool deze_maand = dag.m == s.vandaag.m;
    const bool is_vandaag = s.tijd_geldig && dag == s.vandaag;

    std::snprintf(buf, sizeof(buf), "%d", dag.d);
    if (deze_maand && dag.d == 1) {
      std::snprintf(buf, sizeof(buf), "1 %s", MAAND_KORT[dag.m - 1]);
    } else if (!deze_maand) {
      std::snprintf(buf, sizeof(buf), "%d %s", dag.d, MAAND_KORT[dag.m - 1]);
    } else {
      std::snprintf(buf, sizeof(buf), "%d", dag.d);
    }
    if (is_vandaag) {
      const int nb = breedte(it, f.kop, buf) + 20;
      it.filled_rectangle(x + 1, y + 1, nb, 40, ZWART);
      tekst(it, x + 10, y + 1, f.kop, WIT, ZWART, TextAlign::TOP_LEFT, buf);
    } else {
      tekst(it, x + 10, y + 1, f.kop, deze_maand ? ZWART : GRIJS, WIT, TextAlign::TOP_LEFT, buf);
    }

    const Color kleur = deze_maand ? ZWART : GRIJS_DONKER;
    const int max_regels = (rij_h - 44) / REGEL;
    std::vector<const Afspraak *> lijst;
    for (const auto &a : s.afspraken)
      if (a.datum == dag)
        lijst.push_back(&a);
    int ty = y + 38;
    for (size_t i = 0; i < lijst.size() && static_cast<int>(i) < max_regels; i++) {
      const bool laatste_plek = static_cast<int>(i) == max_regels - 1;
      if (laatste_plek && lijst.size() > i + 1) {
        std::snprintf(buf, sizeof(buf), "+%d meer", static_cast<int>(lijst.size() - i));
        tekst(it, x + 10, ty, f.normaal, GRIJS_DONKER, WIT, TextAlign::TOP_LEFT, buf);
        break;
      }
      const Afspraak &a = *lijst[i];
      const std::string regel = a.tijd.empty() ? a.titel : a.tijd + " " + a.titel;
      if (a.tijd.empty()) {
        it.filled_rectangle(x + 4, ty + 8, MAAND_KOL - 8, REGEL - 2, GRIJS_LICHT);
        tekst(it, x + 10, ty, f.normaal, kleur, GRIJS_LICHT, TextAlign::TOP_LEFT,
              afkappen(it, f.normaal, regel, MAAND_KOL - 22));
      } else {
        tekst(it, x + 10, ty, f.normaal, kleur, WIT, TextAlign::TOP_LEFT,
              afkappen(it, f.normaal, regel, MAAND_KOL - 22));
      }
      ty += REGEL;
    }
  }
}

constexpr int NOTITIE_REGEL = 40;
constexpr int NOTITIE_KOLOMMEN = 3;
constexpr int NOTITIE_TUSSEN = 30;
constexpr int NOTITIE_TOP = NOTITIE_Y + 42;
constexpr int NOTITIE_KOL_B = (BREED - (NOTITIE_KOLOMMEN - 1) * NOTITIE_TUSSEN) / NOTITIE_KOLOMMEN;
constexpr int NOTITIE_PER_KOLOM = (NOTITIE_EIND - NOTITIE_TOP) / NOTITIE_REGEL;
constexpr int NOTITIE_MAX = NOTITIE_PER_KOLOM * NOTITIE_KOLOMMEN;

inline int notitie_x(int i) { return MARGE + (i / NOTITIE_PER_KOLOM) * (NOTITIE_KOL_B + NOTITIE_TUSSEN); }
inline int notitie_y(int i) { return NOTITIE_TOP + (i % NOTITIE_PER_KOLOM) * NOTITIE_REGEL; }

inline void teken_notities(Display &it, const Staat &s, const Fonts &f) {
  tekst(it, MARGE, NOTITIE_Y - 8, f.kop, ZWART, WIT, TextAlign::TOP_LEFT, "Notities");
  tekst(it, MARGE + breedte(it, f.kop, "Notities") + 24, NOTITIE_Y + 2, f.klein, GRIJS_DONKER, WIT,
        TextAlign::TOP_LEFT, "tik op een notitie om af te vinken");
  for (int k = 0; k < NOTITIE_KOLOMMEN; k++) {
    const int x = MARGE + k * (NOTITIE_KOL_B + NOTITIE_TUSSEN);
    it.filled_rectangle(x, NOTITIE_TOP, NOTITIE_KOL_B, 2, ZWART);
    for (int r = 1; r <= NOTITIE_PER_KOLOM; r++)
      it.horizontal_line(x, NOTITIE_TOP + r * NOTITIE_REGEL, NOTITIE_KOL_B, GRIJS);
  }
  if (s.notities.empty()) {
    tekst(it, MARGE + 8, NOTITIE_TOP + 2, f.normaal, GRIJS_DONKER, WIT, TextAlign::TOP_LEFT, "Geen notities");
    return;
  }
  const int aantal = static_cast<int>(s.notities.size());
  for (int i = 0; i < aantal && i < NOTITIE_MAX; i++) {
    const int x = notitie_x(i);
    const int y = notitie_y(i);
    if (i == NOTITIE_MAX - 1 && aantal > NOTITIE_MAX) {
      char buf[24];
      std::snprintf(buf, sizeof(buf), "+%d meer", aantal - i);
      tekst(it, x + 36, y + 2, f.normaal, GRIJS_DONKER, WIT, TextAlign::TOP_LEFT, buf);
      break;
    }
    const Notitie &n = s.notities[i];
    // Vakje om af te vinken
    kader(it, x + 4, y + 10, 22, 22, 2, n.uid.empty() ? GRIJS : ZWART);
    if (n.afgevinkt) {
      for (int d = -1; d <= 1; d++) {
        it.line(x + 8, y + 20 + d, x + 14, y + 27 + d, ZWART);
        it.line(x + 14, y + 27 + d, x + 30, y + 6 + d, ZWART);
      }
    }
    const std::string t = afkappen(it, f.normaal, n.tekst, NOTITIE_KOL_B - 44);
    tekst(it, x + 36, y + 2, f.normaal, n.afgevinkt ? GRIJS_DONKER : ZWART, WIT, TextAlign::TOP_LEFT, t);
    if (n.afgevinkt)
      it.filled_rectangle(x + 34, y + 22, breedte(it, f.normaal, t) + 4, 2, GRIJS_DONKER);
  }
}

inline int ha_knop_x(int i) { return MARGE + SPREEK_B + KNOP_TUSSEN + i * (HA_KNOP_B + KNOP_TUSSEN); }

inline void teken_microfoon(Display &it, int mx, int my, Color voor, Color achter) {
  for (int d = 0; d < 3; d++)
    it.circle(mx, my - 2, 22 + d, voor);
  it.filled_rectangle(mx - 26, my - 30, 52, 28, achter);  // bovenste helft van de boog weg
  it.filled_rectangle(mx - 11, my - 34, 22, 40, voor);
  it.filled_circle(mx, my - 34, 11, voor);
  it.filled_circle(mx, my + 6, 11, voor);
  it.filled_rectangle(mx - 2, my + 20, 4, 18, voor);
  it.filled_rectangle(mx - 14, my + 36, 28, 4, voor);
}

inline void teken_slaapbalk(Display &it, const Staat &s, const Fonts &f) {
  const bool wordt_wakker = s.slaap == WORDT_WAKKER;
  // Alleen zwart/wit: deze balk wordt met de snelle DU-modus getekend.
  const Color a = wordt_wakker ? ZWART : WIT;
  const Color v = wordt_wakker ? WIT : ZWART;
  it.filled_rectangle(MARGE, KNOP_Y, BREED, KNOP_H, a);
  kader(it, MARGE, KNOP_Y, BREED, KNOP_H, 3, ZWART);
  if (wordt_wakker) {
    tekst(it, MARGE + 40, KNOP_Y + 14, f.groot, v, a, TextAlign::TOP_LEFT, "Even wakker worden…");
    tekst(it, B - MARGE - 40, KNOP_Y + 40, f.normaal, v, a, TextAlign::TOP_RIGHT,
          "verbinden met Home Assistant, een paar seconden");
  } else {
    // Maantje
    it.filled_circle(MARGE + 70, KNOP_Y + KNOP_H / 2, 30, v);
    it.filled_circle(MARGE + 84, KNOP_Y + KNOP_H / 2 - 12, 28, a);
    tekst(it, MARGE + 130, KNOP_Y + 14, f.groot, v, a, TextAlign::TOP_LEFT, "Slaapstand");
    tekst(it, B - MARGE - 40, KNOP_Y + 22, f.kop, v, a, TextAlign::TOP_RIGHT, "Tik op het scherm om te wekken");
    tekst(it, B - MARGE - 40, KNOP_Y + 68, f.normaal, v, a, TextAlign::TOP_RIGHT,
          "of druk op een knop · de agenda ververst vanzelf");
  }
}

inline void teken_balk(Display &it, const Staat &s, const Fonts &f) {
  it.filled_rectangle(0, BALK_Y, B, H - BALK_Y, WIT);
  it.horizontal_line(MARGE, BALK_Y, BREED, ZWART);
  if (s.slaap != WAKKER) {
    teken_slaapbalk(it, s, f);
    return;
  }

  // Spreekknop
  std::string label = "Inspreken";
  std::string hint = "een afspraak of een notitie";
  const bool actief = s.status == LUISTEREN || s.status == VERWERKEN;
  if (s.status == LUISTEREN) {
    label = "Ik luister…";
    hint = "tik nogmaals om te stoppen";
  } else if (s.status == VERWERKEN) {
    label = "Even denken…";
    hint = "";
  }
  const Color achter = actief ? ZWART : WIT;
  const Color voor = actief ? WIT : ZWART;
  it.filled_rectangle(MARGE, KNOP_Y, SPREEK_B, KNOP_H, achter);
  kader(it, MARGE, KNOP_Y, SPREEK_B, KNOP_H, 4, ZWART);
  teken_microfoon(it, MARGE + 56, KNOP_Y + KNOP_H / 2, voor, achter);
  tekst(it, MARGE + 108, KNOP_Y + (hint.empty() ? 30 : 12), f.kop, voor, achter, TextAlign::TOP_LEFT, label);
  if (!hint.empty())
    tekst(it, MARGE + 108, KNOP_Y + 66, f.klein, voor, achter, TextAlign::TOP_LEFT,
          afkappen(it, f.klein, hint, SPREEK_B - 124));

  // Home Assistant-knoppen
  for (int i = 0; i < 4; i++) {
    const HaKnop &k = s.knoppen[i];
    if (k.naam.empty())
      continue;
    const int x = ha_knop_x(i);
    const Color a = k.aan ? ZWART : WIT;
    const Color v = k.aan ? WIT : ZWART;
    it.filled_rectangle(x, KNOP_Y, HA_KNOP_B, KNOP_H, a);
    kader(it, x, KNOP_Y, HA_KNOP_B, KNOP_H, 3, ZWART);
    auto regels = omloop(it, f.kop, k.naam, HA_KNOP_B - 24, HA_KNOP_B - 24, 2);
    const int regel_h = 34;
    const int ty = KNOP_Y + (KNOP_H - 30 - static_cast<int>(regels.size()) * regel_h) / 2 - 6;
    for (size_t r = 0; r < regels.size(); r++)
      tekst(it, x + HA_KNOP_B / 2, ty + r * regel_h, f.kop, v, a, TextAlign::TOP_CENTER, regels[r]);
    tekst(it, x + HA_KNOP_B / 2, KNOP_Y + KNOP_H - 36, f.klein, v, a, TextAlign::TOP_CENTER, k.aan ? "aan" : "uit");
  }
}

inline void venster_knop(Display &it, const Fonts &f, int x, const char *label, bool gevuld) {
  const Color a = gevuld ? ZWART : WIT;
  it.filled_rectangle(x, VENSTER_KNOP_Y, VENSTER_KNOP_B, VENSTER_KNOP_H, a);
  kader(it, x, VENSTER_KNOP_Y, VENSTER_KNOP_B, VENSTER_KNOP_H, 4, ZWART);
  tekst(it, x + VENSTER_KNOP_B / 2, VENSTER_KNOP_Y + 26, f.kop, gevuld ? WIT : ZWART, a, TextAlign::TOP_CENTER,
        label);
}

inline int opslaan_x() { return VENSTER_X + 60; }
inline int annuleer_x() { return VENSTER_X + VENSTER_B - 60 - VENSTER_KNOP_B; }
inline int ok_x() { return VENSTER_X + (VENSTER_B - VENSTER_KNOP_B) / 2; }

inline void teken_venster(Display &it, const Staat &s, const Fonts &f) {
  it.filled_rectangle(VENSTER_X, VENSTER_Y, VENSTER_B, VENSTER_H, WIT);
  kader(it, VENSTER_X, VENSTER_Y, VENSTER_B, VENSTER_H, 6, ZWART);
  const int x = VENSTER_X + 60;
  const int w = VENSTER_B - 120;
  int y = VENSTER_Y + 36;

  if (s.status == MELDING) {
    for (const auto &r : omloop(it, f.kop, s.melding, w, w, 6)) {
      tekst(it, x, y, f.kop, ZWART, WIT, TextAlign::TOP_LEFT, r);
      y += 48;
    }
    venster_knop(it, f, ok_x(), "OK", true);
    return;
  }

  const Voorstel &v = s.voorstel;
  const bool notitie = v.soort == "notitie";
  tekst(it, x, y, f.vet, GRIJS_DONKER, WIT, TextAlign::TOP_LEFT,
        notitie ? "Notitie toevoegen?" : "Nieuwe afspraak toevoegen?");
  y += 46;
  for (const auto &r : omloop(it, f.groot, v.titel, w, w, 2)) {
    tekst(it, x, y, f.groot, ZWART, WIT, TextAlign::TOP_LEFT, r);
    y += 66;
  }
  y += 4;
  if (!notitie) {
    Datum d;
    std::string wanneer = lees_datum(v.datum, d) ? datum_lang(d) : v.datum;
    if (v.begin.empty())
      wanneer += "  ·  hele dag";
    else
      wanneer += "  ·  " + v.begin + (v.eind.empty() ? "" : " – " + v.eind);
    tekst(it, x, y, f.kop, ZWART, WIT, TextAlign::TOP_LEFT, afkappen(it, f.kop, wanneer, w));
  } else {
    tekst(it, x, y, f.kop, ZWART, WIT, TextAlign::TOP_LEFT, "op de notitielijst");
  }
  y += 54;
  if (!v.gehoord.empty())
    tekst(it, x, y, f.normaal, GRIJS_DONKER, WIT, TextAlign::TOP_LEFT,
          afkappen(it, f.normaal, "Gehoord: \"" + v.gehoord + "\"", w));

  venster_knop(it, f, opslaan_x(), "Opslaan", true);
  venster_knop(it, f, annuleer_x(), "Annuleer", false);
}

// Hoofdfunctie, aangeroepen vanuit de display-lambda.
inline void teken(Display &it, const Staat &s, const Fonts &f, uint8_t mask) {
  auto &nep = nep_vette_fonts();
  nep[0] = f.nep_vet ? f.vet : nullptr;
  nep[1] = f.nep_vet ? f.kop : nullptr;
  nep[2] = f.nep_vet ? f.groot : nullptr;
  if (mask & VOL) {
    it.fill(WIT);
    teken_kop(it, s, f);
    if (s.scherm == SCHERM_MAAND) {
      teken_maand(it, s, f);
    } else {
      teken_week(it, s, f);
      teken_notities(it, s, f);
    }
    teken_balk(it, s, f);
    if (venster_open(s))
      teken_venster(it, s, f);
    return;
  }
  if (mask & BALK)
    teken_balk(it, s, f);
  if ((mask & NOTITIES) && s.scherm == SCHERM_WEEK) {
    it.filled_rectangle(0, NOTITIE_Y - 12, B, NOTITIE_EIND - NOTITIE_Y + 14, WIT);
    teken_notities(it, s, f);
    // Een open venster mag er niet door overschreven worden.
    if (venster_open(s))
      teken_venster(it, s, f);
  }
  if ((mask & VENSTER) && venster_open(s))
    teken_venster(it, s, f);
}

// ----------------------------------------------------------------- touch ---

inline bool binnen(int x, int y, int bx, int by, int bw, int bh) {
  return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

inline Zone raak(const Staat &s, int x, int y) {
  if (s.slaap != WAKKER)
    return GEEN;  // eerst wakker worden
  if (venster_open(s)) {
    if (s.status == MELDING)
      return binnen(x, y, ok_x(), VENSTER_KNOP_Y, VENSTER_KNOP_B, VENSTER_KNOP_H) ? OK : GEEN;
    if (binnen(x, y, opslaan_x(), VENSTER_KNOP_Y, VENSTER_KNOP_B, VENSTER_KNOP_H))
      return OPSLAAN;
    if (binnen(x, y, annuleer_x(), VENSTER_KNOP_Y, VENSTER_KNOP_B, VENSTER_KNOP_H))
      return ANNULEER;
  }
  // Tabs en notities liggen onder een open venster; dan niet aanraakbaar.
  for (int i = 0; i < 2 && !venster_open(s); i++)
    if (binnen(x, y, tab_x(i), 0, TAB_B, KOP_H))
      return static_cast<Zone>(TAB_WEEK + i);
  if (s.scherm == SCHERM_WEEK && !venster_open(s)) {
    const int aantal = static_cast<int>(s.notities.size());
    for (int i = 0; i < aantal && i < NOTITIE_MAX; i++) {
      if (i == NOTITIE_MAX - 1 && aantal > NOTITIE_MAX)
        break;  // "+N meer"
      if (binnen(x, y, notitie_x(i), notitie_y(i), NOTITIE_KOL_B, NOTITIE_REGEL))
        return s.notities[i].uid.empty() ? GEEN : static_cast<Zone>(NOTITIE0 + i);
    }
  }
  // Iets ruimere vlakken in de balk; vingers zijn geen stylus.
  if (binnen(x, y, MARGE - 10, KNOP_Y - 14, SPREEK_B + 18, KNOP_H + 28))
    return SPREEK;
  for (int i = 0; i < 4; i++)
    if (!s.knoppen[i].naam.empty() &&
        binnen(x, y, ha_knop_x(i) - 8, KNOP_Y - 14, HA_KNOP_B + KNOP_TUSSEN, KNOP_H + 28))
      return static_cast<Zone>(KNOP1 + i);
  return GEEN;
}

// ------------------------------------------------------------ spaarstand ---

// Vingerafdruk van wat er op het scherm staat (zonder de knoppenbalk). Een
// stille wekker-ronde ververst alleen als die verandert.
inline uint32_t inhoud_hash(const Staat &s) {
  uint32_t h = 2166136261u;  // FNV-1a
  auto voeg_toe = [&h](const std::string &t) {
    for (unsigned char c : t)
      h = (h ^ c) * 16777619u;
    h = (h ^ 0xFF) * 16777619u;
  };
  voeg_toe(s.ruw_afspraken);
  voeg_toe(s.ruw_notities);
  char buf[32];
  const int accu_stap = std::isnan(s.accu) ? -2 : (s.accu <= ACCU_LAAG ? -1 : static_cast<int>(s.accu) / 10);
  std::snprintf(buf, sizeof(buf), "%d-%d-%d/%d/%d", s.vandaag.j, s.vandaag.m, s.vandaag.d, accu_stap,
                s.verbonden ? 1 : 0);
  voeg_toe(buf);
  return h;
}

// Slaapduur tot de volgende stille ronde: elke `elke_min` minuten, maar 's nachts
// (van `nacht_van` tot `nacht_tot` uur) in één keer door tot de ochtend.
inline uint32_t slaapduur_ms(int uur, int minuut, int elke_min, int nacht_van, int nacht_tot) {
  const bool nacht = nacht_van > nacht_tot ? (uur >= nacht_van || uur < nacht_tot)
                                           : (uur >= nacht_van && uur < nacht_tot);
  if (uur < 0 || !nacht)
    return static_cast<uint32_t>(elke_min) * 60u * 1000u;
  int tot_ochtend = (nacht_tot * 60) - (uur * 60 + minuut);
  if (tot_ochtend <= 0)
    tot_ochtend += 24 * 60;
  return static_cast<uint32_t>(tot_ochtend) * 60u * 1000u;
}

#ifndef INK_HOST
// Blijft bewaard tijdens deep sleep (RTC-geheugen), niet na stroomverlies.
static RTC_DATA_ATTR uint32_t rtc_inhoud_hash = 0;
static RTC_DATA_ATTR time_t rtc_slaap_begin = 0;
static RTC_DATA_ATTR uint8_t rtc_snelle_touch_wekkers = 0;

enum Wekreden : int { WEK_STROOM = 0, WEK_TIMER = 1, WEK_GEBRUIKER = 2 };

constexpr gpio_num_t PIN_TOUCH_INT = GPIO_NUM_2;
constexpr gpio_num_t PIN_KNOP_GROEN = GPIO_NUM_3;
constexpr gpio_num_t KNOPPEN[] = {GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5};
// Uitgangen die tijdens de slaap laag moeten blijven (zie README, Spaarstand)
constexpr gpio_num_t UIT_IN_SLAAP[] = {GPIO_NUM_11, GPIO_NUM_21, GPIO_NUM_38, GPIO_NUM_39, GPIO_NUM_40, GPIO_NUM_45};
constexpr gpio_num_t PIN_TOUCH_RESET = GPIO_NUM_48;

inline Wekreden wekreden() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER:
      return WEK_TIMER;
    case ESP_SLEEP_WAKEUP_EXT1:
      return WEK_GEBRUIKER;
    default:
      return WEK_STROOM;
  }
}

inline bool gewekt_door(gpio_num_t pin) {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1 &&
         (esp_sleep_get_ext1_wakeup_status() & (1ULL << pin));
}

// Direct na het opstarten: vastgezette pinnen weer vrijgeven en bijhouden of
// de touch-chip ons steeds meteen wakker maakt (dan touch-wekken uitzetten).
inline void na_wakker_worden() {
  for (gpio_num_t pin : UIT_IN_SLAAP)
    gpio_hold_dis(pin);
  gpio_hold_dis(PIN_TOUCH_RESET);
  gpio_deep_sleep_hold_dis();
  if (gewekt_door(PIN_TOUCH_INT) && rtc_slaap_begin != 0 && std::time(nullptr) - rtc_slaap_begin < 5) {
    if (rtc_snelle_touch_wekkers < 255)
      rtc_snelle_touch_wekkers++;
  } else if (wekreden() == WEK_GEBRUIKER && !gewekt_door(PIN_TOUCH_INT)) {
    rtc_snelle_touch_wekkers = 0;  // met een knop gewekt: touch opnieuw proberen
  }
}

inline bool touch_wekken_actief(bool ingesteld) { return ingesteld && rtc_snelle_touch_wekkers < 3; }

// Vlak voor deep sleep: randapparatuur uit, wekbronnen instellen.
inline void bereid_slaap_voor(bool touch_wekt) {
  const bool touch = touch_wekken_actief(touch_wekt);
  for (gpio_num_t pin : UIT_IN_SLAAP) {
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    gpio_hold_en(pin);
  }
  // Touch-chip aan laten (reset hoog) als die mag wekken, anders uit.
  gpio_set_direction(PIN_TOUCH_RESET, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_TOUCH_RESET, touch ? 1 : 0);
  gpio_hold_en(PIN_TOUCH_RESET);
  gpio_deep_sleep_hold_en();

  uint64_t masker = 0;
  for (gpio_num_t pin : KNOPPEN) {
    masker |= 1ULL << pin;
    rtc_gpio_pullup_en(pin);
    rtc_gpio_pulldown_dis(pin);
  }
  if (touch) {
    masker |= 1ULL << PIN_TOUCH_INT;
    rtc_gpio_pullup_en(PIN_TOUCH_INT);
    rtc_gpio_pulldown_dis(PIN_TOUCH_INT);
  }
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  esp_sleep_enable_ext1_wakeup(masker, ESP_EXT1_WAKEUP_ANY_LOW);
  rtc_slaap_begin = std::time(nullptr);
}
#endif

}  // namespace ink
