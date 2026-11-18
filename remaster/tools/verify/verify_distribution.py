#!/usr/bin/env python3
"""Read-only check of a received web export; emits hashes, not a licence/QA claim.
Run: python verify_distribution.py EXPORT_DIR > distribution-check.json
Requires sizes.json, packs/manifest.json and VERSION.txt. Never downloads or edits.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys

ENGINE = ['index.js', 'index.wasm', 'index.side.wasm', 'index.pck',
          'index.audio.worklet.js', 'index.audio.position.worklet.js']

def safe_file(root, name):
    if not isinstance(name, str) or '\\' in name or ':' in name:
        raise ValueError('unsafe file name')
    p = PurePosixPath(name)
    if p.is_absolute() or not p.parts or '..' in p.parts or p.as_posix() != name:
        raise ValueError('unsafe file name: ' + name)
    candidate = root.joinpath(*p.parts)
    if any(root.joinpath(*p.parts[:i]).is_symlink() for i in range(1, len(p.parts)+1)):
        raise ValueError('symlink not allowed: ' + name)
    if not candidate.is_file():
        raise ValueError('missing file: ' + name)
    return candidate

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()

def audit(directory):
    root = Path(directory).resolve()
    manifest = json.loads(safe_file(root, 'packs/manifest.json').read_text(encoding='utf-8-sig'))
    sizes = json.loads(safe_file(root, 'sizes.json').read_text(encoding='utf-8-sig'))
    version = safe_file(root, 'VERSION.txt').read_text(encoding='utf-8-sig').strip()
    expected = {}
    for row in sizes['files']:
        name, size = row['file'], row['raw']
        if name in expected or type(size) is not int or size < 0:
            raise ValueError('duplicate file or invalid byte size')
        safe_file(root, name)
        expected[name] = size
    for name in ['index.html', *ENGINE, 'libbotf_core.web.template_release.wasm32.wasm']:
        if name not in expected:
            raise ValueError('core file not listed: ' + name)
    actual_packs = list(manifest['files'].values())
    if len(set(actual_packs)) != len(actual_packs) or set(actual_packs) != set(manifest['bytes']):
        raise ValueError('pack mapping and size keys differ or duplicate')
    for logical, name in manifest['files'].items():
        if not re.fullmatch(r'[A-Za-z0-9_]+\.pck', logical):
            raise ValueError('invalid logical pack name')
        if not re.fullmatch(re.escape(logical[:-4])+r'\.[0-9a-f]{12}\.pck', name):
            raise ValueError('invalid content-hashed pack name')
        if expected.get('packs/'+name) != manifest['bytes'][name]:
            raise ValueError('pack missing from sizes or byte sizes disagree: ' + name)
    if {n[6:] for n in expected if n.startswith('packs/')} != set(actual_packs):
        raise ValueError('unmapped pack in sizes')
    files = []
    for name, size in sorted(expected.items()):
        p = safe_file(root, name)
        if p.stat().st_size != size:
            raise ValueError('byte-size mismatch: ' + name)
        sha = digest(p)
        if name.startswith('packs/') and sha[:12] != name.split('.')[-2]:
            raise ValueError('pack hash mismatch: ' + name)
        files.append({'file': name, 'bytes': size, 'sha256': sha})
    h = hashlib.sha256()
    for name in ENGINE + sorted(n for n in expected if n.startswith('lib') and n.endswith('.wasm')):
        h.update(name.encode())
        with safe_file(root, name).open('rb') as f:
            for chunk in iter(lambda: f.read(1024*1024), b''):
                h.update(chunk)
    if h.hexdigest()[:12] != manifest['build']:
        raise ValueError('engine build hash mismatch')
    html = safe_file(root, 'index.html').read_text(encoding='utf-8')
    if '<meta name="botf-build" content="'+manifest['build']+'">' not in html:
        raise ValueError('HTML build marker mismatch')
    return {'status': 'listed-runtime-files-verified', 'version': version,
            'build': manifest['build'], 'runtime_files': len(files),
            'runtime_bytes': sum(f['bytes'] for f in files), 'packs': len(actual_packs),
            'scope': 'Listed runtime bytes only. Not source completeness, rights clearance, gameplay, or an independent trusted checksum comparison. Hosting sidecars/support files are not covered.',
            'files': files}

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('export_directory')
    args = ap.parse_args()
    try:
        result = audit(args.export_directory)
    except (ValueError, KeyError, TypeError, OSError) as exc:
        print(json.dumps({'status': 'failed', 'error': str(exc)}))
        return 1
    print(json.dumps(result, indent=2))
    return 0

if __name__ == '__main__':
    sys.exit(main())
