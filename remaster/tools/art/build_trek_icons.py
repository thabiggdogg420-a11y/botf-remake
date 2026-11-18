#!/usr/bin/env python3
"""Trek icon set (Simon, 18 Nov 2026: "it should all be a bit more trekky"): every UI icon drawn in-house as SVG on a 24 px grid
in one LCARS language: solid two-tone shapes (full ink and a 45 % tone), the combadge delta, LCARS pills and elbows, and
show-style objects (dilithium crystal, PADD, replicator alcove, latinum, isolinear chips, rank pips). No third-party sets or fonts.

Writes remaster/godot/scenes/ui/widgets/trek_icon_svg.gd (the SVG strings the game rasterises at the exact pixel size, so each
icon stays sharp at status-bar size on both renderers) and remaster/docs/art/trek-icons/<name>.svg for review (outside the Godot project, so not packed).
    python remaster/tools/art/build_trek_icons.py
"""
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
GODOT = os.path.normpath(os.path.join(HERE, "..", "..", "godot"))
LO = ' fill-opacity=".45"'   # the second tone
BG = ' fill-opacity=".30"'   # the button pill behind a control mark
STROKE = ' fill="none" stroke="#fff" stroke-linecap="round" stroke-linejoin="round"'


def f(v):
    s = "%.2f" % v
    s = s.rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def delta(cx, cy, r, ang=0.0):
    """The combadge delta: tip up before rotation `ang` (radians), as in UiIcon.delta_points."""
    pts = []
    ca, sa = math.cos(ang), math.sin(ang)
    for x, y in [(0, -1.0), (0.78, 0.95), (0, 0.5), (-0.78, 0.95)]:
        x, y = x * r, y * r
        pts.append((cx + x * ca - y * sa, cy + x * sa + y * ca))
    return "M" + " L".join("%s %s" % (f(x), f(y)) for x, y in pts) + "Z"


def circ(cx, cy, r):
    return "M%s %sa%s %s 0 1 0 %s 0a%s %s 0 1 0 %s 0Z" % (f(cx - r), f(cy), f(r), f(r), f(2 * r), f(r), f(r), f(-2 * r))


def rrect(x, y, w, h, r):
    return ("M%s %sh%sa%s %s 0 0 1 %s %sv%sa%s %s 0 0 1 %s %sh%sa%s %s 0 0 1 %s %sv%sa%s %s 0 0 1 %s %sZ" %
            (f(x + r), f(y), f(w - 2 * r), f(r), f(r), f(r), f(r), f(h - 2 * r), f(r), f(r), f(-r), f(r), f(-(w - 2 * r)), f(r), f(r),
             f(-r), f(-r), f(-(h - 2 * r)), f(r), f(r), f(r), f(-r)))


def P(d, tone=""):
    return '<path d="%s"%s/>' % (d, tone)


def PE(d, tone=""):
    return '<path fill-rule="evenodd" d="%s"%s/>' % (d, tone)


def padd(screen):
    """A PADD: solid case, screen cut out, three button cut-outs below; `screen` is drawn inside."""
    body = " ".join([rrect(5, 1.5, 14, 21, 2.4), "M7.4 4v11.6h9.2V4Z", rrect(7.6, 17.6, 2.6, 2, 1), rrect(10.7, 17.6, 2.6, 2, 1), rrect(13.8, 17.6, 2.6, 2, 1)])
    return PE(body) + screen


def laurel(side):
    out = []
    for a in ([140, 165, 190, 215] if side < 0 else [40, 15, -10, -35]):
        r = math.radians(a)
        x, y = 12 + 8.6 * math.cos(r), 12.4 + 8.6 * math.sin(r)
        out.append('<ellipse cx="%s" cy="%s" rx="1.3" ry="2.6" transform="rotate(%s %s %s)"/>' % (f(x), f(y), f(a + 90), f(x), f(y)))
    return "".join(out)


def pill_btn():
    return P(rrect(1.5, 1.5, 21, 21, 7), BG)


