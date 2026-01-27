import os
import sys
import subprocess
import configparser
from typing import List, Dict, Tuple, Optional

# ---
# Console color helpers
# ---

def _supports_color() -> bool:
    try:
        return sys.stdout.isatty() and os.environ.get("NO_COLOR") is None
    except Exception:
        return False


def _fmt_arg(arg: str) -> str:
    return f'"{arg}"' if any(ch.isspace() for ch in arg) else arg


def _colorize_git_cmd(cmd: List[str]) -> str:
    # Fallback to plain when color unsupported
    if not _supports_color():
        return " ".join(_fmt_arg(x) for x in cmd)

    def c(text: str, code: str) -> str:
        return f"\033[{code}m{text}\033[0m"

    colored_parts: List[str] = []
    for i, tok in enumerate(cmd):
        ftok = _fmt_arg(tok)
        if i == 0:  # 'git'
            colored_parts.append(c(ftok, "96;1"))  # bright cyan, bold
        elif tok.startswith("-"):
            colored_parts.append(c(ftok, "93"))    # bright yellow for flags
        elif "://" in tok:
            colored_parts.append(c(ftok, "95"))    # bright magenta for URLs
        else:
            colored_parts.append(c(ftok, "92"))    # bright green for args
    return " ".join(colored_parts)


def _print_git_invocation(cmd: List[str], cwd: Optional[str]) -> None:
    try:
        line = _colorize_git_cmd(cmd)
        print(f">> {line}")
        if cwd:
            if _supports_color():
                print(f"   \033[90mCWD:\033[0m {cwd}")  # dim gray label
            else:
                print(f"   CWD: {cwd}")
    except Exception:
        # Best-effort printing; ignore failures
        pass

# -----------------------------
# Git utilities
# -----------------------------

def run_git(args: List[str], cwd: Optional[str] = None) -> Tuple[int, str, str]:
    cmd = ["git"] + args
    _print_git_invocation(cmd, cwd)
    try:
        proc = subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        out, err = proc.communicate()
        return proc.returncode, (out or "").strip(), (err or "").strip()
    except FileNotFoundError:
        return 1, "", "Git not found. Install Git and ensure it's on PATH."


def is_git_repo(path: str) -> bool:
    code, out, _ = run_git(["rev-parse", "--is-inside-work-tree"], cwd=path)
    return code == 0 and out == "true"


def list_remotes(path: str) -> Dict[str, Dict[str, str]]:
    code, out, _ = run_git(["remote", "-v"], cwd=path)
    if code != 0:
        return {}
    remotes: Dict[str, Dict[str, str]] = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            name = parts[0]
            url = parts[1]
            kind = parts[2].strip("()")
            remotes.setdefault(name, {})[kind] = url
    return remotes


def list_branches(path: str) -> List[str]:
    code, out, _ = run_git(["branch", "-a"], cwd=path)
    if code != 0:
        return []
    branches: List[str] = []
    for line in out.splitlines():
        line = line.strip()
        if not line:
            continue
        if line.startswith("* "):
            branches.append(line[2:])
        else:
            branches.append(line)
    return branches

def get_current_branch(path: str) -> Optional[str]:
    code, out, _ = run_git(["rev-parse", "--abbrev-ref", "HEAD"], cwd=path)
    if code != 0:
        return None
    branch = out.strip()
    if not branch or branch == "HEAD":
        return None
    return branch


def parse_gitmodules(root_path: str) -> List[Tuple[str, str]]:
    gitmodules = os.path.join(root_path, ".gitmodules")
    if not os.path.isfile(gitmodules):
        return []
    code, out, _ = run_git(["config", "-f", ".gitmodules", "--list"], cwd=root_path)
    if code != 0:
        return []
    paths: Dict[str, str] = {}
    for line in out.splitlines():
        if not line.startswith("submodule."):
            continue
        try:
            key, value = line.split("=", 1)
        except ValueError:
            continue
        segments = key.split(".")
        if len(segments) >= 3 and segments[2] == "path":
            name = segments[1]
            paths[name] = value.strip()
    return [(name, paths[name]) for name in paths]


