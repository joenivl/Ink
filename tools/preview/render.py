#!/usr/bin/env python3
"""Maakt PNG-previews van het Ink-scherm zonder hardware.

Compileert ink_kalender.h met een nagebootste ESPHome-display (esphome_stub.h),
draait de tests en tekent elk scenario met Pillow.

    pip install pillow
    python3 tools/preview/render.py        # schrijft docs/preview-*.png
"""

import json
import pathlib
import re
import subprocess
import urllib.request

from PIL import Image, ImageDraw, ImageFont

HIER = pathlib.Path(__file__).resolve().parent
ROOT = HIER.parents[1]
BOUW = HIER / "build"
FONTS = HIER / "fonts"
UIT = ROOT / "docs"

# Moet overeenkomen met de font:-sectie in esphome/ink-kalender.yaml.
FAMILIE = "Kalam"  # = substitution 'lettertype'
FONT_DEFS = [  # (naam, gewicht, grootte)
    ("klein", 400, 22),
    ("normaal", 400, 26),
    ("vet", 700, 26),
    ("kop", 700, 34),
    ("groot", 700, 52),
]
SCENARIOS = ["week", "maand", "luisteren", "voorstel", "notitie", "melding", "accu-laag", "slaapstand", "wakker-worden"]
TEKENS = list(range(32, 0x250)) + [0x2013, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026]
EMOJI_FAMILIE = "Noto Emoji"  # 'extras' in de font:-sectie


def iconen() -> list:
    """ICONEN uit ink_kalender.h, gecontroleerd tegen de glyphs in de YAML."""
    h = (ROOT / "esphome" / "ink_kalender.h").read_text()
    lijst = [int(c, 16) for c in re.findall(r"0x([0-9A-F]+)", h.split("ICONEN[] = {")[1].split("};")[0])]
    yml = (ROOT / "esphome" / "ink-kalender.yaml").read_text()
    glyphs = [ord(c) for c in re.findall(r'"(.)"', yml.split("extras: &iconen")[1].split("]")[0])]
    assert lijst == sorted(set(lijst)), "ICONEN moet gesorteerd zijn, zonder dubbele"
    assert sorted(glyphs) == lijst, "ICONEN in ink_kalender.h en de glyphs in ink-kalender.yaml verschillen"
    return lijst


ICONEN = iconen()


def font_bestand(gewicht: int, familie: str = FAMILIE) -> pathlib.Path:
    pad = FONTS / f"{familie.lower().replace(' ', '_')}-{gewicht}.ttf"
    if not pad.exists():
        FONTS.mkdir(parents=True, exist_ok=True)
        familie = familie.replace(" ", "+")
        req = urllib.request.Request(
            f"https://fonts.googleapis.com/css2?family={familie}:wght@{gewicht}",
            headers={"User-Agent": "Wget/1.0"},  # geeft TTF-links i.p.v. WOFF2
        )
        css = urllib.request.urlopen(req).read().decode()
        url = css.split("url(")[1].split(")")[0]
        pad.write_bytes(urllib.request.urlopen(url).read())
    return pad


def laad_fonts() -> dict:
    """Per naam (lettertype, emoji-lettertype van dezelfde grootte)."""
    return {
        naam: (ImageFont.truetype(str(font_bestand(gewicht)), grootte),
               ImageFont.truetype(str(font_bestand(400, EMOJI_FAMILIE)), grootte))
        for naam, gewicht, grootte in FONT_DEFS
    }


def stukken(s: str, fonts: tuple) -> list:
    """Tekst in stukken per lettertype: emoji uit het emoji-lettertype."""
    uit = []
    for c in s:
        f = fonts[1] if ord(c) in ICONEN else fonts[0]
        if uit and uit[-1][1] is f:
            uit[-1][0] += c
        else:
            uit.append([c, f])
    return uit


