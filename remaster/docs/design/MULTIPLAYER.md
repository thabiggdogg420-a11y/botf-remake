# Multiplayer design note (F5, last item)

Outranked by: Simon's words in `AI_stuff\standing\simon\` and `botf-rulings.md`, then MASTER-PLAN.md §7 (decision 4, 8 Nov 2026: private multiplayer, yes) and FINISH-PLAN.md F5 ("Hotseat then LAN multiplayer (private)").

Status: design settled 20 Nov 2026. Simon: "design it appropriately"; the lead made the calls below on that word (see "Decisions"). Build order: desktop hotseat first, then LAN.

## What exists that helps
- The core is deterministic. Every turn ends in a fingerprint (`botf::fingerprint`) that is the same on native and wasm. Saves round-trip exactly and a reloaded campaign continues identically (SoakTests checks both). A pack's id and checksum are in every save and in the fingerprint, so two peers on different packs disagree straight away (DATA-FORMAT.md).
- Orders are already commands (`botf::submit`), validated one by one against the state. The Godot client only talks to the core through `AppSession` (`request`/`request_json`).
- The battle session is already a separate step (`session.active`, the encounter, auto-resolve).

## What stands in the way
- The core has one human. `GameState::playerEmpire` is a single id, used in about 200 places in the core: the AI skips it (rivalry, intel, build planning), victory and defeat are judged from its side (`CampaignOutcome` Victory/Defeat), events are addressed to it, fog and the briefing are its view, and commands are checked as "owned by the player".
- Battles involving the player wait for the player's tactical orders, and there is only one player to wait for.

## Hotseat (first)
Several humans take turns on one PC; the AI plays the rest.
1. Core: `playerEmpire` becomes the empire whose orders are being taken now, plus a new saved set `humanEmpires`. Everything that means "skip the AI" checks the set; everything that means "whose view" checks the current empire. The outcome gains a winner per campaign (already `winnerEmpire`); Victory/Defeat becomes per viewer. This is a schema bump with migration (one-human saves load as a one-element set).
2. Turn flow: each human in seating order plans their turn (orders are submitted under their id), then one `advance` resolves everyone together, so the simultaneous-turn rules are unchanged. Order between humans has no effect on the result.
3. Hand-over screen: a full-screen "Klingon Empire, take the chair" card between humans, so nobody sees another's map, briefing or diplomacy. The briefing and events are kept per human.
4. Battles: a battle between two humans is planned by each in turn on the same WEGO plan screen (the other one looks away at the hand-over card); human against AI as now.
5. Saves: one file holds the whole hotseat game; loading asks nobody for a password (private, one PC).

## LAN (second)
Each human on their own PC, same rules as hotseat.
1. Model: lockstep on commands, not state sync. The host collects each player's command list for the turn, sends the full set to every peer, and each peer runs the same `advance`. After the turn every peer sends its fingerprint; a mismatch stops the game with "desync on turn N" and the host's save goes to everyone.
2. Joining: the host sends its save (and the pack ids and checksums); a peer missing a pack cannot join. Direct IP on the local network, plus a broadcast "find games" on the LAN. No internet relay, no accounts, no matchmaking: private, as Simon set it.
3. Transport: Godot's ENetMultiplayerPeer on the desktop build. The browser build cannot open raw sockets; LAN play there would need WebRTC or a WebSocket host and is not in this step.
4. Battles: both humans plan on their own PC; the host waits for both plans, then everyone resolves the same battle.
5. Timer and drop-outs: an optional turn timer; a player who drops is played by the AI until they rejoin from the host's latest save.

## Order of work
1. Core: `humanEmpires`, per-viewer outcome and views, schema bump and migration; every existing suite and the turn-20 fingerprint unchanged for one-human games.
2. Hotseat in the client: setup (choose which seats are human), hand-over card, per-human briefing, saves. Proof: a three-human hotseat campaign played to a victory in a real window, with the hand-over shown on every turn.
3. LAN: lockstep host and client, fingerprint check each turn, join from a save. Proof: two instances on this PC (one host, one client) playing 20 turns with matching fingerprints, then a forced desync caught.