ICONS = {
    # ---- status bar ----
    # stardate chronometer: the segmented LCARS ring and the delta as its hand
    "turn": '<circle cx="12" cy="12" r="9.6" fill="none" stroke="#fff" stroke-width="3" stroke-dasharray="7.6 2.45" stroke-opacity=".55"/>'
            + P(delta(12, 12.8, 7)),
    # gold-pressed latinum: three ingots stacked
    "credits": P("M2.5 21.5 L4.6 16.4 H19.4 L21.5 21.5Z") + P("M5.6 14.6 L7.6 9.8 H16.4 L18.4 14.6Z") + P("M8.6 8 L10.2 3.6 H13.8 L15.4 8Z", LO),
    # research: the PADD with an LCARS elbow and readout bars on its screen
    "research": padd(P("M8.6 5.2h3.6v1.8h-1.6v7.4H8.6Z") + P(rrect(11.6, 8.6, 3.8, 1.7, .85)) + P(rrect(11.6, 11.6, 2.6, 1.7, .85), LO)),
    # industry: a replicator alcove with the materialisation shimmer
    "industry": PE("M3.5 22.5V9.5a8.5 8.5 0 0 1 17 0v13Z M6.4 19.6V9.8a5.6 5.6 0 0 1 11.2 0v9.8Z")
                + P(rrect(8.3, 9, 1.5, 9.2, .75), LO) + P(rrect(11.25, 7.4, 1.5, 10.8, .75)) + P(rrect(14.2, 10.4, 1.5, 7.8, .75), LO),
    # food: hydroponics, two leaves on an LCARS pill tray
    "food": P(rrect(3.5, 18.8, 17, 3.4, 1.7)) + P("M11.6 17.4C6.2 16.4 4.2 10.8 5 4.6c5 1 7.6 5.6 6.6 12.8Z")
            + P("M12.6 17.4c-.6-6.4 1.6-10.6 6.4-11.6.8 5.4-1.2 10.2-6.4 11.6Z", LO),
    # population: a crew member, the combadge cut into the uniform
    "population": P(circ(12, 6.2, 3.9)) + PE("M3.5 22.5v-3.4a6.2 6.2 0 0 1 6.2-6.2h4.6a6.2 6.2 0 0 1 6.2 6.2v3.4Z " + delta(15.6, 17.2, 2.7)),
    # intel: the Starfleet Intelligence eye, a delta for the iris
    "intel": PE("M1.5 12C5.6 5 18.4 5 22.5 12 18.4 19 5.6 19 1.5 12Z " + circ(12, 12, 4.7)) + P(delta(12, 12.2, 3.1)),
    # orders: the PADD of standing orders, delta bullets and lines
    "orders": padd(P(delta(9.6, 7.4, 1.6, math.pi / 2)) + P(rrect(11.6, 6.5, 4, 1.7, .85)) + P(delta(9.6, 12.2, 1.6, math.pi / 2), LO)
                   + P(rrect(11.6, 11.3, 3, 1.7, .85), LO)),
    # ---- the rail (F1 to F8, Bulletin, Objectives, Log) ----
    # cartography: the four quadrants, Alpha lit
    "galaxy": P("M11.2 12.8V22A10.2 10.2 0 0 1 2 12.8Z") + P("M11.2 11.2H2A10.2 10.2 0 0 1 11.2 2Z", LO)
              + P("M12.8 2A10.2 10.2 0 0 1 22 11.2H12.8Z", LO) + P("M22 12.8A10.2 10.2 0 0 1 12.8 22V12.8Z", LO) + P(circ(12, 12, 2.2)),
    # colony: the star, its orbit and the colony world
    "system": P(circ(12, 12, 3.6)) + '<circle cx="12" cy="12" r="8.4" fill="none" stroke="#fff" stroke-width="1.7" stroke-opacity=".5"/>'
              + P(circ(18.9, 7.2, 2.6)) + P(circ(5.6, 17.6, 1.5), LO),
    # council: the seal, laurels round a four-point star
    "empire": "<g>" + laurel(-1) + "</g><g" + LO + ">" + laurel(1) + "</g>" + P("M12 5.6 13.5 10.5 18.4 12 13.5 13.5 12 18.4 10.5 13.5 5.6 12 10.5 10.5Z"),
    # starfleet: three deltas in formation
    "fleets": P(delta(12, 7.6, 6.6)) + P(delta(6, 16.4, 5.4), LO) + P(delta(18, 16.4, 5.4), LO),
    # diplomats: the combadge on its oval, hailing
    "diplomacy": '<ellipse cx="9" cy="13.4" rx="6.8" ry="4.4"' + LO + '/>' + P(delta(9, 12.4, 6.6))
                 + '<path d="M16.6 8.2a6 6 0 0 1 0 8.4M19.4 5.6a9.6 9.6 0 0 1 0 13.6" stroke-width="1.9"' + STROKE + '/>',
    # records: isolinear chips
    "database": PE(rrect(3.4, 5, 5, 15.5, 1) + " " + rrect(4.9, 15.8, 2, 2.6, .6), LO) + PE(rrect(9.5, 2.5, 5, 19, 1) + " " + rrect(11, 17.2, 2, 2.8, .6))
                + PE(rrect(15.6, 5, 5, 15.5, 1) + " " + rrect(17.1, 15.8, 2, 2.6, .6), LO),
    # bulletin: the subspace relay, the delta transmitting
    "events": P(delta(12, 13.5, 5.2)) + P(rrect(5, 19.8, 14, 2.8, 1.4))
              + '<path d="M7.4 6.4a6.6 6.6 0 0 1 9.2 0M4.6 3.8a10.6 10.6 0 0 1 14.8 0" stroke-width="1.9" stroke-opacity=".7"' + STROKE + '/>',
    # objectives: the ribbon bar over the delta medal
    "victory": PE(rrect(4, 1.6, 16, 6.4, 1.2) + " M9.1 2.8h1.5v4H9.1Z M13.4 2.8h1.5v4h-1.5Z") + P(delta(12, 15.6, 6.6)),
    # log: the captain's log, record mark and the spoken lines
    "chronicle": padd(P(circ(10, 7.2, 1.6)) + P(rrect(12.4, 6.3, 3.4, 1.7, .85)) + P(rrect(8.4, 10.2, 7.4, 1.7, .85), LO) + P(rrect(8.4, 13.2, 5, 1.7, .85), LO)),
    # ---- system screen ----
    # growth: two deltas climbing
    "growth": P(delta(12, 9.4, 7.2)) + P(delta(12, 17.6, 5.2), LO),
    # morale: rank pips on the collar bar
    "morale": P(rrect(.8, 5.6, 22.4, 12.8, 6.4), LO) + P(circ(6.6, 12, 3)) + P(circ(12, 12, 3)) + P(circ(17.4, 12, 3)),
    # energy: a dilithium crystal, faceted
    "energy": P("M12 1.4 17.6 6.8H6.4Z") + P("M6.2 7.8h5.3V22.6H8.6Z") + P("M12.5 7.8h5.3l-2.4 14.8h-2.9Z", LO),
    # build time: the sand-glass of two deltas between LCARS caps
    "clock": P(rrect(4.5, 1.5, 15, 2.8, 1.4)) + P(rrect(4.5, 19.7, 15, 2.8, 1.4)) + P(delta(12, 9.6, 5.4, math.pi)) + P(delta(12, 14.6, 5.4), LO),
    # a worker figure for the workforce rows: crew member in uniform, combadge on the chest
    "worker": P(circ(12, 4.4, 3.2)) + PE("M5 23V14a5 5 0 0 1 5-5h4a5 5 0 0 1 5 5v9h-3.4v-6.4H8.4V23Z " + delta(14.6, 12.6, 2.1)),
    # ---- controls: the button pill with its mark ----
    "delta_up": pill_btn() + P(delta(12, 12.8, 7.2)),
    "delta_down": pill_btn() + P(delta(12, 11.2, 7.2, math.pi)),
    "delta_left": pill_btn() + P(delta(11.2, 12, 7.2, -math.pi / 2)),
    "delta_right": pill_btn() + P(delta(12.8, 12, 7.2, math.pi / 2)),
    "cross": pill_btn() + '<path d="M7.4 7.4l9.2 9.2M16.6 7.4l-9.2 9.2" stroke-width="3.2"' + STROKE + '/>',
    "plus": pill_btn() + '<path d="M12 6.4v11.2M6.4 12h11.2" stroke-width="3.2"' + STROKE + '/>',
    "minus": pill_btn() + '<path d="M6.4 12h11.2" stroke-width="3.2"' + STROKE + '/>',
    "tick": pill_btn() + '<path d="M6.6 12.6l3.6 3.6 7.4-8" stroke-width="3.2"' + STROKE + '/>',
    # ---- marks used across the screens ----
    # settings / the administrator: LCARS slider tracks with their knobs
    "settings": P(rrect(2, 4.4, 20, 3, 1.5), LO) + P(rrect(2, 10.5, 20, 3, 1.5), LO) + P(rrect(2, 16.6, 20, 3, 1.5), LO)
                + P(rrect(12, 3, 7, 5.8, 2.9)) + P(rrect(4, 9.1, 7, 5.8, 2.9)) + P(rrect(10, 15.2, 7, 5.8, 2.9)),
    # warning: the solid delta with the mark cut out
    "warn": PE(delta(12, 12.6, 11) + " " + rrect(10.9, 7.4, 2.2, 6.4, 1.1) + " " + circ(12, 16.4, 1.3)),
    # information: the LCARS readout pill with the mark
    "info": P(rrect(1.5, 1.5, 21, 21, 7), BG) + P(circ(12, 6.9, 1.9)) + P(rrect(10.4, 10, 3.2, 8.6, 1.6)),
    # routes: a warp lane, the delta at its end
    "routes": P(circ(3.6, 18.6, 1.7), LO) + P(circ(7.6, 14.8, 1.7), LO) + P(circ(11.4, 11.2, 1.7), LO) + P(delta(17, 7.4, 6.2, math.pi / 4)),
    # pin: the combadge, dim when off and lit when pinned
    "pin": '<ellipse cx="12" cy="13.6" rx="9" ry="5.8"' + BG + '/>' + P(delta(12, 12.2, 8.6), LO),
    "pin_on": '<ellipse cx="12" cy="13.6" rx="9" ry="5.8"' + LO + '/>' + P(delta(12, 12.2, 8.6)),
}
ALIASES = {"up": "delta_up", "down": "delta_down", "arrow_right": "delta_right", "close": "cross", "padd": "orders"}


def svg(body):
    return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="#fff">%s</svg>' % body


def main():
    lines = ["class_name TrekIconSvg",
             "## Generated by remaster/tools/art/build_trek_icons.py (do not edit by hand): the Trek icon set as SVG, drawn in-house.",
             "## White on transparent; UiIcon rasterises each at its exact pixel size and tints it.", "",
             "const ALIASES := {%s}" % ", ".join('"%s": "%s"' % kv for kv in ALIASES.items()), "",
             "const SVG := {"]
    out_dir = os.path.join(GODOT, "..", "docs", "art", "trek-icons")
    os.makedirs(out_dir, exist_ok=True)
    for name, body in ICONS.items():
        s = svg(body)
        assert "'" not in s
        lines.append("\t\"%s\": '%s'," % (name, s))
        with open(os.path.join(out_dir, name + ".svg"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(s + "\n")
    lines.append("}")
    with open(os.path.join(GODOT, "scenes", "ui", "widgets", "trek_icon_svg.gd"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")
    print("wrote", len(ICONS), "icons")


if __name__ == "__main__":
    main()