def discover_subrepos(root_path: str) -> List[Tuple[str, str]]:
    found: List[Tuple[str, str]] = []
    modules = parse_gitmodules(root_path)
    for name, rel in modules:
        abspath = os.path.join(root_path, rel)
        if os.path.isdir(abspath):
            found.append((name, abspath))
    if found:
        return found
    try:
        for entry in os.scandir(root_path):
            if entry.is_dir():
                subpath = entry.path
                if subpath == os.path.join(root_path, ".git"):
                    continue
                if is_git_repo(subpath):
                    found.append((os.path.basename(subpath), subpath))
    except Exception:
        pass
    return found

# -----------------------------
# INI remotes
# -----------------------------

def load_remotes_ini(root_path: str) -> Dict[str, str]:
    cfg_path = os.path.join(root_path, "remotes.ini")
    result: Dict[str, str] = {}
    if not os.path.isfile(cfg_path):
        return result
    parser = configparser.ConfigParser()
    try:
        parser.read(cfg_path, encoding="utf-8")
        if parser.has_section("remotes"):
            for alias, base in parser.items("remotes"):
                base = base.strip()
                if not base.endswith("/"):
                    base += "/"
                result[alias.strip()] = base
    except Exception:
        pass
    return result

def set_ini_remote(root_path: str, alias: str, base: str) -> str:
    cfg_path = os.path.join(root_path, "remotes.ini")
    parser = configparser.ConfigParser()
    # Read existing config if present
    if os.path.isfile(cfg_path):
        try:
            parser.read(cfg_path, encoding="utf-8")
        except Exception:
            # Reset parser on read failure
            parser = configparser.ConfigParser()
    if not parser.has_section("remotes"):
        parser.add_section("remotes")
    alias = (alias or "").strip()
    base = (base or "").strip()
    if not alias or not base:
        return "Alias and base are required"
    if not base.endswith("/"):
        base += "/"
    parser.set("remotes", alias, base)
    with open(cfg_path, "w", encoding="utf-8") as f:
        parser.write(f)
    return f"Set INI remote: {alias} = {base}"

def remove_ini_remote(root_path: str, alias: str) -> str:
    cfg_path = os.path.join(root_path, "remotes.ini")
    parser = configparser.ConfigParser()
    if not os.path.isfile(cfg_path):
        return "remotes.ini not found"
    try:
        parser.read(cfg_path, encoding="utf-8")
    except Exception:
        return "Failed to read remotes.ini"
    if not parser.has_section("remotes"):
        return "[remotes] section not found"
    alias = (alias or "").strip()
    if not alias:
        return "Alias is required"
    if not parser.has_option("remotes", alias):
        return f"Alias '{alias}' not found in remotes.ini"
    parser.remove_option("remotes", alias)
    with open(cfg_path, "w", encoding="utf-8") as f:
        parser.write(f)
    return f"Removed INI remote: {alias}"

# -----------------------------
# Remote ensure/rename helpers
# -----------------------------

def ensure_remote(path: str, name: str, url: str) -> Tuple[bool, str]:
    remotes = list_remotes(path)
    if name in remotes:
        fetch = remotes[name].get("fetch")
        if fetch != url:
            code, _, err = run_git(["remote", "set-url", name, url], cwd=path)
            if code == 0:
                return True, f"Updated remote {name} -> {url}"
            else:
                return False, f"Failed to set-url {name}: {err}"
        return False, f"Remote {name} already exists"
    candidate = name
    counter = 2
    while candidate in remotes:
        candidate = f"{name}{counter}"
        counter += 1
    code, _, err = run_git(["remote", "add", candidate, url], cwd=path)
    if code == 0:
        return True, f"Added remote {candidate} -> {url}"
    return False, f"Failed to add remote {candidate}: {err}"


def find_suffix_from_any_base(remotes: Dict[str, Dict[str, str]], ini_remotes: Dict[str, str]) -> Optional[str]:
    for kinds in remotes.values():
        for url in kinds.values():
            for base in ini_remotes.values():
                if url.startswith(base):
                    return url[len(base):]
    return None


