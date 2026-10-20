#!/usr/bin/env python3
"""Builds the web pages in docs/ (GitHub Pages serves that folder):

    docs/index.html      English home page
    docs/hu/index.html   Hungarian home page
    docs/sitemap.xml     both pages, with hreflang alternates

Everything that changes with the project is read from the project, so the pages follow it: the
version from CMakeLists.txt and the factory presets (name, group, styles) from Source/PatternData.cpp.
The FAQ is written once (FAQ below) and used for both the visible questions and the FAQPage
structured data, so they can't differ.

    python site/build-site.py

Python 3 only, no packages.
"""
import datetime
import html
import json
import os
import re
import struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DOCS = os.path.join(ROOT, "docs")

SITE = "https://miklosnemeth.github.io/mnArpee/"
REPO = "https://github.com/miklosnemeth/mnArpee"
AUTHOR = "Miklós Németh"
AUTHOR_HU = "Németh Miklós"


# ----------------------------------------------------------------------------------------------
# Facts read from the project
# ----------------------------------------------------------------------------------------------
def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        return f.read()


def project_version():
    m = re.search(r"project\(\s*mnArpee\s+VERSION\s+([0-9.]+)", read("CMakeLists.txt"))
    return m.group(1) if m else ""


def factory_presets():
    """[(group, [(name, [styles])])]: Melodic first, then Bassline, in the order the plugin lists them."""
    src = read("Source/PatternData.cpp")

    # The original factory set: the bank's programs, except its two bass lines (they are root-only
    # bass lines in the list below). The styles are chosen by name in makeFactoryPatterns().
    start = src.index("Bank makeFactoryBank")
    body = src[start:src.index("\n}\n", start)]
    styles = {m.group(1): m.group(2) for m in re.finditer(r'name == "([^"]+)"\s*\?\s*"([^"]+)"', src)}
    fallback = re.search(r'\:\s*"([A-Za-z]+)";\s*//\s*Tie Glide', src)
    originals = [n for n in re.findall(r'(?:up16|makeInitProgram)\("([^"]+)"', body) if n not in ("Bass 8ths", "Octave Bass")]

    melodic = []
    for n in originals:
        s = styles.get(n) or (fallback.group(1) if fallback else "")
        melodic.append((n, [s] if s else []))
    bass = []
    for m in re.finditer(r'\b(melodic|bass)\("([^"]*)",\s*pattern\("([^"]+)"', src):
        tags = [t.strip() for t in m.group(2).split(",") if t.strip()]
        (melodic if m.group(1) == "melodic" else bass).append((m.group(3), tags))
    return [("Melodic", melodic), ("Bassline", bass)]


def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    return struct.unpack(">II", head[16:24])


# ----------------------------------------------------------------------------------------------
# Texts. UI names (RATE, TYPE, Key 1, ...) stay in English in both languages.
# ----------------------------------------------------------------------------------------------
LANES = {
    "en": [("TYPE", "Normal note, Off, Rest, Tie (legato) or a random key."),
           ("OCTAVE", "-3 to +3 octaves for the step."),
           ("VELOCITY", "1 to 127."),
           ("STEP GATE", "The note length of the step: 10 to 400 % of the pattern's GATE, in steps of 5 %."),
           ("RATCHET", "1 to 4: the step plays its note that many times.")],
    "hu": [("TYPE", "Normál hang, Off, Rest, Tie (legato) vagy véletlen key."),
           ("OCTAVE", "-3 és +3 oktáv között a lépésre."),
           ("VELOCITY", "1 és 127 között."),
           ("STEP GATE", "A lépés hanghossza: a minta GATE-jének 10 és 400 %-a, 5 %-os lépésekben."),
           ("RATCHET", "1 és 4 között: a lépés annyiszor játssza a hangját.")],
}

VALUES = ["RATE 1/64 – 3/8", "OCT -2 – +2", "GATE 1 – 125 %", "SWING -50 – +50 %", "STEPS 0 – 32"]
STYLES = ["Trance", "House", "Techno", "Disco", "Funk", "Pop", "Synthwave", "Ambient", "Chill", "Dubstep"]

GROUP_TITLES = {
    "en": {"Melodic": "Melodic (the held keys)", "Bassline": "Bassline (only the root)"},
    "hu": {"Melodic": "Melodic (a lenyomott key-ek)", "Bassline": "Bassline (csak a root)"},
}

