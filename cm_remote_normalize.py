#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
cm_remote_normalize.py

规范化 ULRE 主仓库及其子仓库（名称以 CM 开头）的 git 远端。

规则
1. hyzgame 老地址改写
   远端 URL 以 http://git.hyzgame.com:3000/hyzboy/ 开头的，
   统一改成 https://git.hyzgame.com/hyzboy/ 开头，并且该远端改名为 hyzgame。

2. github 远端改名
   如果某个仓库里「只有一个」指向 github.com/hyzboy/ 的远端，
   则把这个远端改名为 github（无论它原来叫什么，常见是 origin）。
   若同时存在多个，打印告警并跳过，避免误改。

3. 兜底补齐 hyzgame
   检查是否同时存在：名为 hyzgame 的远端 + 指向 git.hyzgame.com 的远端。
   两者缺其一，就把 hyzgame 补齐（能复名就复名，不能就新建），
   新地址由现有 github 远端或目录名推导：https://git.hyzgame.com/hyzboy/<Repo>.git

4. 补齐 github
   只存在 hyzgame 远端、没有 github 远端时，补一个 github 远端（SSH 形式）。

5. github 远端一律改写成 SSH 形式
   hyzboy 名下的 GitHub 远端不管原来写成哪种形态，都统一成
   git@github.com:hyzboy/<Repo>.git：

       https://github.com/hyzboy/CMProject.git       https://github.com/hyzboy/CMProject
       http://github.com/hyzboy/CMProject            git://github.com/hyzboy/CMProject.git
       https://<user>:<token>@github.com/hyzboy/CMProject.git
       https://www.github.com/hyzboy/CMProject       https://github.com:443/hyzboy/CMProject
       ssh://git@github.com/hyzboy/CMProject.git     ssh://git@github.com:22/hyzboy/CMProject
       git@github.com:hyzboy/CMProject               git@github.com:hyzboy/CMProject.git/

   单独配置的 pushurl 也按同一规则处理。
   非 hyzboy 名下的 GitHub 远端（第三方子模块）只提示、不修改。

用法
    python cm_remote_normalize.py                 # 默认 dry-run，只打印将要做的改动
    python cm_remote_normalize.py --apply         # 真正执行
    python cm_remote_normalize.py --all-sub       # 处理所有子仓库，不限于 CM 开头
    python cm_remote_normalize.py --no-root       # 不处理主仓库本身
    python cm_remote_normalize.py --root D:\ULRE  # 指定仓库根目录
