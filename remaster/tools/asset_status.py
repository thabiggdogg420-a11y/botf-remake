#!/usr/bin/env python3
"""Print asset counts by status and category from remaster/assets/asset-manifest.json.

  python remaster/tools/asset_status.py                    counts by category x status, plus totals
  python remaster/tools/asset_status.py --milestone M1     only rows due in M1
  python remaster/tools/asset_status.py --category ship    only one category
  python remaster/tools/asset_status.py --ws 10            only one workstream
  python remaster/tools/asset_status.py --used             skip rows marked planned
  python remaster/tools/asset_status.py --list missing     list the keys with that status
  python remaster/tools/asset_status.py --web              add the web download budget (KB) per category
  python remaster/tools/asset_status.py --json             machine-readable output
  python remaster/tools/asset_status.py --check            validate the manifest (exit 1 on problems)

Statuses: procedural, placeholder, final, missing (ART-BIBLE.md section 11). Exit 0 unless --check finds a problem.
"""
import argparse
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT = os.path.join(HERE, '..', 'assets', 'asset-manifest.json')
STATUSES = ['procedural', 'placeholder', 'final', 'missing']
KEY_RE = re.compile(r'^[a-z0-9_]+(\.[A-Za-z0-9_<>]+)+$')


def load(path):
    with open(path, encoding='utf8') as f:
        return json.load(f)


def check(doc):
    problems = []
    seen = set()
    for r in doc['assets']:
        k = r.get('key', '?')
        if k in seen:
            problems.append('duplicate key ' + k)
        seen.add(k)
        for f in ('key', 'category', 'name', 'source', 'status', 'ws', 'milestone', 'desktop_budget', 'web_budget'):
            if f not in r:
                problems.append('%s: missing field %s' % (k, f))
        if r.get('status') not in STATUSES:
            problems.append('%s: bad status %r' % (k, r.get('status')))
        if r.get('milestone') not in ('M1', 'M2', 'M3', 'M4'):
            problems.append('%s: bad milestone %r' % (k, r.get('milestone')))
        if not KEY_RE.match(k):
            problems.append('%s: key does not match the scheme' % k)
    ships = [r for r in doc['assets'] if r['category'] == 'ship']
    if len(ships) != 159:
        problems.append('expected 159 ship rows, found %d' % len(ships))
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--manifest', default=DEFAULT)
    ap.add_argument('--milestone')
    ap.add_argument('--category')
    ap.add_argument('--ws', type=int)
    ap.add_argument('--used', action='store_true', help='skip planned rows')
    ap.add_argument('--list', choices=STATUSES, help='list keys with this status')
    ap.add_argument('--web', action='store_true', help='sum the web download budget')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()

    doc = load(a.manifest)
    if a.check:
        probs = check(doc)
        for p in probs:
            print('PROBLEM:', p)
        print('%d rows, %d problems' % (len(doc['assets']), len(probs)))
        return 1 if probs else 0

    rows = doc['assets']
    if a.milestone:
        rows = [r for r in rows if r['milestone'] == a.milestone]
    if a.category:
        rows = [r for r in rows if r['category'] == a.category]
    if a.ws is not None:
        rows = [r for r in rows if r['ws'] == a.ws]
    if a.used:
        rows = [r for r in rows if not r.get('planned')]

    if a.list:
        for r in rows:
            if r['status'] == a.list:
                print('%-44s %s  ws%-2s %s' % (r['key'], r['milestone'], r['ws'], r['name']))
        return 0

    table = collections.defaultdict(collections.Counter)
    for r in rows:
        table[r['category']][r['status']] += 1
    totals = collections.Counter(r['status'] for r in rows)

    if a.json:
        print(json.dumps({'total': len(rows), 'by_status': dict(totals), 'by_category': {c: dict(v) for c, v in table.items()}}, indent=1))
        return 0

    w = max([len(c) for c in table] + [8])
    print('%-*s %s %s' % (w, 'category', ''.join('%12s' % s for s in STATUSES), '%8s' % 'total'))
    for c in sorted(table):
        print('%-*s %s %8d' % (w, c, ''.join('%12d' % table[c][s] for s in STATUSES), sum(table[c].values())))
    print('%-*s %s %8d' % (w, 'ALL', ''.join('%12d' % totals[s] for s in STATUSES), len(rows)))
    done = totals['final']
    print('\nfinal: %d of %d (%.1f%%).  Not yet final: %d.' % (done, len(rows), 100.0 * done / max(len(rows), 1), len(rows) - done))
    planned = sum(1 for r in rows if r.get('planned'))
    print('planned rows (keys the game does not use yet): %d' % planned)
    if a.web:
        print('\nweb download budget by category (KB, sum of rows that state download_kb; *_max rows excluded):')
        kb = collections.Counter()
        for r in rows:
            kb[r['category']] += r['web_budget'].get('download_kb', 0)
        for c in sorted(kb):
            print('  %-*s %8d KB' % (w, c, kb[c]))
        print('  %-*s %8d KB' % (w, 'ALL', sum(kb.values())))
    return 0


if __name__ == '__main__':
    sys.exit(main())