T = {
    "en": {
        "lang": "en", "locale": "en_US", "dir": "", "base": "",
        "title": "mnArpee – free pattern arpeggiator plugin (VST3) for Windows",
        "description": "mnArpee is a free pattern arpeggiator plugin (VST3) for Windows: hold a chord and play the pattern you "
                       "draw. Step matrix, ratchets, bass lines and {presets} factory presets.",
        "og_alt": "The mnArpee window with the key matrix and the value lanes",
        "skip": "Skip to the content",
        "nav": [("#features", "Features"), ("#presets", "Presets"), ("#pattern", "Pattern"), ("#faq", "FAQ"), ("#get", "Get it")],
        "other_lang": ("HU", "Magyar", "hu/"),
        "this_lang": "EN",
        "eyebrow": "Free pattern arpeggiator · VST3",
        "h1_hidden": " – free pattern arpeggiator plugin for Windows (VST3)",
        "tagline": "Hold a chord. Play your pattern.",
        "lede": "No fixed arp modes: you draw the whole pattern in a step matrix, and mnArpee plays it with the notes you hold. "
                "Bass lines, ratchets, random dice and {presets} ready-made presets.",
        "cta_download": "Download for Windows", "cta_guide": "User guide (PDF)", "cta_source": "Source on GitHub",
        "pdf": "mnArpee-User-Guide.pdf",
        "facts": ["v{version}", "VST3 · Windows x64", "Free and open source (AGPL-3.0)"],
        "main_alt": "The mnArpee window: the key matrix, the value lanes and the preset bar",
        "how_h": "How it works",
        "how_sub": "mnArpee is a MIDI effect. It makes no sound itself: it sits in front of your synth.",
        "steps": [("Hold a chord", "Your MIDI keyboard sends the notes. They become Key 1 to Key 5, sorted by pitch."),
                  ("The pattern plays", "Every step of the pattern picks a key (or the chord root), with its own octave, velocity, gate and ratchet."),
                  ("Your synth plays it", "Put any synth after mnArpee. The steps lock to the song position of your DAW.")],
        "feat_h": "Draw the pattern you hear",
        "feat_sub": "Everything you need for arps and bass lines, and nothing that gets in the way.",
        "cards": [
            ("A step matrix, not arp modes", "Click cells to put each step on Key 1 to 5, the chord root or a random key. Up to 32 steps."),
            ("Lanes for every step", "Type, octave, velocity, step gate and ratchet, one box per step. Drag, or use the mouse wheel."),
            ("Bass lines", "The Root row plays the root of the held chord. One dice makes a random bass line with accents, ghost notes and glides."),
            ("Dice with ranges", "Every lane has a dice. Right-click it to set the smallest and largest random value."),
            ("{presets} presets, and a real browser", "Melodic and Bassline presets in ten styles. Search, filters, favourites and your own presets."),
            ("Undo, tools, zoom", "Undo and redo, generate keys, shift, reverse, copy and paste a pattern, and a zoomable window."),
        ],
        "list_h": "Everything in the box",
        "list_sub": "The full feature list of mnArpee {version}.",
        "feature_groups": [
            ("Pattern", [
                "Up to 32 steps per pattern; step types Off, Normal, Rest, Tie (legato) and Random.",
                "Per step: key (Root, Key 1 to 5), octave -3 to +3, velocity, ratchet 1 to 4 and a step gate of 10 to 400 %.",
                "Pattern: rate 1/64 to 3/8, gate 1 to 125 %, swing -50 to +50 %, octave -2 to +2 and 0 to 32 steps.",
                "Steps 0 is MIDI thru and 1 is quantized thru.",
            ]),
            ("Playing", [
                "Held notes are sorted by pitch, every note a key (octave doubles too), up to five keys; the PATTERN strip shows which are used and which are left out.",
                "A key that is not held plays the chord root, so every pattern fits every chord. The chord name (Cmaj7, Am, C/E, ...) is shown.",
                "Steps lock to the song position of the host; free-run when the transport is stopped.",
                "Latch, sustain pedal and panic. Other MIDI (CC, pitch bend, aftertouch) passes through.",
                "Sample-accurate MIDI and an engine that never allocates on the audio thread.",
            ]),
            ("Presets", [
                "{presets} read-only factory presets in two groups, Melodic and Bassline, with ten style tags.",
                "A browser inside the plugin window with search, group and style filters and favourites.",
                "Save as, Revert, Rename, Style, Import, Export, Show in folder and Delete; your own presets are files shared by every project.",
                "A preset switches at the end of the running pattern. The host's 128 programs mirror the presets.",
                "A project stores the active preset with its edits, so it sounds the same on another computer.",
            ]),
            ("Workflow", [
                "Undo and redo for every step edit; the plugin takes no keyboard shortcuts, so it never steals a key from the DAW.",
                "Pattern tools: set all steps, generate keys (Up, Down, Up-Down, Down-Up, Converge, Random), shift, reverse, clear, copy and paste.",
                "Random ranges for every lane and a random bass line, kept with the project.",
                "A dark, flat window with the version under the title, zoom from 80 to 200 %.",
                "Free and open source under the AGPL-3.0.",
            ]),
        ],
        "presets_h": "{presets} factory presets",
        "presets_sub": "Ready to play, in two groups. All of them are read-only: edit one and use Save as to keep your version.",
        "pat_h": "The keys and the lanes",
        "pat_sub": "A step does not say \"C\": it says \"Key 1\", \"Key 2\", and so on, and the keys are the notes you hold.",
        "keys_h": "Keys",
        "keys_list": [
            "Key 1 to Key 5 are the held notes, lowest first, up to five.",
            "Root plays the root of the held chord (triads, sus, sevenths, power chords, inversions).",
            "Random picks one of the keys.",
            "A key you do not hold plays the root instead.",
        ],
        "lanes_h": "Lanes",
        "values_h": "Header values",
        "values_sub": "RATE, OCT, GATE, SWING and STEPS belong to the active preset.",
        "styles_h": "Styles",
        "styles_sub": "Every preset carries style tags that the browser can filter by.",
        "scr_h": "A look inside",
        "scr_sub": "A dark, flat window: the matrix for the keys, and the lanes under it.",
        "shots": [
            ("preset-browser.png", "The preset browser with group and style columns", "The preset browser: group, style, search, favourites"),
            ("random-range.png", "The random options of a lane with MIN and MAX", "Right-click a dice: the range of the random values"),
        ],
        "req_h": "System requirements",
        "reqs": [
            "Windows 10 or 11, 64-bit (installer for the VST3).",
            "A DAW or host that loads VST3 plugins.",
            "A MIDI keyboard or controller (or the DAW's piano roll) to hold the chords.",
            "A synth or sampler after mnArpee, because mnArpee itself makes no sound.",
            "macOS and Linux are not supported yet.",
        ],
        "faq_h": "Frequently asked questions",
        "get_h": "Get mnArpee",
        "get_sub": "Free, with the source code open. The Windows installer is the quick way in.",
        "get_win": "Windows (64-bit)",
        "get_steps": [
            "Download <strong>mnArpee-{version}-win-x64-setup.exe</strong> from the latest release.",
            "Run it. The VST3 goes to <code>Common Files\\VST3\\MNAudio</code>.",
            "Rescan plugins in your DAW and put mnArpee on a track, before your synth.",
        ],
        "get_btn": "Latest release",
        "docs_h": "Docs and source",
        "links": [
            ("{repo}/blob/main/docs/USER_GUIDE.md", "User guide (English)"),
            ("{base}mnArpee-User-Guide.pdf", "User guide, PDF (English)"),
            ("{repo}/blob/main/docs/USER_GUIDE.hu.md", "User guide (Hungarian)"),
            ("{base}mnArpee-Felhasznaloi-utmutato.pdf", "User guide, PDF (Hungarian)"),
            ("{repo}", "Source code and build instructions"),
            ("{repo}/issues", "Report a bug or ask for a feature"),
            ("{repo}/blob/main/LICENSE", "License (AGPL-3.0)"),
        ],
        "mac": "macOS and Linux are not supported yet.",
        "by": "Vibecoded by",
        "name": AUTHOR, "name_html": "Miklós <b>Németh</b>",
        "about": "mnArpee is a vibecoding project: the idea, the taste and the decisions are mine, the code is written together with an AI.",
        "built": 'Built with <a href="https://juce.com">JUCE</a>. Fonts: Montserrat (OFL) and Roboto Condensed (Apache 2.0).',
    },
    "hu": {
        "lang": "hu", "locale": "hu_HU", "dir": "hu/", "base": "../",
        "title": "mnArpee – ingyenes pattern-arpeggiátor plugin (VST3) Windowsra",
        "description": "Az mnArpee egy ingyenes pattern-arpeggiátor plugin (VST3) Windowsra: tarts egy akkordot, és játssza a "
                       "mintát, amit megrajzoltál. Lépésmátrix, ratchet, basszusmenetek és {presets} gyári preset.",
        "og_alt": "Az mnArpee ablaka a key-mátrixszal és az értéksávokkal",
        "skip": "Ugrás a tartalomra",
        "nav": [("#features", "Funkciók"), ("#presets", "Presetek"), ("#pattern", "Minta"), ("#faq", "GYIK"), ("#get", "Letöltés")],
        "other_lang": ("EN", "English", "../"),
        "this_lang": "HU",
        "eyebrow": "Ingyenes pattern-arpeggiátor · VST3",
        "h1_hidden": " – ingyenes pattern-arpeggiátor plugin Windowsra (VST3)",
        "tagline": "Tarts egy akkordot. Játszd a mintádat.",
        "lede": "Nincsenek rögzített arp-módok: az egész mintát egy lépésmátrixban rajzolod, az mnArpee pedig a lenyomott "
                "hangokkal játssza. Basszusmenetek, ratchet, véletlen kockák és {presets} kész preset.",
        "cta_download": "Letöltés Windowsra", "cta_guide": "Útmutató (PDF)", "cta_source": "Forráskód a GitHubon",
        "pdf": "mnArpee-Felhasznaloi-utmutato.pdf",
        "facts": ["v{version}", "VST3 · Windows x64", "Ingyenes és nyílt forráskódú (AGPL-3.0)"],
        "main_alt": "Az mnArpee ablaka: a key-mátrix, az értéksávok és a preset-sáv",
        "how_h": "Hogyan működik",
        "how_sub": "Az mnArpee MIDI-effekt. Hangot nem ad, a szintetizátorod elé kerül.",
        "steps": [("Tarts egy akkordot", "A MIDI-billentyűzeted elküldi a hangokat. Ezekből lesz a Key 1 – Key 5, magasság szerint rendezve."),
                  ("A minta játszik", "A minta minden lépése választ egy key-t (vagy az akkord rootját), saját oktávval, velocityvel, gate-tel és ratchettel."),
                  ("A szintetizátor lejátssza", "Tegyél bármilyen szintetizátort az mnArpee után. A lépések a DAW dalpozíciójához igazodnak.")],
        "feat_h": "Rajzold meg a mintát, amit hallasz",
        "feat_sub": "Minden, ami az arpokhoz és a basszusmenetekhez kell, és semmi, ami útban van.",
        "cards": [
            ("Lépésmátrix arp-módok helyett", "Kattints a cellákra: minden lépés a Key 1–5, az akkord rootja vagy egy véletlen key lehet. Legfeljebb 32 lépés."),
            ("Sávok minden lépéshez", "Type, octave, velocity, step gate és ratchet, lépésenként egy mező. Húzd, vagy használd az egérgörgőt."),
            ("Basszusmenetek", "A Root sor a lenyomott akkord rootját játssza. Egy kocka véletlen basszusmenetet csinál hangsúlyokkal, szellemhangokkal és csúsztatásokkal."),
            ("Kockák tartománnyal", "Minden sávnak van kockája. Jobb kattintásra megadhatod a legkisebb és a legnagyobb véletlen értéket."),
            ("{presets} preset és igazi böngésző", "Melodic és Bassline presetek tíz stílusban. Keresés, szűrők, kedvencek és saját presetek."),
            ("Undo, eszközök, zoom", "Visszavonás és újra, key-generálás, eltolás, megfordítás, minta másolása és beillesztése, és átméretezhető ablak."),
        ],
        "list_h": "Minden, ami benne van",
        "list_sub": "Az mnArpee {version} teljes funkciólistája.",
        "feature_groups": [
            ("Minta", [
                "Mintánként legfeljebb 32 lépés; lépéstípusok: Off, Normal, Rest, Tie (legato) és Random.",
                "Lépésenként: key (Root, Key 1–5), oktáv -3 és +3 között, velocity, ratchet 1–4 és 10–400 % step gate.",
                "Mintánként: rate 1/64 – 3/8, gate 1–125 %, swing -50 – +50 %, oktáv -2 – +2 és 0–32 lépés.",
                "A 0 lépés MIDI thru, az 1 kvantált thru.",
            ]),
            ("Játék", [
                "A lenyomott hangok magasság szerint rendezettek, minden hang egy key (az oktávduplák is), legfeljebb öt; a PATTERN sáv mutatja, melyik van használatban és melyik maradt ki.",
                "Az a key, amelyet nem tartasz, az akkord rootját játssza, így minden minta minden akkordhoz illik. Az akkord neve (Cmaj7, Am, C/E, ...) látszik.",
                "A lépések a hoszt dalpozíciójához igazodnak; álló transzportnál szabadon futnak.",
                "Latch, szusztain pedál és panic. A többi MIDI (CC, pitch bend, aftertouch) átmegy.",
                "Mintapontos MIDI és olyan motor, amely az audioszálon soha nem foglal memóriát.",
            ]),
            ("Presetek", [
                "{presets} csak olvasható gyári preset két csoportban, Melodic és Bassline, tíz stílus-címkével.",
                "Böngésző a plugin ablakán belül, kereséssel, csoport- és stílusszűrővel és kedvencekkel.",
                "Save as, Revert, Rename, Style, Import, Export, Show in folder és Delete; a saját presetjeid fájlok, amelyeket minden projekt közösen használ.",
                "A preset a futó minta végén vált. A hoszt 128 programja a presetek tükre.",
                "A projekt az aktív presetet a szerkesztéseivel együtt tárolja, ezért egy másik gépen is ugyanúgy szól.",
            ]),
            ("Munkafolyamat", [
                "Visszavonás és újra minden lépés-szerkesztéshez; a plugin nem fogad billentyűparancsot, így sosem vesz el billentyűt a DAW-tól.",
                "Minta-eszközök: minden lépés beállítása, key-generálás (Up, Down, Up-Down, Down-Up, Converge, Random), eltolás, megfordítás, törlés, másolás és beillesztés.",
                "Véletlen tartományok minden sávhoz és véletlen basszusmenet, a projekttel együtt mentve.",
                "Sötét, letisztult ablak, a cím alatt a verzióval, zoom 80 és 200 % között.",
                "Ingyenes és nyílt forráskódú, AGPL-3.0 alatt.",
            ]),
        ],
        "presets_h": "{presets} gyári preset",
        "presets_sub": "Azonnal játszhatók, két csoportban. Mind csak olvasható: szerkeszd, és a Save as gombbal mentsd el a saját változatodat.",
        "pat_h": "A key-ek és a sávok",
        "pat_sub": "Egy lépés nem azt mondja, hogy \"C\", hanem hogy \"Key 1\", \"Key 2\" és így tovább, a key-ek pedig a lenyomott hangok.",
        "keys_h": "Key-ek",
        "keys_list": [
            "A Key 1 – Key 5 a lenyomott hangok, alulról felfelé, legfeljebb öt.",
            "A Root a lenyomott akkord alaphangját játssza (hármashangzatok, sus, szeptimek, power chord, fordítások).",
            "A Random véletlenszerűen választ a key-ek közül.",
            "Az a key, amelyet nem tartasz, a rootot játssza helyette.",
        ],
        "lanes_h": "Sávok",
        "values_h": "A fejléc értékei",
        "values_sub": "A RATE, OCT, GATE, SWING és STEPS az aktív presethez tartozik.",
        "styles_h": "Stílusok",
        "styles_sub": "Minden presetnek vannak stílus-címkéi, amelyek szerint a böngésző szűrni tud.",
        "scr_h": "Egy pillantás belülről",
        "scr_sub": "Sötét, letisztult ablak: a mátrix a key-ekhez, alatta a sávok.",
        "shots": [
            ("preset-browser.png", "A preset-böngésző csoport és stílus oszlopokkal", "A preset-böngésző: csoport, stílus, keresés, kedvencek"),
            ("random-range.png", "Egy sáv véletlen beállításai MIN és MAX értékkel", "Jobb kattintás egy kockán: a véletlen értékek tartománya"),
        ],
        "req_h": "Rendszerkövetelmények",
        "reqs": [
            "Windows 10 vagy 11, 64 bites (telepítő a VST3-hoz).",
            "VST3 pluginokat betöltő DAW vagy hoszt.",
            "MIDI-billentyűzet vagy kontroller (vagy a DAW zongorasávja) az akkordok tartásához.",
            "Egy szintetizátor vagy sampler az mnArpee után, mert az mnArpee maga nem ad hangot.",
            "macOS-t és Linuxot egyelőre nem támogat.",
        ],
        "faq_h": "Gyakori kérdések",
        "get_h": "Szerezd meg az mnArpee-t",
        "get_sub": "Ingyenes, nyílt forráskóddal. A leggyorsabb út a Windows-telepítő.",
        "get_win": "Windows (64 bites)",
        "get_steps": [
            "Töltsd le az <strong>mnArpee-{version}-win-x64-setup.exe</strong> fájlt a legfrissebb kiadásból.",
            "Futtasd. A VST3 a <code>Common Files\\VST3\\MNAudio</code> mappába kerül.",
            "Indíts új pluginkeresést a DAW-ban, és tedd az mnArpee-t egy sávra, a szintetizátor elé.",
        ],
        "get_btn": "Legfrissebb kiadás",
        "docs_h": "Dokumentáció és forrás",
        "links": [
            ("{repo}/blob/main/docs/USER_GUIDE.hu.md", "Felhasználói útmutató (magyar)"),
            ("{base}mnArpee-Felhasznaloi-utmutato.pdf", "Felhasználói útmutató, PDF (magyar)"),
            ("{repo}/blob/main/docs/USER_GUIDE.md", "Felhasználói útmutató (angol)"),
            ("{base}mnArpee-User-Guide.pdf", "Felhasználói útmutató, PDF (angol)"),
            ("{repo}", "Forráskód és fordítási útmutató"),
            ("{repo}/issues", "Hibabejelentés, ötletek"),
            ("{repo}/blob/main/LICENSE", "Licenc (AGPL-3.0)"),
        ],
        "mac": "macOS-t és Linuxot egyelőre nem támogat.",
        "by": "Vibecoder",
        "name": AUTHOR_HU, "name_html": "<b>Németh</b> Miklós",
        "about": "Az mnArpee egy vibecoding-projekt: az ötlet, az ízlés és a döntések az enyémek, a kódot egy MI-vel együtt írom.",
        "built": '<a href="https://juce.com">JUCE</a> alapon. Betűk: Montserrat (OFL) és Roboto Condensed (Apache 2.0).',
    },
}

