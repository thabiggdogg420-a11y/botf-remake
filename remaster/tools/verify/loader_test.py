#!/usr/bin/env python3
"""Loader decision test (r3): open the site loader with ?dry=1 in a fresh profile, read BOTF_LOADER from the forwarded console.
  python loader_test.py <chrome|edge|firefox> <port> <run> [extra browser flags...]"""
import os, subprocess, sys, tempfile, time
B = {"chrome": r"C:\Program Files\Google\Chrome\Application\chrome.exe",
     "edge": r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
     "firefox": r"C:\Program Files\Mozilla Firefox\firefox.exe"}
br, port, run = sys.argv[1:4]; extra = sys.argv[4:]
out = os.environ["CAPDIR"]; url = "http://127.0.0.1:%s/index.html?dry=1&run=%s" % (port, run)
prof = tempfile.mkdtemp(prefix="botf-ld-%s-" % br)
if br == "firefox":
    open(os.path.join(prof, "user.js"), "w").write('user_pref("media.volume_scale","0.0");user_pref("browser.shell.checkDefaultBrowser",false);user_pref("datareporting.policy.dataSubmissionEnabled",false);user_pref("toolkit.telemetry.reportingpolicy.firstRun",false);user_pref("browser.aboutwelcome.enabled",false);user_pref("browser.startup.homepage_override.mstone","ignore");user_pref("termsofuse.bypassNotification",true);user_pref("browser.preonboarding.enabled",false);user_pref("datareporting.policy.dataSubmissionPolicyBypassNotification",true);user_pref("trailhead.firstrun.didSeeAboutWelcome",true);\n')
    cmd = [B[br], "-no-remote", "-profile", prof, url]
else:
    cmd = [B[br], "--user-data-dir=" + prof, "--no-first-run", "--no-default-browser-check", "--mute-audio"] + extra + [url]
p = subprocess.Popen(cmd, creationflags=0x00004000)
log = os.path.join(out, run, "console.log"); hit = ""
for _ in range(40):
    time.sleep(1)
    if os.path.isfile(log):
        hit = [l for l in open(log, encoding="utf-8", errors="replace") if "BOTF_LOADER" in l]
        if hit: break
subprocess.run(["taskkill", "/T", "/F", "/PID", str(p.pid)], capture_output=True)
print(run, hit[-1].strip() if hit else "NO DECISION (see %s)" % log)
