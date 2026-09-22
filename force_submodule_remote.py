#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
强制指定 git submodule 的远端，并执行 fetch/pull/push/update

示例:
  python force_submodule_remote.py --root E:\ULRE --remote github --fetch --pull --update
  python force_submodule_remote.py --root E:\ULRE --remote hyzgame --push --update
"""

import argparse
import os
import re
import subprocess
import sys
from typing import Dict, List, Tuple

def run_git(args: List[str], cwd: str = None, dry_run: bool = False) -> Tuple[int, str, str]:
    cmd = ["git"] + args
    print(f"$ {' '.join(cmd)}  (cwd={cwd})" if cwd else f"$ {' '.join(cmd)}")
    if dry_run:
        return 0, "", ""
    try:
        p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           text=True, encoding="utf-8", errors="replace")
        return p.returncode, (p.stdout or "").strip(), (p.stderr or "").strip()
    except FileNotFoundError:
        return 127, "", "git not found on PATH"

def parse_gitmodules(root: str) -> Dict[str, Dict[str, str]]:
    """返回 {submodule_name: {path:..., url:...}}，从 .gitmodules 解析。"""
    gm = os.path.join(root, ".gitmodules")
    if not os.path.exists(gm):
        return {}

    code, out, err = run_git(["config", "-f", ".gitmodules", "--list"], cwd=root)
    if code != 0:
        return {}

    result: Dict[str, Dict[str, str]] = {}
    for line in out.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        parts = key.split(".")
        if len(parts) >= 3 and parts[0] == "submodule":
            subname = parts[1]
            kind = parts[2]
            result.setdefault(subname, {})[kind] = value.strip()
    return result


def repo_name_from_url(url: str, fallback: str) -> str:
    """优先从真实 URL 推导 repo 名，避免把目录名当成仓库名。"""
    if not url:
        return fallback
    candidate = url.rstrip("/").split("/")[-1]
    if candidate.endswith(".git"):
        candidate = candidate[:-4]
    return candidate or fallback

def current_branch(repo_path: str) -> str:
    code, out, err = run_git(["rev-parse", "--abbrev-ref", "HEAD"], cwd=repo_path)
    if code == 0 and out:
        return out.strip()
    return "main"


def detect_default_branch(repo_path: str, remote_name: str, fallback: str = "main", dry_run: bool = False) -> str:
    """检测远端的默认分支（HEAD）；优先使用远端 HEAD 解析。"""
    code, out, err = run_git(["ls-remote", "--symref", remote_name, "HEAD"], cwd=repo_path, dry_run=dry_run)
    if code == 0:
        match = re.search(r"ref:\s*refs/heads/([^\s]+)\s+HEAD", out, re.IGNORECASE)
        if match:
            return match.group(1).strip()

        match = re.search(r"\bHEAD\s+refs/heads/([^\s]+)", out, re.IGNORECASE)
        if match:
            return match.group(1).strip()

    code, out, err = run_git(["remote", "show", remote_name], cwd=repo_path, dry_run=dry_run)
    if code == 0:
        match = re.search(r"HEAD\s+branch:\s*([^\r\n]+)", out, re.IGNORECASE)
        if match:
            return match.group(1).strip()

    branch = current_branch(repo_path)
    return branch if branch and branch not in ("HEAD", "") else fallback


def checkout_default_branch(repo_path: str, remote_name: str, preferred_branch: str = None, dry_run: bool = False) -> str:
    """切到远端的默认分支；若本地不存在，则从远端拉一个跟踪分支。"""
    branch = (preferred_branch or detect_default_branch(repo_path, remote_name, dry_run=dry_run) or "main").strip()
    if not branch:
        branch = "main"

    code, _, _ = run_git(["show-ref", "--verify", "--quiet", f"refs/heads/{branch}"], cwd=repo_path, dry_run=dry_run)
    if code == 0:
        run_git(["checkout", branch], cwd=repo_path, dry_run=dry_run)
    else:
        run_git(["checkout", "-B", branch, "--track", f"{remote_name}/{branch}"], cwd=repo_path, dry_run=dry_run)

    return branch


def ensure_remote(repo_path: str, remote_name: str, remote_url: str, dry_run: bool = False) -> None:
    code, out, err = run_git(["remote", "get-url", remote_name], cwd=repo_path, dry_run=dry_run)
    if code == 0:
        run_git(["remote", "set-url", remote_name, remote_url], cwd=repo_path, dry_run=dry_run)
    else:
        run_git(["remote", "add", remote_name, remote_url], cwd=repo_path, dry_run=dry_run)

def set_gitmodules_url(root: str, submodule_name: str, url: str, dry_run: bool = False) -> None:
    gm = os.path.join(root, ".gitmodules")
    if not os.path.exists(gm):
        return
    run_git(["config", "-f", ".gitmodules", f"submodule.{submodule_name}.url", url], cwd=root, dry_run=dry_run)

def build_url(remote_name: str, repo_name: str) -> str:
    remote_name = remote_name.lower()
    if remote_name == "github":
        return f"https://github.com/hyzboy/{repo_name}.git"
    if remote_name == "hyzgame":
        return f"https://git.hyzgame.com/hyzboy/{repo_name}.git"
    return remote_name

def force_repo_remote(repo_path: str, remote_name: str, repo_name: str, dry_run: bool = False):
    target_url = build_url(remote_name, repo_name)
    ensure_remote(repo_path, remote_name, target_url, dry_run=dry_run)
    print(f"[force] {repo_path} -> {remote_name} = {target_url}")

def process_repo(root: str, remote_name: str, branch: str, do_fetch: bool, do_pull: bool, do_push: bool, do_update: bool, dry_run: bool):
    # 1) 先统一根仓库
    root_name = os.path.basename(os.path.normpath(root))
    force_repo_remote(root, remote_name, root_name, dry_run=dry_run)
    root_branch = checkout_default_branch(root, remote_name, preferred_branch=branch, dry_run=dry_run)

    # 2) 处理子模块
    subs = parse_gitmodules(root)
    for subname, meta in subs.items():
        subpath = meta.get("path")
        old_url = meta.get("url")
        if not subpath:
            continue

        full = os.path.normpath(os.path.join(root, subpath))
        git_dir = os.path.join(full, ".git")
        if not os.path.exists(full) and not os.path.exists(git_dir):
            print(f"[skip] submodule path missing: {full}")
            continue

        # 优先使用 .gitmodules 里真实 URL 的仓库名，避免目录名误判为仓库名
        repo_name = repo_name_from_url(old_url, os.path.basename(os.path.normpath(subpath)))
        target_url = build_url(remote_name, repo_name)

        # 永久覆盖 submodule 配置
        set_gitmodules_url(root, subname, target_url, dry_run=dry_run)
        ensure_remote(full, remote_name, target_url, dry_run=dry_run)
        sub_branch = checkout_default_branch(full, remote_name, preferred_branch=branch, dry_run=dry_run)

        # 额外执行 fetch/pull/push
        if do_fetch:
            run_git(["fetch", remote_name, "--all", "--prune"], cwd=full, dry_run=dry_run)
        if do_pull:
            run_git(["pull", remote_name, sub_branch], cwd=full, dry_run=dry_run)
        if do_push:
            run_git(["push", remote_name, sub_branch], cwd=full, dry_run=dry_run)

    # 3) sync + update
    if do_update:
        run_git(["submodule", "sync", "--recursive"], cwd=root, dry_run=dry_run)

        for subname, meta in subs.items():
            subpath = meta.get("path")
            if not subpath:
                continue
            full = os.path.normpath(os.path.join(root, subpath))
            if not os.path.exists(full):
                continue
            old_url = meta.get("url")
            repo_name = repo_name_from_url(old_url, os.path.basename(os.path.normpath(subpath)))
            target_url = build_url(remote_name, repo_name)
            run_git(
                ["-c", f"submodule.{subname}.url={target_url}", "submodule", "update",
                 "--progress", "--init", "--recursive", "--force"],
                cwd=root,
                dry_run=dry_run,
            )
            checkout_default_branch(full, remote_name, preferred_branch=branch, dry_run=dry_run)

def main():
    ap = argparse.ArgumentParser(description="强制指定 git remote，并执行 fetch/pull/push/update")
    ap.add_argument("--root", default=os.getcwd(), help="仓库根目录")
    ap.add_argument("--remote", choices=["github", "hyzgame"], default="github", help="强制指定的远端名")
    ap.add_argument("--branch", default=None, help="分支名，例如 main/master")
    ap.add_argument("--fetch", action="store_true", help="执行 git fetch")
    ap.add_argument("--pull", action="store_true", help="执行 git pull")
    ap.add_argument("--push", action="store_true", help="执行 git push")
    ap.add_argument("--update", action="store_true", help="执行 git submodule update --progress --init --recursive --force")
    ap.add_argument("--dry-run", action="store_true", help="只打印命令，不实际执行")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    if not os.path.exists(os.path.join(root, ".git")):
        print(f"不是 git 仓库: {root}", file=sys.stderr)
        return 1

    process_repo(
        root=root,
        remote_name=args.remote,
        branch=args.branch,
        do_fetch=args.fetch,
        do_pull=args.pull,
        do_push=args.push,
        do_update=args.update,
        dry_run=args.dry_run
    )
    return 0

if __name__ == "__main__":
    sys.exit(main())