def ensure_ini_remote_for_repo(path: str, alias: str, base: str, ini_remotes: Dict[str, str]) -> List[str]:
    changes: List[str] = []
    remotes = list_remotes(path)
    suffix = find_suffix_from_any_base(remotes, ini_remotes)
    if not suffix:
        repo_dir = os.path.basename(path.rstrip("/\\"))
        suffix = f"{repo_dir}.git"

    match_name = None
    match_url = None
    for rname, kinds in remotes.items():
        for url in kinds.values():
            if url.startswith(base):
                match_name = rname
                match_url = url
                break
        if match_name:
            break

    alias_exists = alias in remotes
    if match_name and match_name != alias:
        if alias_exists:
            code, _, err = run_git(["remote", "set-url", alias, match_url], cwd=path)
            changes.append((code == 0 and f"Updated {alias} -> {match_url}") or f"Failed to set-url {alias}: {err}")
            code, _, err = run_git(["remote", "remove", match_name], cwd=path)
            changes.append((code == 0 and f"Removed old remote {match_name}") or f"Failed to remove {match_name}: {err}")
        else:
            code, _, err = run_git(["remote", "rename", match_name, alias], cwd=path)
            changes.append((code == 0 and f"Renamed {match_name} -> {alias}") or f"Failed to rename {match_name} -> {alias}: {err}")
        return changes

    target_url = base + suffix
    if alias_exists:
        existing_fetch = remotes[alias].get("fetch")
        if not existing_fetch or not existing_fetch.startswith(base):
            code, _, err = run_git(["remote", "set-url", alias, target_url], cwd=path)
            changes.append((code == 0 and f"Updated {alias} -> {target_url}") or f"Failed to set-url {alias}: {err}")
        return changes

    changed, msg = ensure_remote(path, alias, target_url)
    changes.append(msg)
    return changes

# -----------------------------
# CLI operations
# -----------------------------

def print_ini_and_repos(root_path: str):
    ini_remotes = load_remotes_ini(root_path)
    print("[remotes.ini]")
    if not ini_remotes:
        print("(none)")
    else:
        for alias, base in ini_remotes.items():
            print(f"- {alias} = {base}")
    print()

    if not is_git_repo(root_path):
        print("Selected root is not a Git repository.")
        return

    repos = [("Main Repo", root_path)] + discover_subrepos(root_path)
    for name, path in repos:
        print(f"[Repo] {name}: {path}")
        remotes = list_remotes(path)
        if remotes:
            print("  Remotes:")
            for rname, kinds in remotes.items():
                detail = ", ".join([f"{k}: {v}" for k, v in kinds.items()])
                print(f"    - {rname}: {detail}")
        else:
            print("  Remotes: (none)")
        branches = list_branches(path)
        if branches:
            print("  Branches:")
            for b in branches:
                print(f"    - {b}")
        else:
            print("  Branches: (none)")
        print()


def op_fix_remote(root_path: str):
    ini_remotes = load_remotes_ini(root_path)
    if not ini_remotes:
        print("No INI remotes configured. Edit remotes.ini.")
        return
    repos = [("Main Repo", root_path)] + discover_subrepos(root_path)
    any_changes = False
    for repo_name, repo_path in repos:
        for alias, base in ini_remotes.items():
            msgs = ensure_ini_remote_for_repo(repo_path, alias, base, ini_remotes)
            for m in msgs:
                if m:
                    any_changes = True
                    print(f"[{repo_name}] {m}")
    if not any_changes:
        print("No changes; all repositories already match INI remotes.")


def _resolve_remote_name(repo_path: str, alias: str, base: str, ini_remotes: Dict[str, str]) -> Optional[str]:
    remotes = list_remotes(repo_path)
    if alias in remotes:
        return alias
    for rname, kinds in remotes.items():
        if any(url.startswith(base) for url in kinds.values()):
            return rname
    msgs = ensure_ini_remote_for_repo(repo_path, alias, base, ini_remotes)
    # print ensure messages
    for m in msgs:
        if m:
            print(f"[{os.path.basename(repo_path)}] {m}")
    remotes = list_remotes(repo_path)
    if alias in remotes:
        return alias
    return None

