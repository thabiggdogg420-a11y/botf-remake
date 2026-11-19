# Multiplayer design note (F5, last item)

Outranked by: Simon's words in `AI_stuff\standing\simon\` and `botf-rulings.md`, then MASTER-PLAN.md §7 (decision 4, 8 Nov 2026: private multiplayer, yes) and FINISH-PLAN.md F5 ("Hotseat then LAN multiplayer (private)").

Status: design only, nothing built (Barclay, 20 Nov 2026). The build starts when Simon has read this. Questions for him are at the end.

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

## Questions for Simon
1. Hotseat: should two humans be allowed in the same alliance and share a diplomatic victory, or must every human win alone?
2. LAN: desktop only (as above), or is browser-to-browser play over the internet wanted later? That would need a relay and is a different job.
3. Any turn timer by default, or off unless the host sets one?
