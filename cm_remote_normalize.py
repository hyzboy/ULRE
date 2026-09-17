#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
cm_remote_normalize.py

规范化 ULRE 主仓库及其子仓库（名称以 CM 开头）的 git 远端：

规则
1. hyzgame 老地址改写
   远端 URL 以 http://git.hyzgame.com:3000/hyzboy/ 开头的，
   统一改成 https://git.hyzgame.com/hyzboy/ 开头，并且该远端改名为 hyzgame。

2. github 远端改名
   如果某个仓库里「只有一个」远端 URL 以 https://github.com/hyzboy/ 开头，
   则把这个远端改名为 github（无论它原来叫什么，常见是 origin）。
   若同时存在多个，打印告警并跳过，避免误改。

3. 兜底补齐 hyzgame
   检查是否同时存在：名为 hyzgame 的远端 + 指向 git.hyzgame.com 的远端。
   两者缺其一，就把 hyzgame 补齐（能复名就复名，不能就新建），
   新地址由现有 github 远端或目录名推导：https://git.hyzgame.com/hyzboy/<Repo>.git

用法
    python cm_remote_normalize.py                 # 默认 dry-run，只打印将要做的改动
    python cm_remote_normalize.py --apply         # 真正执行
    python cm_remote_normalize.py --all-sub       # 处理所有子仓库，不限于 CM 开头
    python cm_remote_normalize.py --no-root       # 不处理主仓库本身
    python cm_remote_normalize.py --root D:\ULRE  # 指定仓库根目录