# One FAQ for both the visible questions and the structured data. Answers may contain <code>.
FAQ = {
    "en": [
        ("What is mnArpee?",
         "mnArpee is a free pattern arpeggiator plugin (VST3) for Windows. You hold a chord and it plays a pattern of single notes. "
         "There are no fixed arp modes: you draw the pattern step by step in a matrix, or start from a factory preset."),
        ("Is mnArpee free?",
         "Yes. mnArpee is free software under the GNU Affero General Public License v3.0, and the source code is open on GitHub."),
        ("Does mnArpee make sound by itself?",
         "No. It is a MIDI effect: it turns the notes you hold into a pattern, and the synth or sampler after it plays the sound. "
         "Put it on a track, before your synth."),
        ("Which operating systems does it run on?",
         "There is an installer for Windows 10 and 11 (64-bit). macOS and Linux are not supported yet."),
        ("Which DAWs can I use it in?",
         "mnArpee is a standard VST3 plugin, so a DAW that hosts VST3 plugins on Windows can load it. "
         "How MIDI is routed to the next plugin differs between DAWs; the user guide has the steps."),
        ("What do Key 1 to Key 5 mean?",
         "They are the notes you hold, sorted by pitch, up to five. A step says which key it plays, so the same pattern fits any "
         "chord. A key you do not hold plays the root of the chord instead."),
        ("How do I make a bass line?",
         "Pick a Bassline preset, or put steps on the Root row. Root plays the root of the held chord. The dice at the Root row makes a "
         "random bass line with accents, ghost notes, rests, glides, octave jumps and ratchets."),
        ("What is the difference between GATE and STEP GATE?",
         "GATE in the header is the note length of the whole pattern, as a percentage of a step. STEP GATE is a lane that scales it for "
         "single steps, from 10 to 400 %. The length of a note is GATE times STEP GATE."),
        ("Where are my presets saved?",
         "Your presets are files in <code>Documents\\MNAudio\\mnArpee\\User</code>, shared by every project. A DAW project also stores the "
         "active preset, with its edits, so it sounds the same on another computer."),
        ("Does it use keyboard shortcuts?",
         "No. The plugin takes no keys at all, so it can never collide with the shortcuts of your DAW."),
        ("Who made mnArpee?",
         "mnArpee is made by Miklós Németh as a vibecoding project: the idea, the taste and the decisions are his, and the code is written together with an AI."),
    ],
    "hu": [
        ("Mi az mnArpee?",
         "Az mnArpee egy ingyenes pattern-arpeggiátor plugin (VST3) Windowsra. Lenyomsz egy akkordot, és egyes hangok mintáját játssza le. "
         "Nincsenek rögzített arp-módok: a mintát lépésről lépésre rajzolod egy mátrixban, vagy egy gyári presetből indulsz."),
        ("Ingyenes az mnArpee?",
         "Igen. Az mnArpee szabad szoftver a GNU Affero General Public License v3.0 alatt, a forráskódja nyílt a GitHubon."),
        ("Ad hangot az mnArpee magától?",
         "Nem. MIDI-effekt: a lenyomott hangokból mintát csinál, a hangot a mögötte lévő szintetizátor vagy sampler adja. "
         "Tedd egy sávra, a szintetizátor elé."),
        ("Milyen operációs rendszereken fut?",
         "Windows 10-re és 11-re (64 bites) van telepítő. macOS-t és Linuxot egyelőre nem támogat."),
        ("Melyik DAW-ban használhatom?",
         "Az mnArpee szabványos VST3 plugin, ezért minden DAW betöltheti, amelyik Windowson VST3 pluginokat hosztol. "
         "Hogy a MIDI hogyan jut a következő pluginhoz, DAW-onként más; a lépések az útmutatóban vannak."),
        ("Mit jelent a Key 1 – Key 5?",
         "A lenyomott hangok magasság szerint rendezve, legfeljebb öt. A lépés megmondja, melyik key-t játssza, így ugyanaz a minta "
         "bármilyen akkordhoz illik. Az a key, amelyet nem tartasz, az akkord rootját játssza helyette."),
        ("Hogyan csinálok basszusmenetet?",
         "Válassz egy Bassline presetet, vagy tegyél lépéseket a Root sorra. A Root a lenyomott akkord alaphangját játssza. A Root sor kockája "
         "véletlen basszusmenetet csinál hangsúlyokkal, szellemhangokkal, szünetekkel, csúsztatásokkal, oktávugrásokkal és ratchetekkel."),
        ("Mi a különbség a GATE és a STEP GATE között?",
         "A fejléc GATE értéke az egész minta hanghossza a lépés százalékában. A STEP GATE egy sáv, amely ezt skálázza az egyes lépésekre, "
         "10 és 400 % között. A hang hossza a GATE és a STEP GATE szorzata."),
        ("Hol vannak a presetjeim?",
         "A saját presetjeid fájlok a <code>Documents\\MNAudio\\mnArpee\\User</code> mappában, minden projekt közösen használja őket. A DAW-projekt az "
         "aktív presetet a szerkesztéseivel együtt is tárolja, ezért egy másik gépen is ugyanúgy szól."),
        ("Használ billentyűparancsokat?",
         "Nem. A plugin egyetlen billentyűt sem fogad, így sosem ütközhet a DAW gyorsbillentyűivel."),
        ("Ki készítette az mnArpee-t?",
         "Az mnArpee-t Németh Miklós készítette vibecoding-projektként: az ötlet, az ízlés és a döntések az övéi, a kódot egy MI-vel együtt írja."),
    ],
}


