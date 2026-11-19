"""Guard for the player's real saves and settings during a test or verify run (F6 round: a test overwrote Simon's
Autosave 2 and 3). `snapshot <file>` records a fingerprint of user://saves and user://botf_settings.json; `check <file>`
compares against it and exits 1, naming every changed, added or removed file, when anything differs.
    python real_saves_guard.py snapshot <state.json> [--user-dir DIR]
    python real_saves_guard.py check <state.json> [--user-dir DIR]
The user dir defaults to %APPDATA%/Godot/app_userdata/BOTF Remastered (Godot) (Linux: ~/.local/share/godot/...)."""
import hashlib
import json
import os
import sys

NAME = "BOTF Remastered (Godot)"


def user_dir(argv):
    if "--user-dir" in argv:
        return argv[argv.index("--user-dir") + 1]
    if os.name == "nt":
        return os.path.join(os.environ["APPDATA"], "Godot", "app_userdata", NAME)
    return os.path.join(os.path.expanduser("~/.local/share/godot/app_userdata"), NAME)


def fingerprint(root):
    out = {}
    paths = [os.path.join(root, "botf_settings.json")]
    saves = os.path.join(root, "saves")
    if os.path.isdir(saves):
        for dp, _dn, fns in os.walk(saves):
            paths += [os.path.join(dp, f) for f in fns]
    for p in paths:
        if os.path.isfile(p):
            with open(p, "rb") as f:
                out[os.path.relpath(p, root).replace(os.sep, "/")] = hashlib.sha256(f.read()).hexdigest()
    return out


def main(argv):
    if len(argv) < 3 or argv[1] not in ("snapshot", "check"):
        print(__doc__)
        return 2
    root, state = user_dir(argv), argv[2]
    now = fingerprint(root)
    if argv[1] == "snapshot":
        with open(state, "w", encoding="utf-8") as f:
            json.dump(now, f, indent=1)
        print("real-saves guard: %d files fingerprinted in %s" % (len(now), root))
        return 0
    with open(state, encoding="utf-8") as f:
        before = json.load(f)
    bad = sorted({k for k in before if now.get(k) != before[k]} | {k for k in now if k not in before})
    if bad:
        print("real-saves guard: FAIL, the player's files changed during the run: " + ", ".join(
            "%s (%s)" % (k, "removed" if k not in now else "added" if k not in before else "changed") for k in bad))
        return 1
    print("real-saves guard: ok, %d files unchanged" % len(now))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
