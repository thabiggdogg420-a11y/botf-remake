# F6 extras: real-time mode and helm mode

Outranked by: Simon's words in `AI_stuff\standing\simon\` and `botf-rulings.md`, then MASTER-PLAN.md decision 5 (combat is "a modernized, updated, smartly designed 3d turn based space battle"; a real-time mode and a Bridge-Commander-style helm mode stay parked as a later option) and FINISH-PLAN.md F6. Combat rules changes go to Simon (8 Nov 2026).

Barclay, 20 Nov 2026. Status: first build in the combat screen, waiting for Simon's verdict (the F6 exit gate).

## The line this build holds
Both modes are ways of giving orders. Neither changes a rule. The core still resolves the same WEGO rounds (tactical rules 4: 12 ticks, simultaneous movement, facings, subsystems), with the same orders, in the same order. A battle fought in real time or from the helm gives exactly the result the same orders give in the plan phase. A true continuous-time simulation, or free flight outside the core's maneuvers, would be a rules change and is not in this build; it waits for Simon.

## Real-time mode
- The Real-time toggle on the combat header (or R) starts it. From the plan phase the first round resolves at once.
- When a round's playback ends, the next round resolves straight away with the standing orders: the last maneuver goes again for the current scope (all ships, group or ship), as the existing "closing in" chain already did for up to four idle rounds. There is no plan phase between rounds and the panels stay folded, so the battle runs on like a real-time battle.
- Orders while a round plays: keys 1 to 0 give the maneuvers in the orders-bar order (Hail and Ram stay on the bar, as they need care), Tab cycles the target. An order is sent to the core at once and counts from the next round, exactly as a plan-phase order does.
- Space asks for a pause: the round playing finishes, then real time stops and the plan phase opens. Space in the plan phase (or the toggle, or R) turns it off.
- A strip at the foot of the battle view shows the mode, the standing order, the target and the keys.
- The battle's end stops it.

## Helm mode
- Take the helm on the combat header (or H) with one of your ships selected (not a station). Real time turns on with it.
- The camera follows that ship (the battle camera's ship mode, the same one F gives), and the order keys and Tab apply to that ship alone (scope "ship"). The rest of your ships keep their own orders and their captains.
- The strip shows HELM, the ship's name and hull.
- H (or Release helm) gives the ship back. The helm is released by itself when the ship is destroyed, disabled or withdraws, and when real time stops.

## Proof
- test_campaign_ui.gd `_f6_realtime`: the two header controls, the helm taking the selected ship and turning real time on, the strip, a round resolving at a round's end with no plan phase, a number key giving the matching maneuver, and Space stopping real time after the round.
- Frames: a battle drill in a real window in real time and at the helm (captures beside the round's report).

## Next, for Simon's verdict
1. Is "rounds that run on with standing orders" the real-time mode he wants, or does he want a continuous simulation (a rules change)?
2. The helm: is following one ship with order keys enough, or does he want direct heading and speed control (Bridge Commander style free flight, a rules change)?