"""

import argparse
import os
import re
import subprocess
import sys
from typing import Dict, List, Optional, Tuple

# ---------------------------------------------------------------- 配置常量

OLD_HYZ_PREFIX = "http://git.hyzgame.com:3000/hyzboy/"
NEW_HYZ_PREFIX = "https://git.hyzgame.com/hyzboy/"

HYZ_NAME = "hyzgame"
GH_NAME = "github"
HYZ_HOST = "git.hyzgame.com"
HYZ_BASE = NEW_HYZ_PREFIX                       # 新增 hyzgame 远端时的基址

GH_OWNER = "hyzboy"                             # 只处理这个账号名下的 GitHub 仓库
GH_SSH_HOST = "github.com"
GH_SSH_BASE = f"git@{GH_SSH_HOST}:{GH_OWNER}/"


# ---------------------------------------------------------------- GitHub URL 解析

# 带协议的形态：https://  http://  git://  ssh://  （可含 user[:pass]@、www.、:port）
_GITHUB_URL_RE = re.compile(
    r"^(?:git\+)?(?:https?|git|ssh)://"
    r"(?:[^@/]+@)?"
    r"(?:www\.)?github\.com"
    r"(?::\d+)?"
    r"(?P<path>/[^?#]*)"
    r"$",
    re.IGNORECASE,
)

# scp 形态：git@github.com:owner/repo(.git)
_GITHUB_SCP_RE = re.compile(
    r"^(?:[^@/]+@)?(?:www\.)?github\.com:(?P<path>.+)$",
    re.IGNORECASE,
)


def parse_github(url: str) -> Optional[Tuple[str, str]]:
    """把任意形式的 GitHub 远端 URL 解析成 (owner, repo)；不是 GitHub 地址返回 None。"""
    text = (url or "").strip()
    if not text:
        return None
    m = _GITHUB_URL_RE.match(text) or _GITHUB_SCP_RE.match(text)
    if not m:
        return None
    path = m.group("path").strip("/")
    if path.endswith(".git"):
        path = path[: -len(".git")]
    parts = [p for p in path.split("/") if p]
    if len(parts) != 2:          # 只认 owner/repo 两段，多出来的层不猜
        return None
    return parts[0], parts[1]


def github_ssh_url(owner: str, repo: str) -> str:
    """拼 SSH 形式的 GitHub 地址：git@github.com:owner/repo.git。"""
    repo = repo[: -len(".git")] if repo.endswith(".git") else repo
    return f"git@{GH_SSH_HOST}:{owner}/{repo}.git"


def canonical_github_url(url: str) -> Optional[str]:
    """任意 GitHub URL -> 规范的 SSH 地址；不是 GitHub 地址返回 None。"""
    parts = parse_github(url)
    return github_ssh_url(*parts) if parts else None


def is_hyz_github(url: str) -> bool:
    """是否指向 GH_OWNER 名下的 GitHub 仓库（任意 URL 形态）。"""
    parts = parse_github(url)
    return bool(parts) and parts[0].lower() == GH_OWNER.lower()


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


def list_remotes(path: str, git_dir: Optional[str] = None) -> Dict[str, Dict[str, str]]:
    """返回 {远端名: {"fetch": url, "push": url}}，保持 git 输出顺序。

    git_dir 非空时读取 .git/modules/*/config 这类配置，不依赖工作树目录。
    """
    cmd = ([] if git_dir is None else ["--git-dir", git_dir]) + ["remote", "-v"]
    code, out, _ = run_git(cmd, path)
    if code != 0:
        return {}
    remotes: Dict[str, Dict[str, str]] = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        name, url, kind = parts[0], parts[1], parts[2].strip("()")
        remotes.setdefault(name, {})[kind] = url
    return remotes


def get_pushurl(path: str, name: str, git_dir: Optional[str] = None) -> Optional[str]:
    """该远端是否单独配置了 pushurl；配了才需要单独维护 --push。"""
    cmd = ([] if git_dir is None else ["--git-dir", git_dir])
    code, out, _ = run_git(cmd + ["config", "--get", f"remote.{name}.pushurl"], path)
    return out if code == 0 and out else None


def ensure_git_suffix(url: str) -> str:
    """补齐 .git 后缀，和主仓库 / github 远端的地址风格保持一致。"""
    base = url.split("?", 1)[0].rstrip("/")
    return url if base.endswith(".git") else base + ".git"


def fetch_url(kinds: Dict[str, str]) -> str:
    return kinds.get("fetch") or kinds.get("push") or ""


def has_hyzgame_like_url(urls) -> bool:
    """是否存在 hyzgame 相关地址（旧地址或指向 git.hyzgame.com）。"""
    for url in urls:
        if not url:
            continue
        if url.startswith(OLD_HYZ_PREFIX) or HYZ_HOST in url:
            return True
    return False


def find_submodule_gitdirs(root: str) -> List[str]:
    """遍历 .git/modules 下所有已初始化 submodule 的 gitdir。"""
    modules_root = os.path.join(root, ".git", "modules")
    if not os.path.isdir(modules_root):
        return []
    result: List[str] = []
    for current, _, files in os.walk(modules_root):
        if "config" in files:
            result.append(os.path.normpath(current))
    return sorted(result)


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
    """推导新地址用的 <Repo>.git：优先从 github 远端取，其次 hyzgame，最后用目录名。"""
    for kinds in remotes.values():
        for kind in ("fetch", "push"):
            url = kinds.get(kind, "")
            if not url:
                continue
            parts = parse_github(url)
            if parts:
                return parts[1] + ".git"
            if url.startswith(HYZ_BASE) or url.startswith(OLD_HYZ_PREFIX):
                base = HYZ_BASE if url.startswith(HYZ_BASE) else OLD_HYZ_PREFIX
                suffix = url[len(base):]
                return suffix if suffix.endswith(".git") else suffix + ".git"
    return os.path.basename(path.rstrip("\\/")) + ".git"


def add_change(changes: List[Change], ch: Change) -> None:
    """避免重复的远端命令被重复安排。"""
    if any(c.kind == ch.kind and c.args == ch.args and c.desc == ch.desc for c in changes):
        return
    changes.append(ch)


def plan_remotes(display: str, path: str, repo_name: str,
                 remotes: Dict[str, Dict[str, str]],
                 git_dir: Optional[str] = None) -> Tuple[List[Change], List[str]]:
    """基于已解析好的 remote 映射生成改动计划。支持普通工作树和 .git/modules gitdir。

    计划过程中维护一份「模拟状态」（fetch / pushurl），每条规则都读改写后的状态、
    并按执行顺序产生命令，避免「rename 之后 set-url 还用旧名字」这类错位。
    """
    changes: List[Change] = []
    if not remotes:
        add_change(changes, Change("warn", "无远端（或不是 git 仓库），跳过"))
        return changes, []

    fetch: Dict[str, str] = {n: fetch_url(k) for n, k in remotes.items()}
    pushurl: Dict[str, str] = {}
    for name in list(fetch):
        pu = get_pushurl(path, name, git_dir)
        if pu:
            pushurl[name] = pu

    def rename(old: str, new: str) -> None:
        if old == new or old not in fetch or new in fetch:
            return
        add_change(changes, Change("ok", f"rename 远端 '{old}' -> '{new}'",
                                   ["rename", old, new]))
        fetch[new] = fetch.pop(old)
        if old in pushurl:
            pushurl[new] = pushurl.pop(old)

    def set_fetch(name: str, url: str, note: str = "") -> None:
        old = fetch.get(name, "")
        if old == url:
            return
        add_change(changes, Change("ok", f"set-url {name}: {old} -> {url}{note}",
                                   ["set-url", name, url]))
        fetch[name] = url

    def set_push(name: str, url: str, note: str = "") -> None:
        old = pushurl.get(name, "")
        if old == url:
            return
        add_change(changes, Change("ok", f"set-url --push {name}: {old} -> {url}{note}",
                                   ["set-url", "--push", name, url]))
        pushurl[name] = url

    def add_remote(name: str, url: str, desc: str) -> None:
        add_change(changes, Change("ok", desc, ["add", name, url]))
        fetch[name] = url

    # ---- 规则 1：老 hyzgame 地址 -> 新地址，并改名 hyzgame
    old_hyz = [(n, u) for n, u in fetch.items() if u.startswith(OLD_HYZ_PREFIX)]
    for name, url in old_hyz:
        new_url = ensure_git_suffix(NEW_HYZ_PREFIX + url[len(OLD_HYZ_PREFIX):])
        if name != HYZ_NAME:
            if HYZ_NAME in fetch:
                if fetch[HYZ_NAME] != new_url:
                    add_change(changes, Change(
                        "warn",
                        f"远端 '{HYZ_NAME}' 已存在且指向 {fetch[HYZ_NAME]}，"
                        f"未自动处理 '{name}'（{url}）以免冲突"))
                else:
                    add_change(changes, Change(
                        "warn",
                        f"远端 '{HYZ_NAME}' 已存在且地址相同，'{name}'（{url}）保持原样"))
                continue
            rename(name, HYZ_NAME)
        set_fetch(HYZ_NAME, new_url)

    # 单独配置的 pushurl 里的老地址同样改写（放在 rename 之后，名字已是新名）
    for name, url in list(pushurl.items()):
        if url.startswith(OLD_HYZ_PREFIX):
            set_push(name, ensure_git_suffix(NEW_HYZ_PREFIX + url[len(OLD_HYZ_PREFIX):]))

    # ---- 规则 2：唯一的 github.com/hyzboy/ 远端 -> 改名为 github
    all_gh = [(n, u) for n, u in fetch.items() if is_hyz_github(u)]
    if GH_NAME in fetch:
        if not is_hyz_github(fetch[GH_NAME]) and all_gh:
            add_change(changes, Change(
                "warn",
                f"远端 '{GH_NAME}' 指向的不是 {GH_OWNER} 名下的 GitHub 仓库"
                f"（{fetch[GH_NAME]}），同时还存在 {len(all_gh)} 个同账号远端，未自动改名"))
    elif len(all_gh) == 1:
        rename(all_gh[0][0], GH_NAME)
    elif len(all_gh) > 1:
        detail = ", ".join(f"{n}({u})" for n, u in all_gh)
        add_change(changes, Change(
            "warn", f"存在多个 {GH_OWNER} 名下的 GitHub 远端，跳过改名以免误伤：{detail}"))

    # ---- 规则 3：兜底补齐 hyzgame（名为 hyzgame + 指向 git.hyzgame.com）
    if HYZ_NAME in fetch and HYZ_HOST in fetch[HYZ_NAME]:
        set_fetch(HYZ_NAME, ensure_git_suffix(fetch[HYZ_NAME]), "（补齐 .git）")

    has_named = HYZ_NAME in fetch
    has_host = has_hyzgame_like_url(list(fetch.values()) + list(pushurl.values()))
    if not (has_named and has_host):
        if has_named:
            set_fetch(HYZ_NAME, HYZ_BASE + repo_name, f"（原地址不含 {HYZ_HOST}）")
        else:
            candidates = [n for n, u in fetch.items() if HYZ_HOST in u]
            if len(candidates) == 1:
                rename(candidates[0], HYZ_NAME)
                set_fetch(HYZ_NAME, ensure_git_suffix(fetch[HYZ_NAME]), "（补齐 .git）")
            else:
                url = HYZ_BASE + repo_name
                add_remote(HYZ_NAME, url, f"add 远端 {HYZ_NAME} -> {url}")

    # ---- 规则 4：只有 hyzgame 远端、没有 github，则补齐 github（SSH 形式）
    #      已有别家名下的 GitHub 远端 = 这是第三方仓库，补一个 hyzboy 的空远端没有意义
    other_gh = [(n, u) for n, u in fetch.items()
                if parse_github(u) and not is_hyz_github(u)]
    if has_hyzgame_like_url(list(fetch.values())) and GH_NAME not in fetch and not all_gh:
        if other_gh:
            detail = ", ".join(f"{n}({u})" for n, u in other_gh)
            add_change(changes, Change(
                "info",
                f"已有别家 GitHub 远端 {detail}，未自动补齐 {GH_NAME} 远端"))
        else:
            url = github_ssh_url(GH_OWNER, repo_name)
            add_remote(GH_NAME, url,
                       f"add 远端 {GH_NAME} -> {url}（仅存在 hyzgame 远端，补齐 github）")

    # ---- 规则 5：GitHub 远端一律改写成 SSH 形式 git@github.com:<owner>/<repo>.git
    for name, url in list(fetch.items()):
        parts = parse_github(url)
        if not parts:
            continue
        if parts[0].lower() != GH_OWNER.lower():
            add_change(changes, Change(
                "info",
                f"远端 '{name}' 指向非 {GH_OWNER} 名下的 GitHub 仓库（{url}），保持原样"))
            continue
        set_fetch(name, github_ssh_url(*parts), "（改为 SSH）")

    for name, url in list(pushurl.items()):
        parts = parse_github(url)
        if not parts or parts[0].lower() != GH_OWNER.lower():
            continue
        set_push(name, github_ssh_url(*parts), "（改为 SSH）")

    return changes, []


def plan_repo(display: str, path: str, repo_name: str) -> Tuple[List[Change], List[str]]:
    """针对单个仓库生成改动计划。不修改任何东西。返回 (changes, resulting_state)。"""
    remotes = list_remotes(path)
    return plan_remotes(display, path, repo_name, remotes)


def apply_repo(path: str, changes: List[Change], git_dir: Optional[str] = None) -> None:
    for ch in changes:
        if ch.kind != "ok" or not ch.args:
            continue
        if ch.args[0] in ("rename", "add", "set-url"):
            cmd = [] if git_dir is None else ["--git-dir", git_dir]
            cmd += ["remote"] + ch.args
            code, _, err = run_git(cmd, path)
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
        if not [c for c in changes if c.kind in ("ok", "warn", "info")]:
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

    # 修正 submodule 的独立 gitdir 配置（.git/modules/.../config）
    for git_dir in find_submodule_gitdirs(root):
        rel = os.path.relpath(git_dir, os.path.join(root, ".git", "modules"))
        display = os.path.basename(rel) if rel and rel != "." else os.path.basename(root)
        remotes = list_remotes(root, git_dir)
        if not remotes:
            continue
        repo_name = derive_repo_name(git_dir, remotes)
        changes, _ = plan_remotes(display, git_dir, repo_name, remotes, git_dir)
        actionable = [c for c in changes if c.kind == "ok"]
        if not [c for c in changes if c.kind in ("ok", "warn", "info")]:
            continue
        print(f"=== [submodule-gitdir] {display}  ({git_dir})")
        for ch in changes:
            print(f"    {SYMBOL.get(ch.kind, '?')} {ch.desc}")
            if ch.kind == "warn":
                warned += 1
        if args.apply:
            apply_repo(root, changes, git_dir)
        else:
            for ch in actionable:
                print(f"      [计划] git --git-dir {git_dir} {' '.join(ch.args)}")
        print()

    print("-" * 60)
    print(f"需要改动的仓库: {changed_repos}    告警: {warned}")
    if not args.apply and changed_repos:
        print("以上是预览，未做任何修改。确认无误后加 --apply 执行。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