def schrijf_metrics(fonts: dict) -> None:
    regels = ["#pragma once", "#include <cstdint>", "namespace metrics {"]
    tabellen = []
    for i, (naam, _, _) in enumerate(FONT_DEFS):
        tekens, emoji = fonts[naam]
        breedtes = [round(tekens.getlength(chr(c))) for c in TEKENS] + [round(emoji.getlength(chr(c))) for c in ICONEN]
        regels.append(f"static const uint16_t B{i}[] = {{{','.join(map(str, breedtes))}}};")
        tabellen.append(f"B{i}")
    regels.append(f"static const uint32_t CP[] = {{{','.join(map(str, TEKENS + ICONEN))}}};")
    regels.append(f"static const uint16_t *const TAB[] = {{{','.join(tabellen)}}};")
    hoogtes = [sum(fonts[n][0].getmetrics()) for n, _, _ in FONT_DEFS]
    regels.append(f"static const int H[] = {{{','.join(map(str, hoogtes))}}};")
    regels.append(
        "inline int breedte(int f, uint32_t cp) {"
        f" for (int i = 0; i < {len(TEKENS) + len(ICONEN)}; i++) if (CP[i] == cp) return TAB[f][i];"
        " return TAB[f][31]; }"  # onbekend teken: breedte van '?'
    )
    regels.append("inline int hoogte(int f) { return H[f]; }")
    regels.append("}  // namespace metrics")
    (BOUW / "metrics.h").write_text("\n".join(regels) + "\n")


def compileer(bron: str) -> pathlib.Path:
    uit = BOUW / pathlib.Path(bron).stem
    subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-DINK_HOST", f"-I{BOUW}", f"-I{HIER}",
         "-o", str(uit), str(HIER / bron)],
        check=True,
    )
    return uit


def teken(opdrachten: list, fonts: dict) -> Image.Image:
    img = Image.new("L", (1872, 1404), 255)
    d = ImageDraw.Draw(img)
    for o in opdrachten:
        if o["op"] == "fill":
            d.rectangle([0, 0, 1871, 1403], fill=o["c"])
        elif o["op"] == "rect":
            vak = [o["x"], o["y"], o["x"] + o["w"] - 1, o["y"] + o["h"] - 1]
            if o["fill"]:
                d.rectangle(vak, fill=o["c"])
            else:
                d.rectangle(vak, outline=o["c"])
        elif o["op"] == "line":
            d.line([o["x"], o["y"], o["x2"], o["y2"]], fill=o["c"])
        elif o["op"] == "circle":
            vak = [o["x"] - o["r"], o["y"] - o["r"], o["x"] + o["r"], o["y"] + o["r"]]
            if o["fill"]:
                d.ellipse(vak, fill=o["c"])
            else:
                d.ellipse(vak, outline=o["c"])
        elif o["op"] == "text":
            # Op de basislijn van het lettertype, zoals ESPHome emoji uit 'extras' plaatst.
            f = fonts[o["font"]]
            delen = stukken(o["s"], f)
            totaal = sum(fd.getlength(s) for s, fd in delen)
            x = o["x"] - [0, totaal / 2, totaal][o["align"]]
            y = o["y"] + f[0].getmetrics()[0]
            for s, fd in delen:
                d.text((x, y), s, font=fd, fill=o["c"], anchor="ls")
                x += fd.getlength(s)
    # 16 grijstinten, zoals het paneel
    return img.point(lambda v: round(v / 17) * 17)


def main() -> None:
    BOUW.mkdir(exist_ok=True)
    UIT.mkdir(exist_ok=True)
    fonts = laad_fonts()
    schrijf_metrics(fonts)

    subprocess.run([str(compileer("tests.cpp"))], check=True)

    preview = compileer("preview.cpp")
    for scenario in SCENARIOS:
        uitvoer = subprocess.run([str(preview), scenario], check=True, capture_output=True, text=True).stdout
        opdrachten = [json.loads(r) for r in uitvoer.splitlines() if r]
        pad = UIT / f"preview-{scenario}.png"
        teken(opdrachten, fonts).save(pad)
        print(f"{pad.relative_to(ROOT)}: {len(opdrachten)} tekenopdrachten")


if __name__ == "__main__":
    main()
