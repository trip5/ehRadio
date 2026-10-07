#!/usr/bin/env python3
"""
add-builds.py

Add or refresh firmware builds in the latest GitHub release, then carry the
same files over to the dev branch. Steps:

  STAGE 0  PRECHECK   confirm main branch, that fix_releases.py was run, and
                      collect the changed contributor folders
  STAGE 1  COMMIT     commit builds/ on main
  STAGE 2  PUSH main  push main to origin
  STAGE 3  RUN        trigger and watch the Update Builds in Release workflow
  STAGE 4  MERGE      copy the commit's files into dev and push dev
  STAGE 5  FINALIZE   return to dev and remove the lock file

Progress is stored in builds/add-builds.lck so a Ctrl-C can be resumed by
re-running the script. Pass --reset to discard the saved state.

Run from anywhere:
  python builds/add-builds.py
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
LCK_PATH = REPO_ROOT / "builds" / "add-builds.lck"
FIRMWARE_INFO = "builds/releases/web_assets/firmware-info.json"
WORKFLOW_NAME = "update-builds-in-release.yml"
WORKFLOW_DISPLAY = "Update Builds in Release"

MAIN_BRANCH = "main"
DEV_BRANCH = "dev"

STAGE_NAMES = {
    0: "PRECHECK",
    1: "COMMIT on main",
    2: "PUSH main",
    3: "RUN WORKFLOW",
    4: "MERGE into dev",
    5: "FINALIZE",
}


def run(cmd, check=True, capture=True):
    result = subprocess.run(cmd, cwd=REPO_ROOT, text=True, capture_output=capture)
    if check and result.returncode != 0:
        out = ((result.stdout or "") + (result.stderr or "")).strip()
        raise RuntimeError(
            "Command failed (%d): %s\n%s" % (result.returncode, " ".join(cmd), out)
        )
    return result


def git(*args, check=True):
    return run(["git", *args], check=check)


def git_out(*args, check=True):
    return git(*args, check=check).stdout.strip()


def confirm(prompt):
    while True:
        ans = input("%s [y/N] " % prompt).strip().lower()
        if ans in ("y", "yes"):
            return True
        if ans in ("n", "no", ""):
            return False


def load_lck():
    if LCK_PATH.exists():
        try:
            return json.loads(LCK_PATH.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            return None
    return None


def save_lck(stage, **context):
    data = load_lck() or {}
    data.update(context)
    data["stage"] = stage
    data["updated"] = datetime.now().isoformat(timespec="seconds")
    LCK_PATH.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def current_branch():
    result = git("rev-parse", "--abbrev-ref", "HEAD", check=False)
    if result.returncode != 0:
        return None
    name = result.stdout.strip()
    return name or None


def changed_paths():
    # Raw stdout, not git_out(): git_out() strips the whole string, which eats
    # the leading space of the first " M path" line and corrupts that path.
    out = git("status", "--porcelain").stdout
    paths = []
    for line in out.splitlines():
        if len(line) < 3:
            continue
        path = line[3:].strip()
        if not path:
            continue
        if " -> " in path:
            path = path.split(" -> ", 1)[1]
        paths.append(path.strip('"').replace("\\", "/"))
    return paths


def is_build_folder(name):
    folder = REPO_ROOT / "builds" / name
    return folder.is_dir() and (folder / "platformio.ini").is_file() and (folder / "myoptions.h").is_file()


def changed_build_folders(paths):
    folders = set()
    for path in paths:
        parts = path.split("/")
        if len(parts) < 2 or parts[0] != "builds":
            continue
        name = parts[1]
        if not name or name == "releases":
            continue
        # Only real contributor folders (platformio.ini + myoptions.h), which
        # also filters out builds/__pycache__ and loose files under builds/.
        if is_build_folder(name):
            folders.add(name)
    return sorted(folders)


def non_builds_changes(paths):
    return [p for p in paths if not p.startswith("builds/")]


def radionversion():
    options_h = REPO_ROOT / "src" / "core" / "options.h"
    if not options_h.exists():
        return ""
    match = re.search(r'#define\s+RADIOVERSION\s+"([^"]+)"', options_h.read_text(encoding="utf-8", errors="ignore"))
    return match.group(1) if match else ""


def latest_release_tag():
    if not shutil.which("gh"):
        return ""
    result = run(["gh", "release", "view", "--json", "tagName", "-q", ".tagName"], check=False)
    if result.returncode != 0:
        return ""
    return result.stdout.strip()


def gh_ready():
    if not shutil.which("gh"):
        return False
    return run(["gh", "auth", "status"], check=False).returncode == 0


def stage_0():
    print("[*] STAGE 0: PRECHECK")
    branch = current_branch()
    if branch is None:
        if not confirm("Are you on main branch? Have you added the build files?"):
            print("[X] Aborted.")
            return None
    elif branch != MAIN_BRANCH:
        print("[X] You should switch to main branch and add the build. (current: %s)" % branch)
        return None

    if git("fetch", "origin", MAIN_BRANCH, check=False).returncode != 0:
        print("[!] Could not fetch origin/%s; comparing against the local remote ref." % MAIN_BRANCH)

    local_info = REPO_ROOT / FIRMWARE_INFO
    if not local_info.exists():
        print("[X] %s not found. Run: cd builds && python fix_releases.py" % FIRMWARE_INFO)
        return None
    local_text = local_info.read_text(encoding="utf-8").replace("\r\n", "\n")
    remote = git("show", "origin/%s:%s" % (MAIN_BRANCH, FIRMWARE_INFO), check=False)
    if remote.returncode == 0 and remote.stdout.replace("\r\n", "\n") == local_text:
        print("[X] You forgot to run fix_releases.py!")
        return None

    paths = changed_paths()
    if not paths:
        print("[X] No changes detected. Add your build files, then run fix_releases.py.")
        return None

    folders = changed_build_folders(paths)
    if not folders:
        print("[!] No changes detected under builds/ (other than builds/releases/).")
        if not confirm("Continue anyway?"):
            print("[X] Aborted.")
            return None

    outside = non_builds_changes(paths)
    if outside:
        print("[!] Files outside /builds have been changed:")
        for path in outside:
            print("      %s" % path)
        print("    These are not part of the build commit. A changed source needs")
        print("    'builds=all' to stay consistent with the released version.")
        if not confirm("Files outside /builds have been changed. Do you still wish to proceed?"):
            print("[X] Aborted.")
            return None

    print("[*] Build files to be committed:")
    for path in paths:
        if path.startswith("builds/"):
            print("      %s" % path)
    if folders:
        print("[*] Changed contributor folders: %s" % ", ".join(folders))
    else:
        print("[*] Changed contributor folders: (none)")

    save_lck(1, builds=folders, version=radionversion())
    return 1


def stage_1():
    print("\n[*] STAGE 1: COMMIT on main")
    dirty = git_out("status", "--porcelain")
    if dirty:
        if not confirm("Ready to commit?"):
            print("[!] Stopped. State saved; re-run to resume.")
            return None
        git("add", "builds/")
        git("commit", "-m", datetime.now().strftime("%Y.%m.%d"))
    else:
        print("[*] Nothing to commit; using the current HEAD.")
    sha = git_out("rev-parse", "HEAD")
    print("[OK] Commit %s" % sha[:12])
    save_lck(2, commit_sha=sha)
    return 2


def stage_2():
    print("\n[*] STAGE 2: PUSH main")
    if not confirm("Push main to origin?"):
        print("[!] Stopped. State saved; re-run to resume.")
        return None
    git("push", "origin", MAIN_BRANCH)
    print("[OK] Pushed %s" % MAIN_BRANCH)
    save_lck(3)
    return 3


def wait_for_run(timeout=90):
    deadline = time.time() + timeout
    while time.time() < deadline:
        result = run(
            ["gh", "run", "list", "--workflow", WORKFLOW_NAME, "--limit", "1",
             "--json", "databaseId,status,conclusion"],
            check=False,
        )
        if result.returncode == 0 and result.stdout.strip() not in ("", "[]"):
            try:
                data = json.loads(result.stdout)
            except json.JSONDecodeError:
                data = []
            if data:
                return data[0]["databaseId"]
        time.sleep(3)
    return None


def stage_3():
    print("\n[*] STAGE 3: RUN WORKFLOW")
    lck = load_lck() or {}
    folders = lck.get("builds", [])
    version = latest_release_tag()
    if version:
        print("[*] Target release: %s" % version)
    else:
        print("[!] Could not resolve the latest release tag; the workflow will resolve it.")

    if not folders:
        print("[*] Force list: (none) - the workflow will auto-detect missing firmware.")

    if gh_ready() and confirm("Trigger the %s workflow now?" % WORKFLOW_DISPLAY):
        cmd = ["gh", "workflow", "run", WORKFLOW_NAME, "--ref", MAIN_BRANCH]
        if folders:
            cmd += ["-f", "builds=" + ",".join(folders)]
        run(cmd)
        print("[*] Waiting for the run to start...")
        run_id = wait_for_run()
        if run_id is None:
            print("[X] Could not find the run. Check GitHub Actions.")
            return None
        print("[*] Watching run %s ..." % run_id)
        watch = run(["gh", "run", "watch", str(run_id), "--exit-status"], check=False)
        if watch.returncode != 0:
            print("[X] Workflow failed or was cancelled. Inspect: gh run view %s" % run_id)
            print("[!] State saved; re-run to resume.")
            return None
        print("[OK] Workflow completed successfully")
    else:
        print("[*] Go to github and run the %s workflow!" % WORKFLOW_DISPLAY)
        while input("Did you run the release? [y/N] ").strip().lower() not in ("y", "yes"):
            pass

    save_lck(4)
    return 4


def repo_slug():
    if shutil.which("gh"):
        result = run(["gh", "repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"], check=False)
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip()
    url = git_out("remote", "get-url", "origin", check=False)
    match = re.search(r"github\.com[:/](.+?)(?:\.git)?/?$", url)
    return match.group(1) if match else ""


def compare_url():
    slug = repo_slug()
    if not slug:
        return ""
    return "https://github.com/%s/compare/%s...%s" % (slug, DEV_BRANCH, MAIN_BRANCH)


def merge_main_into_dev():
    ff = git("merge", "--ff-only", MAIN_BRANCH, check=False)
    if ff.returncode == 0:
        print("[OK] Fast-forwarded %s to %s" % (DEV_BRANCH, MAIN_BRANCH))
        return True

    print("[*] Fast-forward not possible; creating a merge commit.")
    merge = git("merge", "--no-ff", "-m", datetime.now().strftime("%Y.%m.%d"), MAIN_BRANCH, check=False)
    if merge.returncode == 0:
        return True

    conflicted = [p for p in git_out("diff", "--name-only", "--diff-filter=U", check=False).splitlines() if p]
    print("[!] Merge conflict.")
    if conflicted:
        print("[!] Conflicted files:")
        for path in conflicted:
            print("      %s" % path)
    url = compare_url()
    if url:
        print("[!] Inspect the difference: %s" % url)

    if not confirm("Force %s to accept main's versions of the conflicted files?" % DEV_BRANCH):
        git("merge", "--abort", check=False)
        print("[!] Merge aborted. State saved; re-run to resume.")
        return False

    for path in conflicted:
        git("checkout", "--theirs", "--", path, check=False)
        git("add", "--", path)
    if git("diff", "--cached", "--quiet", check=False).returncode != 0:
        if git("commit", "--no-edit", check=False).returncode != 0:
            print("[!] Could not finalize the merge; check 'git status' on %s." % DEV_BRANCH)
            return False
    print("[OK] Conflict resolved in favor of %s" % MAIN_BRANCH)
    return True


def stage_4():
    print("\n[*] STAGE 4: MERGE main into dev")
    if git_out("status", "--porcelain"):
        print("[X] Working tree is not clean. Commit or stash other changes, then re-run.")
        return None

    if not confirm("Switch to %s and merge %s into it?" % (DEV_BRANCH, MAIN_BRANCH)):
        print("[!] Stopped. State saved; re-run to resume.")
        return None

    git("switch", DEV_BRANCH)
    if git("fetch", "origin", check=False).returncode != 0:
        print("[!] Could not fetch origin; using local refs.")
    if git("pull", "--ff-only", "origin", DEV_BRANCH, check=False).returncode != 0:
        print("[!] Could not fast-forward %s from origin; continuing." % DEV_BRANCH)

    if not merge_main_into_dev():
        return None

    dev_sha = git_out("rev-parse", "HEAD")
    print("[OK] %s now at %s" % (DEV_BRANCH, dev_sha[:12]))

    if confirm("Push %s to origin?" % DEV_BRANCH):
        git("push", "origin", DEV_BRANCH)
        print("[OK] Pushed %s" % DEV_BRANCH)

    save_lck(5, dev_commit_sha=dev_sha)
    return 5


def stage_5():
    print("\n[*] STAGE 5: FINALIZE")
    if current_branch() != DEV_BRANCH:
        if git("switch", DEV_BRANCH, check=False).returncode != 0:
            print("[!] Could not switch back to %s. Please switch manually." % DEV_BRANCH)
    if LCK_PATH.exists():
        LCK_PATH.unlink()
    print("[OK] Done. Current branch: %s" % current_branch())
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Add or refresh firmware builds in the latest release, then copy to dev."
    )
    parser.add_argument("--reset", action="store_true", help="discard saved state and start over")
    args = parser.parse_args()

    if args.reset and LCK_PATH.exists():
        LCK_PATH.unlink()
        print("[*] State cleared.")

    lck = load_lck()
    stage = lck.get("stage", 0) if lck else 0
    if lck and stage:
        print("[!] Resuming from STAGE %d (%s)" % (stage, STAGE_NAMES.get(stage, "?")))

    stages = {0: stage_0, 1: stage_1, 2: stage_2, 3: stage_3, 4: stage_4, 5: stage_5}
    try:
        while stage is not None and stage <= 5:
            stage = stages[stage]()
    except KeyboardInterrupt:
        print("\n[!] Interrupted. State saved; re-run to resume from STAGE %d." % (stage or 0))
        return 130
    except OSError as exc:
        print("\n[X] %s" % exc)
        return 1
    except RuntimeError as exc:
        print("\n[X] %s" % exc)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