# ----------------------------------------------------------------------------------------------
# HTML
# ----------------------------------------------------------------------------------------------
def esc(s):
    return html.escape(s, quote=True)


def plain(s):
    return html.unescape(re.sub(r"<[^>]+>", "", s))


def logo_svg(extra=""):
    return ('<svg %sviewBox="0 0 32 32" aria-hidden="true"><rect width="32" height="32" rx="7" fill="#13211a"/>'
            '<rect x="6" y="7" width="5" height="18" rx="1.5" fill="#3ddc84"/><rect x="13.5" y="7" width="5" height="18" rx="1.5" fill="#fbfcfb"/>'
            '<rect x="21" y="7" width="5" height="18" rx="1.5" fill="#3ddc84"/></svg>') % extra


ICONS = [
    '<rect x="3" y="5" width="18" height="14" rx="2"/><path d="M8 5v14M13 5v14M18 5v14M3 12h18"/>',
    '<path d="M4 7h16M4 12h10M4 17h6"/>',
    '<path d="M3 18c3 0 3-12 6-12s3 12 6 12 3-8 6-8"/>',
    '<rect x="4" y="4" width="16" height="16" rx="3"/><circle cx="9" cy="9" r="1.2"/><circle cx="15" cy="15" r="1.2"/><circle cx="12" cy="12" r="1.2"/>',
    '<path d="m12 3 2.7 5.6 6.1.9-4.4 4.3 1 6.1L12 17l-5.4 2.9 1-6.1L3.2 9.5l6.1-.9z"/>',
    '<path d="M9 14 4 9l5-5M4 9h10a6 6 0 0 1 0 12h-3"/>',
]


