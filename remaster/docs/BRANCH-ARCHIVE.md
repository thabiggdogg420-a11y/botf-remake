# Branch archive (26 Nov 2026)

Simon, 26 Nov 2026: "Ok to tidy up" (standing/simon/botf-rulings.md). `main` is now the default branch and the place work goes. Every other branch was tagged `archive/<branch-name>` at the SHA below, the tags pushed to origin, and the branch removed locally and on origin. To get one back: `git switch -c <name> archive/<name>`.

`claude/serene-archimedes-nvtcnw` was the default branch until `main` took over; its tip was 71fdc09 (0.1.597), and `main` carries that plus the CI repair (see FINISH-PLAN.md, CI repair 26 Nov).

| Branch | Local | origin | SHA at tip | Tag |
|---|---|---|---|---|
| art/portraits-look | yes | - | 35fc0ecbe28577a8bbe7bc8be2dc08463f2c2f74 | archive/art/portraits-look |
| claude/botf-ai-war-food | yes | - | 9a617d952347cdff0f78fcf3cccf6eaa1e168e0e | archive/claude/botf-ai-war-food |
| claude/f1-audit-fixes | yes | - | 4adba56935989a2f8604bb2d6e2169450f5b804f | archive/claude/f1-audit-fixes |
| claude/hopeful-franklin-4psgy7 | - | yes | 0e51bc9a05d2a1b749db262465920a54dca5061c | archive/claude/hopeful-franklin-4psgy7 |
| claude/pictures-ui | yes | - | ea7795a0a6f8d43e20283c7c02fd0b9c98e4dafc | archive/claude/pictures-ui |
| claude/serene-archimedes-nvtcnw | yes | yes | 71fdc097c73789b0938de32af2ca15df365e3d37 | archive/claude/serene-archimedes-nvtcnw |
| claude/ui-redesign | - | yes | 6491a1fbfc11f6ccf064388c8a83d1939d6849a1 | archive/claude/ui-redesign |
| f0/importcrash | yes | - | b0cc8ec14ee05031b2e6bc9cdcda5b515c1a145c | archive/f0/importcrash |
| f1/browser | yes | - | 4d1bff6be2711f33f9a9e87079b1e3b75332b74e | archive/f1/browser |
| f2-beamrange | yes | - | 3a6174c8e4128269f5805fd5fd106699bc0327ab | archive/f2-beamrange |
| fix/art-late-pack | yes | - | 209c2497c14168dc36d975220a0df1d3a4213d85 | archive/fix/art-late-pack |
| fix/territory-line | yes | - | 26f71da4e2f5c57468eb2543f8419053e192054c | archive/fix/territory-line |
| godot/r1-sound | yes | - | 6575571364dd17b10b53ee0594c053dbee19c02a | archive/godot/r1-sound |
| godot/r2-gameplay | yes | - | 32b2049a7af7c8faac9ec29d8a11cac796b166f0 | archive/godot/r2-gameplay |
| godot/r3-base | yes | - | f80805edc5221ebdf61e5378811a7e5a888faf86 | archive/godot/r3-base |
| godot/r3-present | yes | - | 21995aabb8110706f339500ee9217830abe01ab9 | archive/godot/r3-present |
| godot/r35 | yes | - | 02fcfbc0ab5a7ed0b180dd863d1b78842fa11547 | archive/godot/r35 |
| lane/hull-look | yes | yes | 29fc92668e5a95f413af4510da70ac073da901b6 | archive/lane/hull-look |
| lane/mobile-ma | yes | - | e8b7b4e41c1c4ebb06e895ae3525f81954260328 | archive/lane/mobile-ma |
| lane/onboarding | yes | - | a99a33967ff4686c7c345620052084fc310b8cdb | archive/lane/onboarding |
| m0/art1 | yes | - | 2eb523ef00412a0183db676195665f6406bac23a | archive/m0/art1 |
| m0/build | yes | - | fb9c88ba8f94a7d43df2f8da4db72e50cc53691f | archive/m0/build |
| m0/data | yes | - | bca18e5b6b46c12c1bdaf8a88e9029e28ebe7e9f | archive/m0/data |
| m0/data2 | yes | - | 9fe0a0d2606c1d038669ae256d17c97026cea0cb | archive/m0/data2 |
| m0/galaxy3d | yes | - | 65d6b9969a423365906aabab3a5da7f80b1d5288 | archive/m0/galaxy3d |
| m0/m0-arch | yes | - | 3b78f52de140939617f2164d40680b56cd55e6a5 | archive/m0/m0-arch |
| m1/art2 | yes | - | 3bb702124f66c5b1621904d616a31df2acacf8dd | archive/m1/art2 |
| m1/audio | yes | - | 9f6dd78918611cbdb416c835b80feb10bf27c9af | archive/m1/audio |
| m1/battlechoreo | yes | - | 70405c1107fdf41e8fcdd4918abcffe61a7a2ee1 | archive/m1/battlechoreo |
| m1/battleplay | yes | - | cf98d088d9f531cd44cba669795051d76226425e | archive/m1/battleplay |
| m1/betafix | yes | - | acc382d060c188ecc5d0887d128f0017381a4b0c | archive/m1/betafix |
| m1/combat | yes | - | 227c6ec34e6b023f2b143ddad7d2e1e883f90e88 | archive/m1/combat |
| m1/galaxy | yes | - | 3daab9a296e3ae7dc440088dc7cc24be52cc2415 | archive/m1/galaxy |
| m1/gen | yes | - | 3616195edad937a26a145697bf6849e01a44547c | archive/m1/gen |
| m1/hullhd | yes | - | f4b80be2887fbe8c0700f9bdd6f29046bc0b76ad | archive/m1/hullhd |
| m1/int | yes | - | ed5968b3de93a42220f576885ebd1ce03864b235 | archive/m1/int |
| m1/planets | yes | - | 79e2f0212aa130415816ce10895f7db9b28b4b06 | archive/m1/planets |
| m1/polish | yes | - | 31af0f9866c755d47f33e1e4a605024badcbc231 | archive/m1/polish |
| m1/trekicons | yes | - | 7a881bd0d6e75429328c1a477de106ef611c0020 | archive/m1/trekicons |
| m1/trektext | yes | - | f45e396977b0459a22b7099f6aa5e4cc18c0bcf6 | archive/m1/trektext |
| m1/ui | yes | - | fbe391501de8d31626cd91bd6b8feba4d0cecbb3 | archive/m1/ui |
| m1/weaponfx | yes | - | 6ef6f9dae23c9caa8431dc255d2f3bad63fccfab | archive/m1/weaponfx |
| m1/webpack | yes | - | 250da2a69acbfea5854b5b0a7dbbbb71e1d5e690 | archive/m1/webpack |
| m1/webparity | yes | - | c076d6fcc1c9cd83c2ba3cf1b6b5f9ff85298213 | archive/m1/webparity |
| m1/wego2 | yes | - | 1014226d9c9f6cac06ee4451cd9774d5ebb0633a | archive/m1/wego2 |
| m2/empire | yes | - | 102fcdf9cf286c844b1ef082ea878a549f7a5328 | archive/m2/empire |
| m2/gen | yes | - | a5d5d597dc91c0a8b0747d0ef82edcc9b4db261c | archive/m2/gen |
| m3/balance | yes | - | 6afcb10c1d92b432ce42c8780a16e77f45afdad9 | archive/m3/balance |
| m3/dominion | yes | - | b2972d45d52021e229220499372576357ea4ec1c | archive/m3/dominion |
| m3/economy | yes | - | 30e2a1c870349b85af4307f170b3ca32dc6ba028 | archive/m3/economy |
| m3/eras | yes | - | 7ba494ce981212906a5cfb558952c9809e83e889 | archive/m3/eras |
| m3/modtools | yes | - | 0676dc3afc5a4c0bb50597ae7fb8b57ce26d0d48 | archive/m3/modtools |
| m3/presets | yes | - | 3a6458affe28bd1295a53aceee9f37c78c0a0a5c | archive/m3/presets |
| m3/scenarios | yes | - | 08a77eb916bfca52244fc408c371650efd2aabff | archive/m3/scenarios |
| m3/slots | yes | - | 545e9e25cb777d816680b8aca98b9f5105b941f9 | archive/m3/slots |
| m3/threats | yes | - | 65d4177bb5f10dafc06499846686cbc095ac0ca1 | archive/m3/threats |
| m4/audio | yes | - | 779cf3aa204c4c30e9b401498cda10f01df48f78 | archive/m4/audio |
| m4/combat | yes | - | 6524649bd6171ffb32d9cb86d7331bfc53c24e2a | archive/m4/combat |
| m4/combat2 | yes | - | 8ab7a9ab3a11a1db95e351f3dfad53d9181c6dad | archive/m4/combat2 |
| m4/crashfix | yes | - | 6d63bc740be419e23a8c916071ac1b45f3207a56 | archive/m4/crashfix |
| m4/flight | yes | - | 6b084a27afc85bc42d4f55f7773877edb4bd72b8 | archive/m4/flight |
| m4/trekify | yes | - | 556c8ac847d072f4bffa4e8371b80e7d3ed8a79e | archive/m4/trekify |
| master | yes | yes | 77eebd68170f854ff05ca27bfa24e96930176a72 | archive/master |
| webgpu-trial | yes | yes | 718cc581d0ab7956cbd727fb618f0b2b5d091921 | archive/webgpu-trial |
| webgpu-trial-r2-wip | - | yes | 70bdd3c4953395beacbd2dbcaf1975a5d1b7d136 | archive/webgpu-trial-r2-wip |
