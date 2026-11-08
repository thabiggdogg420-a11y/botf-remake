# Gemini (Tuvok / agy) slice ledger

Simon, 8 Nov 2026: use Gemini for small slices of well-briefed work, and switch away from any task type he keeps failing at. The rule: a type with 2 or more failed slices is not sent to Gemini again.

| Date | Slice | Type | Result | Errors found (by whom) |
|---|---|---|---|---|
| 2026-11-08 | L1 parity matrix and vanilla data | research doc, citation-heavy | landed | 41 wrong citations, fixed by his own L2b pass; Odo check pending |
| 2026-11-08 | L2 12 design docs | design doc | landed | 27 wrong citations, fixed in L2b; missed later requirements (expected) |
| 2026-11-08 | L2b browser and photoreal fold-in, citation fix | doc revision | landed | Odo check pending |

Tally by type: citation-heavy docs have 1 slice with substantial errors. Watch this type.

**Odo verification, 8 Nov.** After L2b claimed "zero citation discrepancies remain", 4 of 15 sampled L1 citations and 4 of 10 sampled L2 citations were still wrong or loose. L1 also presented secondhand victory data as "disassembly".
- **Citation-heavy docs:** 2 failures. This type is no longer sent to Gemini. Claude lanes write code citations from now on.
- **Self-reported "verified" claims** from Gemini are not trusted. Every slice gets a Claude or Odo check.
| 2026-11-08 | G1 tutorial opening script | content writing, structured | landed. Claude check: 161 lines; the missing UI hooks are listed honestly | none found |
| 2026-11-08 | G2 hail lines (majors, Dominion, 30 minors) | content writing | landed. Claude check: 30/30 minor ids present, no overlong lines | none found |

Tally: content writing is 2 for 2 clean, so it is a good fit for Gemini.
