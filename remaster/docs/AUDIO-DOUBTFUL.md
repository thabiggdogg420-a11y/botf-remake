# Audio doubtful list (auto-check, 17 Nov 2026)

For Simon to listen to. Sources: `docs/AUDIO-LISTEN-SHEET.md` (auto-check column has every row). Method: whisper large-v3 transcript against the line in the sheet, plus signal checks. It cannot hear music under an effect, so a `pass` on an effect row means no words, not silent, not cut, nothing more.

Totals for the 245 untested rows: 186 pass, 44 doubtful, 15 fail (4 TOS ack clips trimmed and re-heard as pass on 17 Nov).

13 of the 15 fails are in no cue, so they cost nothing today; the 2 wired ones were the two TOS "in a moment" clips, now dropped from the cues.

## Fail (listen first, probably remove or re-cut)

- `ds9/impact/impact_s06e05_1298.wav` dialogue over the effect: 'No, I'm not!'. Cue: not in any cue.
- `ds9/voice/fed_energize_s01e06_2476.wav` says "Energize."; no intelligible speech (silence or buried under noise). Cue: not in any cue.
- `ent/explosion/explosion_s04e02_1775.wav` dialogue over the effect: 'Too much of your children.'; starts mid-sound (first 100 ms already loud): start clipped. Cue: not in any cue.
- `ent/impact/impact_s02e01_189.wav` dialogue over the effect: 'I've got faith of the heart.'. Cue: not in any cue.
- `tng/explosion/explosion_s02e16_1870.wav` dialogue over the effect: 'Captain's log supplemental. We have been attacked without pr'. Cue: not in any cue.
- `tng/explosion/explosion_s03e15_1819.wav` dialogue over the effect: 'KILL HER NOW! FED ALERT!'. Cue: not in any cue.
- `tng/impact/impact_s03e10_689.wav` dialogue over the effect: 'And I'll see you in the next one.'. Cue: not in any cue.
- `tng/impact/impact_s03e15_2533.wav` dialogue over the effect: 'The Forza Bridge! I can't hold that!'. Cue: not in any cue.
- `tng/impact/impact_s03e26_4221.wav` dialogue over the effect: 'Data.'; starts mid-sound (first 100 ms already loud): start clipped. Cue: not in any cue.
- `tng/impact/impact_s04e26_654.wav` dialogue over the effect: 'I do not laugh because I do not feel.'; still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tng/impact/impact_s05e01_2320.wav` dialogue over the effect: 'defeat how where are the Romulans'. Cue: not in any cue.
- `tng/launch/launch_s03e26_4190.wav` dialogue over the effect: 'Worf, Klingon species.'; starts mid-sound (first 100 ms already loud): start clipped. Cue: not in any cue.
- `tng/launch/launch_s03e26_4223.wav` dialogue over the effect: 'primitive arti-'. Cue: not in any cue.
- `tos/voice/fed_ack_s01e14_2226.wav` says "Yes, sir."; wrong words: heard 'in a moment.' (padded 'In a moment', prompted 'In a moment, yes'). Cue: vo.combat.ack.fed.tos. Dropped from the cue 17 Nov 2026.
- `tos/voice/kli_ack_s01e14_2226.wav` says "Yes, sir."; wrong words: heard 'in a moment.' (padded 'In a moment', prompted 'In a moment, yes'). Cue: vo.combat.ack.kli.tos. Dropped from the cue 17 Nov 2026.

## Doubtful voice lines

- `ds9/voice/fed_ack_s01e05_883.wav` says "Aye, sir."; heard 'Hi, sir.' (padded 'Hi, sir.'); matches the line only when whisper is told what to expect. Cue: vo.combat.ack.fed.ds9.
- `ds9/voice/fed_hull_breach_s01e01_107.wav` says "Direct hit..."; heard 'Dora!' (padded 'Dora!'); matches the line only when whisper is told what to expect; dry only +4 dB: music or effects under the line. Cue: vo.combat.underattack.fed.ds9.
- `ds9/voice/fed_shields_up_s01e12_581.wav` says "Shields up."; extra words 'reading a math' around the line (heard 'shields up reading a math'): other voice or a name; first 20 ms at full level: start may be clipped. Cue: vo.combat.shieldsup.fed.ds9.
- `tng/voice/fed_ack_s02e20_554.wav` says "Aye, sir."; heard 'I do.' (padded 'I sure'); matches the line only when whisper is told what to expect. Cue: vo.combat.ack.fed.tng.
- `tng/voice/fed_all_stop_s02e13_2043.wav` says "All stop."; heard 'I'll stop.' (padded 'Oh stop!'); matches the line only when whisper is told what to expect. Cue: vo.combat.allstop.fed.tng.
- `tng/voice/fed_fire_s02e02_1191.wav` says "Fire torpedoes."; heard 'Bye torpedoes.' (padded 'Bye Torpedoes!'); matches the line only when whisper is told what to expect. Cue: vo.combat.phaser.fed.tng.
- `tng/voice/fed_fire_s02e16_1605.wav` says "Fire."; heard 'Bye.' (padded 'Ready? Fire!'); matches the line only when whisper is told what to expect; first 20 ms at full level: start may be clipped. Cue: vo.combat.phaser.fed.tng.
- `tng/voice/fed_fire_s02e16_2261.wav` says "Fire."; heard 'Bye.' (padded 'Bye.'); matches the line only when whisper is told what to expect. Cue: vo.combat.phaser.fed.tng.
- `tng/voice/kli_fire_s02e16_1605.wav` says "Fire."; heard 'Bye.' (padded 'Forty-five.'); matches the line only when whisper is told what to expect; first 20 ms at full level: start may be clipped. Cue: vo.combat.phaser.kli.tng.
- `tng/voice/kli_fire_s02e16_2261.wav` says "Fire."; heard 'Bye.' (padded 'Bye.'); matches the line only when whisper is told what to expect. Cue: vo.combat.phaser.kli.tng.
- `tos/voice/fed_all_stop_s01e16_1470.wav` says "Cease fire."; first 20 ms at full level: start may be clipped; dry only -2 dB: music or effects under the line; line is 'Cease fire.', class is all stop. Cue: vo.combat.allstop.fed.tos.
- `tos/voice/fed_battle_stations_s01e10_1019.wav` says "Battle stations."; heard 'yourself' (padded 'Yourself'); matches the line only when whisper is told what to expect. Cue: vo.combat.encounter.fed.tos.
- `tos/voice/fed_battle_stations_s01e10_909.wav` says "Battle stations."; extra words 'alert' around the line (heard 'Alert! Battle stations!'): other voice or a name. Cue: vo.combat.encounter.fed.tos.
- `tos/voice/fed_energize_s01e09_40.wav` says "Energize."; first 20 ms at full level: start may be clipped; ends loud: end may be clipped. Cue: not in any cue.
- `tos/voice/fed_engage_s01e04_2748.wav` says "Engage."; heard 'and get' (padded 'And get.'); matches the line only when whisper is told what to expect; ends loud: end may be clipped. Cue: vo.combat.engage.fed.tos.
- `tos/voice/fed_engage_s01e12_2703.wav` says "Engage."; heard 'Thank you.' (padded 'I'm gay.'); matches the line only when whisper is told what to expect. Cue: vo.combat.engage.fed.tos.
- `tos/voice/fed_fire_s01e21_1048.wav` says "Fire."; extra words 'whitefield' around the line (heard 'Whitefield, fire.'): other voice or a name; first 20 ms at full level: start may be clipped. Cue: vo.combat.phaser.fed.tos.
- `tos/voice/kli_fire_s01e21_1048.wav` says "Fire."; extra words 'whitefield' around the line (heard 'Whitefield, fire.'): other voice or a name; first 20 ms at full level: start may be clipped. Cue: vo.combat.phaser.kli.tos.
- `voy/voice/fed_ack_s01e09_2038.wav` says "Aye, Captain."; heard 'I kept it.' (padded 'Yeah, I kept it and'); matches the line only when whisper is told what to expect. Cue: vo.combat.ack.fed.voy.
- `voy/voice/fed_all_stop_s02e02_1388.wav` says "All stop."; heard 'I'll stop.' (padded 'I'll stop'); matches the line only when whisper is told what to expect. Cue: vo.combat.allstop.fed.voy.
- `voy/voice/fed_all_stop_s02e08_1624.wav` says "All stop."; heard 'I'll stop' (padded 'I'll stop.'); matches the line only when whisper is told what to expect. Cue: vo.combat.allstop.fed.voy.
- `voy/voice/fed_fire_s01e01_5131.wav` says "Fire."; dry only +4 dB: music or effects under the line. Cue: vo.combat.phaser.fed.voy.
- `voy/voice/fed_shields_down_s01e16_1549.wav` says "Shields are buckling."; heard 'Shields are' (padded 'Shields are'); matches the line only when whisper is told what to expect. Cue: vo.combat.shields.50.fed.voy, vo.combat.shields.25.fed.voy.
- `voy/voice/kli_fire_s01e01_5131.wav` says "Fire."; dry only +4 dB: music or effects under the line. Cue: vo.combat.phaser.kli.voy.

## Doubtful effects (start or end clipped)

- `ds9/explosion/explosion_s05e26_1974.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `ds9/explosion/explosion_s06e05_2148.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `ent/explosion/explosion_s02e01_872.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `ent/explosion/explosion_s03e24_1199.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tng/explosion/explosion_s02e16_1272.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tng/explosion/explosion_s03e10_1323.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tng/explosion/explosion_s03e10_2585.wav` starts mid-sound (first 100 ms already loud): start clipped; still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tng/explosion/explosion_s03e26_1049.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tng/explosion/explosion_s03e26_1121.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tng/explosion/explosion_s04e26_1732.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tng/explosion/explosion_s05e01_2387.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tng/impact/impact_s04e26_1191.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tos/explosion/explosion_s02e06_1496.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `tos/impact/impact_s01e10_1866.wav` starts mid-sound (first 100 ms already loud): start clipped. Cue: not in any cue.
- `tos/impact/impact_s02e06_1452.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `voy/explosion/explosion_s03e26_1301.wav` starts mid-sound (first 100 ms already loud): start clipped. Cue: not in any cue.
- `voy/explosion/explosion_s04e09_298.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `voy/impact/impact_s04e01_81.wav` still loud at the end (last 100 ms): end clipped. Cue: not in any cue.
- `voy/launch/launch_s04e01_1893.wav` starts mid-sound (first 100 ms already loud): start clipped. Cue: not in any cue.
- `voy/launch/launch_s04e08_464.wav` starts mid-sound (first 100 ms already loud): start clipped. Cue: not in any cue.

## Notes
- Every `kli_ack` and `kli_fire` clip in TOS, TNG and VOY is the same audio as a Federation line (same source time), so the Klingon cues speak Federation voices; DS9 `kli_ack_s01e17_1174` ("As you wish.") is the only distinct one that passed. A design point, not an audio fault.
- The 132 clips in `trek/fx` are not on the sheet. They were run through the same checks (words, silence): all 132 clean.
- "Padded" means the clip re-run with 0.5 s of silence either side; "told what to expect" means a second run primed with the sheet line, which proves nothing alone, so those rows stay doubtful.