def build_page(lang, ctx):
    t = T[lang]
    fmt = dict(presets=ctx["n_presets"], version=ctx["version"], repo=REPO, base=t["base"])

    def f(s):
        return s.format(**fmt)

    url = SITE + t["dir"]
    en_url, hu_url = SITE, SITE + "hu/"
    title = t["title"]
    description = f(t["description"])
    image = SITE + "images/main.png"
    iw, ih = ctx["image_size"]
    base = t["base"]
    cta_download = REPO + "/releases/latest"

    faq = FAQ[lang]

    # ---- structured data
    graph = [
        {"@type": "WebSite", "@id": SITE + "#website", "url": SITE, "name": "mnArpee",
         "inLanguage": ["en", "hu"], "publisher": {"@id": SITE + "#author"}},
        {"@type": "Person", "@id": SITE + "#author", "name": AUTHOR, "alternateName": AUTHOR_HU,
         "url": "https://github.com/miklosnemeth", "jobTitle": "Vibecoder"},
        {"@type": "SoftwareApplication", "@id": SITE + "#app", "name": "mnArpee",
         "url": url, "description": description, "inLanguage": lang,
         "applicationCategory": "MultimediaApplication", "applicationSubCategory": "MIDI effect, VST3 audio plugin",
         "operatingSystem": "Windows 10, Windows 11", "softwareVersion": ctx["version"],
         "downloadUrl": cta_download, "installUrl": cta_download, "fileFormat": "application/vnd.steinberg.vst3",
         "softwareRequirements": "A DAW that hosts VST3 plugins",
         "license": "https://www.gnu.org/licenses/agpl-3.0.html", "isAccessibleForFree": True,
         "offers": {"@type": "Offer", "price": "0", "priceCurrency": "USD"},
         "author": {"@id": SITE + "#author"}, "image": image,
         "screenshot": [SITE + "images/" + s[0] for s in t["shots"]] + [image],
         "featureList": [plain(f(x)) for _, items in t["feature_groups"] for x in items]},
        {"@type": "FAQPage", "@id": url + "#faq", "inLanguage": lang,
         "mainEntity": [{"@type": "Question", "name": q,
                         "acceptedAnswer": {"@type": "Answer", "text": plain(a)}} for q, a in faq]},
    ]
    jsonld = json.dumps({"@context": "https://schema.org", "@graph": graph}, ensure_ascii=False, indent=1)
    jsonld = jsonld.replace("</", "<\\/")

    o = []
    w = o.append

    w("<!doctype html>")
    w('<html lang="%s">' % lang)
    w("<head>")
    w('<meta charset="utf-8">')
    w('<meta name="viewport" content="width=device-width, initial-scale=1">')
    w("<title>%s</title>" % esc(title))
    w('<meta name="description" content="%s">' % esc(description))
    w('<meta name="robots" content="index, follow, max-image-preview:large">')
    w('<meta name="author" content="%s">' % esc(t["name"]))
    w('<link rel="canonical" href="%s">' % url)
    w('<link rel="alternate" hreflang="en" href="%s">' % en_url)
    w('<link rel="alternate" hreflang="hu" href="%s">' % hu_url)
    w('<link rel="alternate" hreflang="x-default" href="%s">' % en_url)
    w('<meta property="og:type" content="website">')
    w('<meta property="og:site_name" content="mnArpee">')
    w('<meta property="og:title" content="%s">' % esc(title))
    w('<meta property="og:description" content="%s">' % esc(description))
    w('<meta property="og:url" content="%s">' % url)
    w('<meta property="og:locale" content="%s">' % t["locale"])
    w('<meta property="og:locale:alternate" content="%s">' % T["hu" if lang == "en" else "en"]["locale"])
    w('<meta property="og:image" content="%s">' % image)
    w('<meta property="og:image:width" content="%d">' % iw)
    w('<meta property="og:image:height" content="%d">' % ih)
    w('<meta property="og:image:alt" content="%s">' % esc(t["og_alt"]))
    w('<meta name="twitter:card" content="summary_large_image">')
    w('<meta name="twitter:title" content="%s">' % esc(title))
    w('<meta name="twitter:description" content="%s">' % esc(description))
    w('<meta name="twitter:image" content="%s">' % image)
    w('<meta name="color-scheme" content="dark">')
    w('<meta name="theme-color" content="#0b1510">')
    w('<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns=\'http://www.w3.org/2000/svg\' viewBox=\'0 0 32 32\'%3E%3Crect width=\'32\' height=\'32\' rx=\'7\' fill=\'%230b1510\'/%3E%3Crect x=\'6\' y=\'7\' width=\'5\' height=\'18\' rx=\'1.5\' fill=\'%233ddc84\'/%3E%3Crect x=\'13.5\' y=\'7\' width=\'5\' height=\'18\' rx=\'1.5\' fill=\'%23fbfcfb\'/%3E%3Crect x=\'21\' y=\'7\' width=\'5\' height=\'18\' rx=\'1.5\' fill=\'%233ddc84\'/%3E%3C/svg%3E">')
    w('<link rel="preload" href="%sassets/fonts/Montserrat.ttf" as="font" type="font/ttf" crossorigin>' % base)
    w("<style>")
    w(ctx["css"].replace("{{BASE}}", base))
    w("</style>")
    w('<script type="application/ld+json">')
    w(jsonld)
    w("</script>")
    w("</head>")
    w("<body>")
    w('<a class="skip" href="#main">%s</a>' % esc(t["skip"]))

    # ---- header
    w('<div class="wrap">')
    w('  <header class="top">')
    w('    <a class="brand display" href="%s" aria-label="mnArpee">%smnArpee</a>' % (base or "./", logo_svg()))
    w('    <nav class="nav" aria-label="%s">' % ("Main" if lang == "en" else "Főmenü"))
    for href, label in t["nav"]:
        w('      <a class="hide-sm" href="%s">%s</a>' % (href, esc(label)))
    code_, name_, path_ = t["other_lang"]
    w('      <span class="lang" role="group" aria-label="Language / Nyelv"><span class="cur" aria-current="true">%s</span>'
      '<a href="%s" hreflang="%s" lang="%s" title="%s">%s</a></span>'
      % (t["this_lang"], path_, code_.lower(), code_.lower(), esc(name_), code_))
    w("    </nav>")
    w("  </header>")

    # ---- hero
    w('  <main id="main">')
    w('    <div class="hero">')
    w('      <p class="eyebrow">%s</p>' % esc(t["eyebrow"]))
    w('      <h1 class="display">mn<span>Arpee</span><span class="sr-only">%s</span></h1>' % esc(t["h1_hidden"]))
    w('      <p class="tagline">%s</p>' % esc(t["tagline"]))
    w('      <p class="lede">%s</p>' % esc(f(t["lede"])))
    w('      <div class="cta">')
    w('        <a class="btn primary" href="%s"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" '
      'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3v12m0 0-5-5m5 5 5-5M4 20h16"/></svg>%s</a>'
      % (cta_download, esc(t["cta_download"])))
    w('        <a class="btn" href="%s%s"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" '
      'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M6 3h8l4 4v14H6z"/><path d="M14 3v4h4M9 13h6M9 17h6"/></svg>%s</a>'
      % (base, t["pdf"], esc(t["cta_guide"])))
    w('        <a class="btn" href="%s"><svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M12 .5a11.5 11.5 0 0 0-3.64 '
      '22.41c.58.1.79-.25.79-.56v-2c-3.2.7-3.88-1.37-3.88-1.37-.52-1.33-1.28-1.69-1.28-1.69-1.04-.71.08-.7.08-.7 1.15.08 1.76 1.19 '
      '1.76 1.19 1.03 1.76 2.69 1.25 3.35.96.1-.75.4-1.25.73-1.54-2.55-.29-5.24-1.28-5.24-5.69 0-1.26.45-2.28 1.19-3.09-.12-.29-.52-1.46.11-3.05 '
      '0 0 .97-.31 3.17 1.18a11 11 0 0 1 5.78 0c2.2-1.49 3.17-1.18 3.17-1.18.63 1.59.23 2.76.11 3.05.74.81 1.19 1.83 1.19 3.09 0 4.42-2.69 '
      '5.39-5.25 5.68.41.36.78 1.06.78 2.14v3.17c0 .31.21.67.8.56A11.5 11.5 0 0 0 12 .5Z"/></svg>%s</a>' % (REPO, esc(t["cta_source"])))
    w("      </div>")
    w('      <ul class="facts">')
    for fact in t["facts"]:
        w("        <li>%s</li>" % esc(f(fact)))
    w("      </ul>")
    w('      <div class="shot"><img src="%simages/main.png" width="%d" height="%d" alt="%s"></div>' % (base, iw, ih, esc(t["main_alt"])))
    w("    </div>")

    # ---- how it works
    w('    <section aria-labelledby="how">')
    w('      <h2 id="how" class="display">%s</h2>' % esc(t["how_h"]))
    w('      <p class="sub">%s</p>' % esc(t["how_sub"]))
    w('      <ol class="steps">')
    for h, p in t["steps"]:
        w('        <li class="step"><h3>%s</h3><p>%s</p></li>' % (esc(h), esc(p)))
    w("      </ol>")
    w("    </section>")

    # ---- features: cards, then the full list
    w('    <section id="features" aria-labelledby="feat">')
    w('      <h2 id="feat" class="display">%s</h2>' % esc(t["feat_h"]))
    w('      <p class="sub">%s</p>' % esc(t["feat_sub"]))
    w('      <ul class="cards">')
    for i, (h, p) in enumerate(t["cards"]):
        w('        <li class="card"><div class="ic"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" '
          'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">%s</svg></div><h3>%s</h3><p>%s</p></li>'
          % (ICONS[i], esc(f(h)), esc(f(p))))
    w("      </ul>")
    w('      <h3 class="listh">%s</h3>' % esc(t["list_h"]))
    w('      <p class="sub">%s</p>' % esc(f(t["list_sub"])))
    w('      <div class="featlists">')
    for gh, items in t["feature_groups"]:
        w('        <div class="fl"><h4>%s</h4><ul>' % esc(gh))
        for item in items:
            w("          <li>%s</li>" % esc(f(item)))
        w("        </ul></div>")
    w("      </div>")
    w("    </section>")

    # ---- presets
    w('    <section id="presets" aria-labelledby="pre">')
    w('      <h2 id="pre" class="display">%s</h2>' % esc(f(t["presets_h"])))
    w('      <p class="sub">%s</p>' % esc(t["presets_sub"]))
    w('      <div class="presetgroups two">')
    for group, presets in ctx["groups"]:
        w('        <div class="pg"><h3>%s</h3><ul>' % esc(GROUP_TITLES[lang].get(group, group)))
        for name, tags in presets:
            extra = ' <span class="tags">%s</span>' % esc(", ".join(tags)) if tags else ""
            w("          <li><strong>%s</strong>%s</li>" % (esc(name), extra))
        w("        </ul></div>")
    w("      </div>")
    w("    </section>")

    # ---- pattern: keys, lanes, header values, styles
    w('    <section id="pattern" aria-labelledby="pat">')
    w('      <h2 id="pat" class="display">%s</h2>' % esc(t["pat_h"]))
    w('      <p class="sub">%s</p>' % esc(t["pat_sub"]))
    w('      <div class="twocol">')
    w('        <div><h3 class="listh">%s</h3><ul class="plain">' % esc(t["keys_h"]))
    for x in t["keys_list"]:
        w("          <li>%s</li>" % esc(x))
    w("        </ul></div>")
    w('        <div><h3 class="listh">%s</h3><ul class="plain">' % esc(t["values_h"]))
    for x in VALUES:
        w("          <li>%s</li>" % esc(x))
    w("        </ul><p class=\"sub\">%s</p></div>" % esc(t["values_sub"]))
    w("      </div>")
    w('      <h3 class="listh">%s</h3>' % esc(t["lanes_h"]))
    w('      <ul class="symbols">')
    for name, text in LANES[lang]:
        w("        <li><code>%s</code> <span>%s</span></li>" % (esc(name), esc(text)))
    w("      </ul>")
    w('      <h3 class="listh">%s</h3><p class="sub">%s</p><ul class="chips">' % (esc(t["styles_h"]), esc(t["styles_sub"])))
    for x in STYLES:
        w("        <li>%s</li>" % esc(x))
    w("      </ul>")
    w("    </section>")

    # ---- screenshots
    w('    <section id="screens" aria-labelledby="scr">')
    w('      <h2 id="scr" class="display">%s</h2>' % esc(t["scr_h"]))
    w('      <p class="sub">%s</p>' % esc(t["scr_sub"]))
    w('      <div class="gallery">')
    for fn, alt, cap in t["shots"]:
        sw, sh = ctx["shot_sizes"][fn]
        w('        <figure><img src="%simages/%s" loading="lazy" width="%d" height="%d" alt="%s"><figcaption>%s</figcaption></figure>'
          % (base, fn, sw, sh, esc(alt), esc(cap)))
    w("      </div>")
    w("    </section>")

    # ---- requirements
    w('    <section id="requirements" aria-labelledby="req">')
    w('      <h2 id="req" class="display">%s</h2>' % esc(t["req_h"]))
    w('      <ul class="plain">')
    for r in t["reqs"]:
        w("        <li>%s</li>" % esc(r))
    w("      </ul>")
    w("    </section>")

    # ---- FAQ
    w('    <section id="faq" aria-labelledby="faqh">')
    w('      <h2 id="faqh" class="display">%s</h2>' % esc(t["faq_h"]))
    w('      <div class="faq">')
    for q, a in faq:
        w("        <details><summary>%s</summary><p>%s</p></details>" % (esc(q), a))
    w("      </div>")
    w("    </section>")

    # ---- get
    w('    <section id="get" aria-labelledby="getit">')
    w('      <h2 id="getit" class="display">%s</h2>' % esc(t["get_h"]))
    w('      <p class="sub">%s</p>' % esc(t["get_sub"]))
    w('      <div class="get">')
    w('        <div class="box"><h3>%s</h3><ol>' % esc(t["get_win"]))
    for s in t["get_steps"]:
        w("          <li>%s</li>" % f(s))
    w('        </ol><a class="btn primary" href="%s">%s</a></div>' % (cta_download, esc(t["get_btn"])))
    w('        <div class="box"><h3>%s</h3><ul>' % esc(t["docs_h"]))
    for href, label in t["links"]:
        w('          <li><a href="%s">%s</a></li>' % (esc(f(href)), esc(label)))
    w("        </ul><p>%s</p></div>" % esc(t["mac"]))
    w("      </div>")
    w("    </section>")
    w("  </main>")
    w("</div>")

    # ---- footer
    w('<footer class="author">')
    w('  <div class="wrap">')
    w('    <div class="tag"><svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" '
      'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="m8 8-5 4 5 4M16 8l5 4-5 4M14 5l-4 14"/></svg>%s</div>' % esc(t["by"]))
    w('    <p class="name display">%s</p>' % t["name_html"])
    w("    <p>%s</p>" % esc(t["about"]))
    w('    <p class="legal">© %d %s · <a href="%s/blob/main/LICENSE">AGPL-3.0</a> · %s</p>'
      % (ctx["year"], esc(t["name"]), REPO, t["built"]))
    w("  </div>")
    w("</footer>")
    w("</body>")
    w("</html>")
    return "\n".join(o) + "\n"