def _resolve_direct_remote(repo_path: str, rname: str) -> Optional[str]:
    remotes = list_remotes(repo_path)
    return rname if rname in remotes else None


def op_fetch_pull_push(root_path: str, cmd: str, target_alias: Optional[str] = None):
    ini_remotes = load_remotes_ini(root_path)
    if not ini_remotes:
        print("No INI remotes configured. Edit remotes.ini.")
        return
    if cmd not in ("fetch", "pull", "push"):
        print(f"Unknown command: {cmd}")
        return

    repos = [("Main Repo", root_path)] + discover_subrepos(root_path)
    use_direct = bool(target_alias and target_alias not in ini_remotes)
    aliases = [target_alias] if target_alias else list(ini_remotes.keys())

    for repo_name, repo_path in repos:
        for alias in aliases:
            if use_direct:
                rname = _resolve_direct_remote(repo_path, alias)
                if not rname:
                    print(f"[{repo_name}] Remote '{alias}' not found; skip")
                    continue
            else:
                base = ini_remotes.get(alias)
                if not base:
                    print(f"[{repo_name}] Alias '{alias}' not in INI remotes.")
                    continue
                rname = _resolve_remote_name(repo_path, alias, base, ini_remotes)
            if not rname:
                print(f"[{repo_name}] No matching remote for base {base}")
                continue
            if cmd == "fetch":
                code, out, err = run_git(["fetch", rname, "--prune", "--tags"], cwd=repo_path)
            elif cmd == "pull":
                cur = get_current_branch(repo_path)
                if not cur:
                    print(f"[{repo_name}] pull {rname}: SKIP (detached HEAD or unknown branch)")
                    continue
                code, out, err = run_git(["pull", "--ff-only", rname, cur], cwd=repo_path)
            else:  # push (branches then tags)
                code1, out1, err1 = run_git(["push", rname, "--all"], cwd=repo_path)
                status1 = "OK" if code1 == 0 else "FAIL"
                print(f"[{repo_name}] push branches to {rname}: {status1}")
                if out1:
                    print(out1)
                if err1:
                    print(err1)

                code2, out2, err2 = run_git(["push", rname, "--tags"], cwd=repo_path)
                status2 = "OK" if code2 == 0 else "FAIL"
                print(f"[{repo_name}] push tags to {rname}: {status2}")
                if out2:
                    print(out2)
                if err2:
                    print(err2)


def main():
    root = os.environ.get("CMPROJECT_ROOT", os.getcwd())
    args = sys.argv[1:]
    if not args:
        print_ini_and_repos(root)
        return
    cmd = args[0].lower()
    if cmd in ("fix", "fix_remote"):
        op_fix_remote(root)
    elif cmd in ("fetch", "pull", "push"):
        alias = args[1] if len(args) > 1 else None
        op_fetch_pull_push(root, cmd, alias)
    elif cmd == "ar":
        if len(args) < 3:
            print("Usage: python git_repos_cli.py ar <alias> <base_url_prefix>")
            return
        alias = args[1]
        base = args[2]
        msg = set_ini_remote(root, alias, base)
        print(msg)
    elif cmd == "rr":
        if len(args) < 2:
            print("Usage: python git_repos_cli.py rr <alias>")
            return
        alias = args[1]
        msg = remove_ini_remote(root, alias)
        print(msg)
    else:
        print("Usage:")
        print("  python git_repos_cli.py                  # list INI remotes, all repo remotes + branches")
        print("  python git_repos_cli.py fix              # fix remote names per INI, add missing remotes")
        print("  python git_repos_cli.py fetch [alias]    # git fetch for alias (or all INI remotes)")
        print("  python git_repos_cli.py pull [alias]     # git pull for alias (or all INI remotes)")
        print("  python git_repos_cli.py push [alias]     # git push --all --tags for alias (or all INI remotes)")
        print("  python git_repos_cli.py ar <alias> <base_url_prefix>  # add/update INI remote alias")
        print("  python git_repos_cli.py rr <alias>       # remove alias from remotes.ini")


if __name__ == "__main__":
    main()