"""

import argparse
import os
import subprocess
import sys
from typing import Dict, List, Optional, Tuple

# ---------------------------------------------------------------- 配置常量

OLD_HYZ_PREFIX = "http://git.hyzgame.com:3000/hyzboy/"
NEW_HYZ_PREFIX = "https://git.hyzgame.com/hyzboy/"

GITHUB_PREFIX = "https://github.com/hyzboy/"

HYZ_NAME = "hyzgame"
GH_NAME = "github"
HYZ_HOST = "git.hyzgame.com"
HYZ_BASE = "https://git.hyzgame.com/hyzboy/"   # 新增远端时的基址


# ---------------------------------------------------------------- 基础工具

def run_git(args: List[str], cwd: str) -> Tuple[int, str, str]:
    try:
        proc = subprocess.run(["git"] + args, cwd=cwd,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True, encoding="utf-8", errors="replace")
        return proc.returncode, (proc.stdout or "").strip(), (proc.stderr or "").strip()
    except FileNotFoundError:
        return 127, "", "git not found on PATH"


def is_git_repo(path: str) -> bool:
    code, out, _ = run_git(["rev-parse", "--is-inside-work-tree"], path)
    return code == 0 and out == "true"


def list_remotes(path: str) -> Dict[str, Dict[str, str]]:
    """返回 {远端名: {"fetch": url, "push": url}}，保持 git 输出顺序。"""
    code, out, _ = run_git(["remote", "-v"], path)
    if code != 0:
        return {}
    remotes: "Dict[str, Dict[str, str]]" = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        name, url, kind = parts[0], parts[1], parts[2].strip("()")
        remotes.setdefault(name, {})[kind] = url
    return remotes


def has_pushurl(path: str, name: str) -> bool:
    """是否给该远端单独配了 pushurl。只有配了才需要单独维护 --push。"""
    code, _, _ = run_git(["config", "--get", f"remote.{name}.pushurl"], path)
    return code == 0


def ensure_git_suffix(url: str) -> str:
    """补齐 .git 后缀，和主仓库 / github 远端的地址风格保持一致。"""
    base = url.split("?", 1)[0].rstrip("/")
    return url if base.endswith(".git") else base + ".git"


def fetch_url(kinds: Dict[str, str]) -> str:
    return kinds.get("fetch") or kinds.get("push") or ""


# ---------------------------------------------------------------- 仓库发现

def parse_gitmodules(root: str) -> List[Tuple[str, str]]:
    """返回 [(submodule 名, 相对路径)]，用 git config 解析，避免手写 ini 解析的坑。"""
    gm = os.path.join(root, ".gitmodules")
    if not os.path.isfile(gm):
        return []
    code, out, _ = run_git(["config", "-f", ".gitmodules", "--list"], root)
    if code != 0:
        return []
    paths: Dict[str, str] = {}
    for line in out.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        seg = key.split(".")
        if len(seg) >= 3 and seg[0] == "submodule" and seg[2] == "path":
            paths[seg[1]] = value.strip()
    return [(name, paths[name]) for name in paths]


def has_git_dir(path: str) -> bool:
    """快速判断是否像 git 仓库（子仓库的 .git 可能是文件，指向父级 modules 目录）。"""
    return os.path.exists(os.path.join(path, ".git"))


def scan_dir_fallback(root: str) -> List[Tuple[str, str]]:
    """没有 .gitmodules 时，退化为扫描一层子目录里的 git 仓库。仅在顶层使用。"""
    found: List[Tuple[str, str]] = []
    try:
        for entry in sorted(os.scandir(root), key=lambda e: e.name):
            if not entry.is_dir() or entry.name == ".git":
                continue
            if has_git_dir(entry.path):
                found.append((entry.name, entry.name))
    except OSError:
        pass
    return found


def discover_repos(root: str, max_depth: int = 3) -> List[Tuple[str, str]]:
    """递归收集子仓库 [(显示名, 绝对路径)]，去重。

    子仓库一律通过各层 .gitmodules 递归，不做全目录 fs 扫描：
    3rdpty 这类目录有上百个子目录，逐个起 git 进程会慢到超时。
    """
    result: List[Tuple[str, str]] = []
    seen: set = set()

    def walk(base: str, depth: int) -> None:
        if depth > max_depth:
            return
        subs = parse_gitmodules(base)
        if not subs and depth == 0:
            subs = scan_dir_fallback(base)
        for name, rel in subs:
            abspath = os.path.normpath(os.path.join(base, rel))
            key = os.path.normcase(abspath)
            if key in seen or not has_git_dir(abspath):
                continue
            seen.add(key)
            result.append((name, abspath))
            walk(abspath, depth + 1)

    walk(root, 0)
    return result


# ---------------------------------------------------------------- 改写逻辑

class Change:
    def __init__(self, kind: str, desc: str, args: Optional[List[str]] = None):
        self.kind = kind      # info / warn / ok / fail
        self.desc = desc
        self.args = args


def derive_repo_name(path: str, remotes: Dict[str, Dict[str, str]]) -> str:
    """推导新地址用的 <Repo>.git：优先从 github 远端后缀取，否则用目录名。"""
    for kinds in remotes.values():
        for kind in ("fetch", "push"):
            url = kinds.get(kind, "")
            for base in ("https://github.com/hyzboy/",
                         NEW_HYZ_PREFIX.rsplit("/", 1)[0] + "/"):
                if url.startswith(base):
                    suffix = url[len(base):]
                    return suffix if suffix.endswith(".git") else suffix + ".git"
    return os.path.basename(path.rstrip("\\/")) + ".git"


def plan_repo(display: str, path: str, repo_name: str) -> Tuple[List[Change], List[str]]:
    """针对单个仓库生成改动计划。不修改任何东西。返回 (changes, resulting_state)。"""
    changes: List[Change] = []
    remotes = list_remotes(path)
    if not remotes:
        changes.append(Change("warn", "无远端（或不是 git 仓库），跳过"))
        return changes, []

    # ---- 规则 1：老 hyzgame 地址 -> 新地址，并改名 hyzgame
    victims = [(n, fetch_url(k)) for n, k in remotes.items()
               if fetch_url(k).startswith(OLD_HYZ_PREFIX)]
    if victims:
        for n, url in victims:
            new_url = ensure_git_suffix(NEW_HYZ_PREFIX + url[len(OLD_HYZ_PREFIX):])
            if n != HYZ_NAME:
                if HYZ_NAME in remotes and fetch_url(remotes[HYZ_NAME]) != new_url:
                    changes.append(Change(
                        "warn",
                        f"远端 '{HYZ_NAME}' 已存在且指向 {fetch_url(remotes[HYZ_NAME])}，"
                        f"未自动处理 '{n}'（{url}）以免冲突"))
                    continue
                changes.append(Change("ok", f"rename 远端 '{n}' -> '{HYZ_NAME}'",
                                      ["rename", n, HYZ_NAME]))
            if has_pushurl(path, n) and remotes.get(n, {}).get("push", "").startswith(OLD_HYZ_PREFIX):
                changes.append(Change("ok", f"set-url --push {HYZ_NAME} -> {new_url}",
                                      ["set-url", "--push", HYZ_NAME, new_url]))
            changes.append(Change("ok", f"set-url {HYZ_NAME}: {url} -> {new_url}",
                                  ["set-url", HYZ_NAME, new_url]))

    # ---- 规则 2：唯一的 github.com/hyzboy/CM 远端 -> 改名为 github
    all_gh = [(n, fetch_url(k)) for n, k in remotes.items()
              if fetch_url(k).startswith(GITHUB_PREFIX)]
    if GH_NAME in remotes:
        # 已经有个名叫 github 的远端就不用改名了；多余的同前缀远端交给规则 3 纠偏
        if not fetch_url(remotes[GH_NAME]).startswith(GITHUB_PREFIX) and all_gh:
            changes.append(Change(
                "warn",
                f"远端 '{GH_NAME}' 指向的不是 {GITHUB_PREFIX}*（{fetch_url(remotes[GH_NAME])}），"
                f"同时还存在 {len(all_gh)} 个同前缀远端，未自动改名"))
    else:
        if len(all_gh) == 1:
            n, url = all_gh[0]
            changes.append(Change("ok", f"rename 远端 '{n}' -> '{GH_NAME}'",
                                  ["rename", n, GH_NAME]))
        elif len(all_gh) > 1:
            detail = ", ".join(f"{n}({u})" for n, u in all_gh)
            changes.append(Change(
                "warn", f"存在多个 {GITHUB_PREFIX}* 远端，跳过改名以免误伤：{detail}"))

    # ---- 规则 3：兜底补齐 hyzgame（名为 hyzgame + 指向 git.hyzgame.com）
    has_named = HYZ_NAME in remotes
    has_host = any(HYZ_HOST in u for k in remotes.values()
                   for u in (k.get("fetch"), k.get("push")) if u)
    if not (has_named and has_host):
        if has_named:
            # 名字对了但地址不对
            url = HYZ_BASE + repo_name
            changes.append(Change("ok", f"set-url {HYZ_NAME} -> {url}（原地址不含 {HYZ_HOST}）",
                                  ["set-url", HYZ_NAME, url]))
        else:
            candidates = [n for n, k in remotes.items()
                          if any(HYZ_HOST in u for u in (k.get("fetch"), k.get("push")) if u)]
            if len(candidates) == 1:
                changes.append(Change("ok", f"rename 远端 '{candidates[0]}' -> '{HYZ_NAME}'",
                                      ["rename", candidates[0], HYZ_NAME]))
            else:
                url = HYZ_BASE + repo_name
                changes.append(Change("ok", f"add 远端 {HYZ_NAME} -> {url}",
                                      ["add", HYZ_NAME, url]))

    return changes, []


def apply_repo(path: str, changes: List[Change]) -> None:
    for ch in changes:
        if ch.kind != "ok" or not ch.args:
            continue
        if ch.args[0] in ("rename", "add", "set-url"):
            # 三种指令的 args 已经是完整的 git remote 子参数
            code, _, err = run_git(["remote"] + ch.args, path)
        else:
            continue
        print(f"      [{'OK' if code == 0 else 'FAIL'}] git remote {' '.join(ch.args)}"
              + (f"  <- {err}" if code != 0 else ""))


# ---------------------------------------------------------------- 主流程

SYMBOL = {"info": "-", "warn": "!", "ok": "+", "fail": "x"}


def main() -> int:
    ap = argparse.ArgumentParser(description="规范化 ULRE 及 CM* 子仓库的 git 远端")
    ap.add_argument("--root", default=os.environ.get("CMPROJECT_ROOT") or os.getcwd(),
                    help="仓库根目录，默认当前目录")
    ap.add_argument("--apply", action="store_true",
                    help="真正执行改动；默认仅 dry-run 预览")
    ap.add_argument("--all-sub", action="store_true",
                    help="处理所有子仓库，不限于 CM 开头")
    ap.add_argument("--no-root", action="store_true", help="不处理主仓库本身")
    ap.add_argument("--max-depth", type=int, default=3, help="递归子仓库深度，默认 3")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    if not is_git_repo(root):
        print(f"错误：{root} 不是 git 仓库", file=sys.stderr)
        return 1

    targets: List[Tuple[str, str]] = []
    if not args.no_root:
        targets.append((os.path.basename(root.rstrip("\\/")) or "Main Repo", root))
    for name, path in discover_repos(root, args.max_depth):
        if args.all_sub or name.startswith("CM"):
            targets.append((name, path))

    print(f"仓库根目录: {root}")
    print(f"模式      : {'APPLY（会修改远端）' if args.apply else 'DRY-RUN（只预览）'}")
    print(f"纳入范围  : {len(targets)} 个仓库\n")

    warned = 0
    changed_repos = 0

    for display, path in targets:
        remotes = list_remotes(path)
        print(f"=== {display}  ({path})")
        if not remotes:
            print("    (无远端)\n")
            continue
        for n, k in remotes.items():
            push = k.get("push")
            extra = f"   push={push}" if push and push != k.get("fetch") else ""
            print(f"    当前: {n:<12} {k.get('fetch', '')}{extra}")

        repo_name = derive_repo_name(path, remotes)
        changes, _ = plan_repo(display, path, repo_name)

        actionable = [c for c in changes if c.kind == "ok"]
        if not [c for c in changes if c.kind in ("ok", "warn")]:
            print("    (无需改动)\n")
            continue

        for ch in changes:
            print(f"    {SYMBOL.get(ch.kind, '?')} {ch.desc}")
            if ch.kind == "warn":
                warned += 1

        if actionable:
            changed_repos += 1
            if args.apply:
                apply_repo(path, changes)
            else:
                for ch in actionable:
                    print(f"      [计划] git remote {' '.join(ch.args)}")
        print()

    print("-" * 60)
    print(f"需要改动的仓库: {changed_repos}    告警: {warned}")
    if not args.apply and changed_repos:
        print("以上是预览，未做任何修改。确认无误后加 --apply 执行。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