def sitemap(lastmod):
    def entry(url, other_url, lang_self, lang_other):
        return (
            "  <url>\n    <loc>%s</loc>\n    <lastmod>%s</lastmod>\n    <changefreq>monthly</changefreq>\n    <priority>%s</priority>\n"
            '    <xhtml:link rel="alternate" hreflang="%s" href="%s"/>\n    <xhtml:link rel="alternate" hreflang="%s" href="%s"/>\n'
            '    <xhtml:link rel="alternate" hreflang="x-default" href="%s"/>\n  </url>\n'
            % (url, lastmod, "1.0" if url == SITE else "0.9", lang_self, url, lang_other, other_url, SITE))
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" xmlns:xhtml="http://www.w3.org/1999/xhtml">\n'
            + entry(SITE, SITE + "hu/", "en", "hu") + entry(SITE + "hu/", SITE, "hu", "en") + "</urlset>\n")


def main():
    with open(os.path.join(os.path.dirname(__file__), "site.css"), encoding="utf-8") as f:
        css = f.read()
    css += "\n.presetgroups.two { grid-template-columns: repeat(2, 1fr); }\n@media (max-width: 860px) { .presetgroups.two { grid-template-columns: 1fr; } }\n"

    shots = {}
    for fn in ("main.png", "preset-browser.png", "random-range.png"):
        shots[fn] = png_size(os.path.join(DOCS, "images", fn))

    groups = factory_presets()
    n = sum(len(p) for _, p in groups)
    ctx = dict(
        version=project_version(), css=css, groups=groups, n_presets=n,
        image_size=shots["main.png"], shot_sizes=shots, year=datetime.date.today().year,
    )

    os.makedirs(os.path.join(DOCS, "hu"), exist_ok=True)
    for lang, rel in (("en", "index.html"), ("hu", os.path.join("hu", "index.html"))):
        with open(os.path.join(DOCS, rel), "w", encoding="utf-8", newline="\n") as f:
            f.write(build_page(lang, ctx))
        print("wrote docs/" + rel.replace("\\", "/"))

    with open(os.path.join(DOCS, "sitemap.xml"), "w", encoding="utf-8", newline="\n") as f:
        f.write(sitemap(datetime.date.today().isoformat()))
    print("wrote docs/sitemap.xml  (%d presets, version %s)" % (n, ctx["version"]))


if __name__ == "__main__":
    main()