## Decisions (20 Nov 2026; Simon "design it appropriately", calls by the lead)
1. Allies may share a diplomatic victory. When the winning bloc holds two or more humans, every human in it wins (the outcome screen names the bloc); humans outside it lose. A human may still win alone by any other condition. The diplomatic win must hold for 10 turns in a row (P1, Simon's yes the same day).
2. Desktop hotseat first, then LAN, both lockstep on commands with a fingerprint check after every turn (above). Browser online play is a later, separate job (it needs WebRTC or a relay) and is not part of this one.
3. The turn timer is off by default. The host can set one (per-turn seconds) in the game setup; when it runs out, that human's turn ends with the orders already given.

## Hotseat build (20 Nov 2026, Barclay): first cut
Built (core, bridge, Godot client):
- Core: `GameState::hotseatHumans` (every human, ascending; saved as the optional HOTSEAT section, schema 25), `setHotseat`,
  `switchSeat` (applies the seated human's queued orders, then swaps that empire into the chair) and `humanSeat`. Every AI
  planner (orders, builds, research, war, diplomacy proposals and answers, calls to arms, intelligence, stations, galaxy
  features, megaprojects) skips all humans. A human who loses every world is out; the chair passes on and the game goes on
  for the others. Any human's victory is a human victory (`winner` names who). Suite `BotfHotseatTests`: validation, the
  save round-trip, orders applied on hand-over, the AI leaving both humans alone for eleven turns, determinism, and back to
  one player.
- Bridge: `setHotseat {humans}`, `switchSeat {empire}`, `meta.hotseat`; every human gets the turn's briefing from its own seat.
- Client: "Hotseat players" on the New campaign screen; End turn hands the chair to the next human behind a full-screen
  hand-over card (Enter on the card takes the chair; it never ends the hidden turn); after the last human the turn
  resolves, and the next turn opens with the first human behind the card. Test: test_campaign_ui `_hotseat`.

Not yet (next steps, in this order):
1. Battles: a battle involving the human in the chair at the end of the turn (the last one) is fought on the tactical
   screen; a battle involving any other human is resolved by the tactical AI. Planned: a hand-over to each human in a battle.
2. A galactic event choice or a conquered world's fate for a human not in the chair is decided by the AI's rule.
3. The shared diplomatic win is shown as the winner's victory; the end screen does not yet name the allied humans.
4. Turn timer (off by default, host-set) and LAN lockstep are not started.

## Round 2 (21 Nov 2026, Barclay): hotseat battles, the shared win named, LAN lockstep
### Hotseat
- Battles: a battle of a human who is not in the chair now waits for that human instead of being fought by the tactical AI.
  Core: `resolveTacticalBattles` and the threat battles route every human's encounter to the session; when the chair is not
  in it, the encounter's controller is the human who is. `session::order` accepts any human's ship (the controller is that
  ship's owner for the order and is put back afterwards); the replay check replays every human's recorded orders. Bridge:
  `session.battleHumans` (the humans with ships in the pending battle), `session.battleSeat` (who commands it now) and
  `battleSeat {empire}`; the tactical view, "all ships" orders and group orders follow the battle seat. Client: the battle
  screen opens behind the hand-over card for that human. In a battle between two humans each plans the round in turn behind
  the card (the first human's Resolve hands the round over; the second's resolves it), both watch it, and the next round
  starts again with the first. One-player games are unchanged (the same encounters pause, the same replay).
  One edge, written down rather than changed: the tactical rules let "the AI side" accept a hail truce; in a two-human
  battle the side that is not the encounter's controller is judged by that rule when every ship of the other side hails.
- The shared diplomatic win: the bridge's `meta.winners` lists the humans who share the win (the winner if human; for a
  diplomatic win every human in the winner's full alliance). The end screen judges the human in the chair by it and names
  every allied human ("Shared victory: Federation, Klingon Empire, allied players."). Decision 1's "two or more humans in
  the winning bloc all win" also holds under an AI's lead (Victory.cpp, hotseat games only).
- Tests: BotfHotseatTests (27 checks: the battle of a human not in the chair waits, they order only their own ships, the
  turn commits; a two-human battle with both humans' orders in one round replays and commits; one-player and headless
  routes unchanged). Frames: `godot/scenes/ui/tools/hotseat_battle_capture.gd`.

### LAN (desktop)
- `godot/scripts/lan_session.gd` (LanSession, a child of App) and the "LAN game" modal (`scenes/ui/screens/lan_lobby.gd`,
  Title > LAN game): host (port 47474 by default, the host's power, turn timer, 0 = off, the default) or join by address,
  or "Find games" (UDP broadcast on port + 1). Transport: ENetMultiplayerPeer, JSON messages on one reliable channel. A
  joining peer with a different protocol or content pack checksum is refused.
- The game is a hotseat game where each LAN player is one of the humans; every peer holds the host's save of the turn's
  start. The core journals each order the seat gives (bridge `journal`: the accepted `submit`/`cancel` calls with their
  ids). End turn sends the journal to the host; with every human's in (or a dropped player counted as empty) the host sends
  the full set; each peer reloads the turn-start save, `lanApply` replays every seat's journal in seating order (switchSeat
  between seats, submit ids renumbered, a refused order skipped the same way everywhere), ends the turn with battles fought
  by the tactical AI (the same on every peer), and sends its fingerprint. Matching: the turn stands. Different: "DESYNC ON
  TURN N" with every peer's fingerprint, the report saved to `user://lan/desync-turn-N-<host|client>.txt`, and the host's
  save goes to everyone; play continues from it.
- Turn timer: off by default; the host sets seconds per turn; when they run out, that human's turn ends with the orders
  already given. A strip under the header shows host/client, the turn, planning or waiting, the seconds left and the last
  check.
- Proof (21 Nov, two windowed instances on this PC over 127.0.0.1, off-screen): 13 turns, every fingerprint matched on
  both peers (4 orders replayed a turn, none refused); a forced desync (the client replays one extra order on one turn) was
  caught on that turn, the host's state went out and the next turns matched again (the same fingerprints as the clean run);
  with a 6 s timer and a client that takes 20 s to plan, the client's turns ended on the timer and the fingerprints matched.
  Driver: `BOTF_LAN_AUTO` (lan_session.gd, test only), runner `Documents\ChatGPT\r6-lan\lan_run.ps1`.

Not yet:
1. LAN battles are fought by the tactical AI on every peer; planning them on each human's PC (design point LAN 4) is next.
2. A dropped player's seat plans nothing until the end of the game (the design says the AI plays it until they rejoin from
   the host's latest save); rejoining is not built.
3. The host starts a new campaign from the lobby (default galaxy, the host's power, a fixed seed in tests); starting LAN from
   a saved game or the full New campaign setup is not built.
