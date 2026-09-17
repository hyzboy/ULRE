# Gitea 配置 HTTPS 反向代理（openSUSE 15.6）

目标：外网走 `https://git.hyzgame.com/`（443），由反向代理把请求转给本机 `127.0.0.1:3000` 的 Gitea，证书用 Let's Encrypt 自动续期。

---

## 0. 开工前先确认 4 件事

```bash
# 1) 域名解析是否指向本机公网 IP
dig +short git.hyzgame.com
curl -4 ifconfig.co          # 对比服务器公网 IP

# 2) 80 / 443 是否可达（Let's Encrypt 的 HTTP-01 校验需要 80）
sudo ss -lntp | grep -E ':80|:443'      # 确认没被别的进程占
#   路由器/NAT 上要把 80、443 转发到这台机器

# 3) 服务器能出外网访问 ACME
curl -sI https://acme-v02.api.letsencrypt.org/directory | head -1

# 4) Gitea 现在监听在哪
sudo ss -lntp | grep 3000
sudo systemctl cat gitea | grep -i execstart    # 顺带看 -c 指定的 app.ini 路径
```

> 如果 **80 端口被运营商封了**，直接看第 5 节（DNS-01 校验）。

---

## 1. 三种方案怎么选

| 方案 | 复杂度 | 说明 |
|---|---|---|
| **A. nginx + certbot**（推荐） | 中 | openSUSE 官方源就有，最标准，出问题好查 |
| B. Caddy | 低 | 配置文件就 3 行、自动续期，但 openSUSE 官方源没包，要装二进制 |
| C. Gitea 内置 HTTPS | 低 | 不用反代，但要让 Gitea 绑 443/80，需要额外授权，且续期由 Gitea 自己管 |

下面以 **方案 A** 为主写全步骤，B / C 见第 3、4 节。

---

## 2. 方案 A：nginx + certbot

### 2.1 装包

```bash
sudo zypper refresh
sudo zypper install nginx python3-certbot python3-certbot-nginx certbot-systemd-timer
```

### 2.2 先拿证书（webroot 方式，不打断现有 Gitea）

```bash
sudo systemctl enable --now nginx          # openSUSE 默认站点根目录是 /srv/www/htdocs
sudo certbot certonly --webroot -w /srv/www/htdocs -d git.hyzgame.com
```

首次运行会问邮箱、同意条款。成功后证书在：

```
/etc/letsencrypt/live/git.hyzgame.com/fullchain.pem
/etc/letsencrypt/live/git.hyzgame.com/privkey.pem
```

### 2.3 写 vhost 配置

新建 `/etc/nginx/vhosts.d/gitea.conf`：

```nginx
# WebSocket 升级用的变量，需要放在 http{} 里
# 如果下面这段报 "map directive is not allowed here"，就单独存成 /etc/nginx/conf.d/ws_upgrade.conf
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

server {
    listen 80;
    listen [::]:80;
    server_name git.hyzgame.com;

    # ACME 续期校验必须保持 HTTP
    location /.well-known/acme-challenge/ {
        root /srv/www/htdocs;
    }

    # 其余全部跳 HTTPS
    location / {
        return 301 https://$host$request_uri;
    }
}

server {
    listen 443 ssl http2;          # nginx >= 1.25.1 改成两行： listen 443 ssl;  http2 on;
    listen [::]:443 ssl http2;
    server_name git.hyzgame.com;

    ssl_certificate     /etc/letsencrypt/live/git.hyzgame.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/git.hyzgame.com/privkey.pem;
    ssl_protocols       TLSv1.2 TLSv1.3;
    ssl_session_cache   shared:SSL:10m;
    ssl_session_timeout 1d;

    # Git 推送可能很大：0 = 不限制（也可用 512M）
    client_max_body_size 0;

    location / {
        proxy_pass http://127.0.0.1:3000;

        proxy_set_header Host              $host;
        proxy_set_header X-Real-IP         $remote_addr;
        proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_set_header X-Forwarded-Host  $host;
        proxy_set_header X-Forwarded-Port  $server_port;

        proxy_http_version 1.1;
        proxy_set_header Upgrade    $http_upgrade;
        proxy_set_header Connection $connection_upgrade;

        # 大仓库 clone / push 别被 60 秒掐断
        proxy_read_timeout       3600s;
        proxy_send_timeout       3600s;
        proxy_buffering          off;
        proxy_request_buffering  off;
    }
}
```

> 关键点：
> - `client_max_body_size` 默认是 1m，不放大就 **推大仓库直接 413**；
> - `X-Forwarded-Proto` 必须给，否则 Gitea 生成的链接还是 http；
> - `proxy_request_buffering off` 让 push 边收边转，避免 nginx 先落盘一整份。

检查并生效：

```bash
sudo nginx -t
sudo systemctl enable --now nginx
sudo systemctl reload nginx
```

### 2.4 防火墙

```bash
sudo firewall-cmd --permanent --add-service=http --add-service=https
sudo firewall-cmd --reload
sudo firewall-cmd --list-all
```

建议顺手把 3000 从外网关掉（只在你想强制走 HTTPS 时）：

```bash
sudo firewall-cmd --permanent --remove-port=3000/tcp
sudo firewall-cmd --reload
```

### 2.5 改 Gitea 配置

```bash
sudo systemctl cat gitea | grep -i execstart     # 找 -c 后面的路径
# 常见位置：/etc/gitea/app.ini  或  /var/lib/gitea/custom/conf/app.ini
sudo vim /etc/gitea/app.ini
```

```ini
[server]
DOMAIN        = git.hyzgame.com
SSH_DOMAIN    = git.hyzgame.com
HTTP_ADDR     = 127.0.0.1          ; 只让本机反代访问（可选，更安全）
HTTP_PORT     = 3000
ROOT_URL      = https://git.hyzgame.com/     ; 关键！决定网页上显示的 clone 地址
```

```bash
sudo systemctl restart gitea
sudo systemctl status gitea
```

### 2.6 自动续期

```bash
sudo systemctl enable --now certbot-renew.timer
sudo certbot renew --dry-run          # 必须跑一次，确认能续
```

让续期后自动 reload nginx（写进续期配置）：

```bash
sudo sh -c 'echo "deploy_hook = systemctl reload nginx" >> /etc/letsencrypt/renewal/git.hyzgame.com.conf'
```

> 注意：追加的行要落在 `[renewalparams]` 段内，用 `sudo vim` 打开确认一下。

### 2.7 验证

```bash
curl -I https://git.hyzgame.com/
curl -sI https://git.hyzgame.com/hyzboy/CMMath.git/info/refs?service=git-upload-pack | head -1
```

Windows 本机：

```bash
git ls-remote https://git.hyzgame.com/hyzboy/CMMath.git
```

---

## 3. 方案 B：Caddy（配置最少）

> **用 openSUSE 官方源的 caddy 包更简单**：包装好就自带 `caddy` 用户、systemd 服务和
> `AmbientCapabilities`，不用自己建用户、写单元。二进制在 `/usr/sbin/caddy`，
> 配置文件在 `/etc/caddy/Caddyfile`，改完 `sudo systemctl reload caddy`（或 `restart`）。
> 证书默认落在 `/var/lib/caddy/.local/share/caddy`。
>
> ⚠️ **装了系统包就别再照下面装二进制、也别自己写 `/etc/systemd/system/caddy.service`**。
> `/etc/systemd/system/` 下的同名单元会**覆盖**包自带的 `/usr/lib/systemd/system/caddy.service`，
> 一旦 ExecStart 指向 `/usr/local/bin/caddy`（那个下载失败的假文件），就会一直报
> `status=203/EXEC ... Failed to locate executable /usr/local/bin/caddy: Permission denied`。
> 已经写了的：`sudo rm /etc/systemd/system/caddy.service && sudo rm /usr/local/bin/caddy
> && sudo systemctl daemon-reload`，用包自带的单元即可。

官方源没有 Caddy 时用二进制（**`-L` 必须加**，否则下载到的是 302 跳转响应，
systemd 启动会报 `status=203/EXEC`）：

```bash
# 按架构选包
case "$(uname -m)" in
  x86_64|amd64) ARCH=amd64 ;;
  aarch64)      ARCH=arm64 ;;
  *) echo "不支持的架构: $(uname -m)"; exit 1 ;;
esac

sudo curl -fsSL -o /usr/local/bin/caddy \
  "https://caddyserver.com/api/download?os=linux&arch=${ARCH}"
sudo chmod 0755 /usr/local/bin/caddy
```

**装完必须先验证，别急着起服务**：

```bash
file /usr/local/bin/caddy        # 必须是: ELF 64-bit LSB executable, ... statically linked
ls -l /usr/local/bin/caddy       # 至少几十 MB；几百字节就是下载失败了
/usr/local/bin/caddy version     # 能打印版本号才算成功
```

如果 `file` 显示 `HTML document` / `ASCII text`，或只有几百字节，说明没下到真身，重下一次；
网络不通就用镜像：`https://ghproxy.net/https://github.com/caddyserver/caddy/releases/latest/download/caddy_linux_${ARCH}`

用户和目录：

```bash
sudo useradd -r -s /sbin/nologin caddy
sudo mkdir -p /etc/caddy /var/lib/caddy
sudo chown -R caddy:caddy /var/lib/caddy
```

`/etc/caddy/Caddyfile`：

```caddyfile
git.hyzgame.com {
    reverse_proxy 127.0.0.1:3000
    request_body {
        max_size 0
    }
}
```

systemd 单元 `/etc/systemd/system/caddy.service`：

```ini
[Unit]
Description=Caddy
After=network.target

[Service]
ExecStart=/usr/local/bin/caddy run --config /etc/caddy/Caddyfile
ExecReload=/usr/local/bin/caddy reload --config /etc/caddy/Caddyfile
Restart=always
User=caddy
Group=caddy
# 证书默认存在 $HOME/.local/share/caddy，caddy 是 nologin 用户没有家目录，必须指定
Environment=HOME=/var/lib/caddy
AmbientCapabilities=CAP_NET_BIND_SERVICE
NoNewPrivileges=true

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now caddy
```

Caddy 会自己去 Let's Encrypt 签证书并自动续期，不需要 certbot。

---

### 3.1 反代模式下 Gitea `app.ini` 怎么配（Caddy / nginx 通用）

只要 TLS 是反代在做，Gitea 这一侧**一律保持普通 HTTP**，不要开 `PROTOCOL=https`、
也不要开内置 Let's Encrypt。只改这几行：

```ini
[server]
DOMAIN       = git.hyzgame.com
SSH_DOMAIN   = git.hyzgame.com      ; 用 SSH 克隆时显示的地址，跟 HTTPS 无关，保持不变即可
ROOT_URL     = https://git.hyzgame.com/     ; ← 唯一必须改的关键项
HTTP_ADDR    = 127.0.0.1            ; 只让本机反代访问（推荐）
HTTP_PORT    = 3000                 ; 端口不动
```

```bash
sudo systemctl restart gitea
```

要点解释：

- **`ROOT_URL` 必须带 `https` 且以 `/` 结尾**。它决定：网页上显示的克隆地址、邮件链接、
  webhook 回调地址、部分静态资源 URL。不改的话页面上还是 `http://git.hyzgame.com:3000/...`。
- **`PROTOCOL` / `ENABLE_LETSENCRYPT` 不要开**，否则 Gitea 自己去签证书、跟 Caddy 抢 80/443，
  两边都起不来。
- **`HTTP_ADDR = 127.0.0.1`** 意思是只监听本机，外网只能经 Caddy 进来，更安全。
  代价是 `http://git.hyzgame.com:3000` 这种直连地址会失效——正好配合第 6 节把远端全改成 https。
- Caddy 的 `reverse_proxy` 默认就会自动加上 `X-Forwarded-For` / `X-Forwarded-Proto`，
  所以 Gitea 能正确识别 https，不需要额外写 header。
  只有日志里客户端 IP 显示成 127.0.0.1 时才需要显式开：
  ```ini
  [server]
  REVERSE_PROXY_LIMIT          = 1
  REVERSE_PROXY_TRUSTED_PROXIES = 127.0.0.1
  ```
- 大仓库推送：Caddy 默认流式转发不限制包体；若出现 413，在站点块里加
  ```caddyfile
  request_body {
      max_size 0
  }
  ```

验证：

```bash
curl -I https://git.hyzgame.com/                                  # 200
git ls-remote https://git.hyzgame.com/hyzboy/CMMath.git           # 能列出引用
```

## 4. 方案 C：Gitea 内置 HTTPS（不装反代）

`app.ini`：

```ini
[server]
PROTOCOL              = https
DOMAIN                = git.hyzgame.com
ROOT_URL              = https://git.hyzgame.com/
HTTP_PORT             = 443
ENABLE_LETSENCRYPT    = true
LETSENCRYPT_ACCEPTTOS = true
LETSENCRYPT_DIRECTORY = https
CERT_FILE             = /var/lib/gitea/https/cert.pem
KEY_FILE              = /var/lib/gitea/https/key.pem
```

绑定 443 需要授权（Gitea 默认以 git 用户运行）：

```bash
sudo systemctl edit gitea
```

```ini
[Service]
AmbientCapabilities=CAP_NET_BIND_SERVICE
```

```bash
sudo systemctl daemon-reload
sudo systemctl restart gitea
```

> 注意：内置 ACME 一般还需要 80 端口可达（HTTP-01 校验），所以 443 和 80 都得通。
> 这条路最省事，但证书和 Web 服务耦合在一起，出问题不好单独排查，个人更推荐反代方案。

---

## 5. 80 端口不通：改用 DNS-01 校验

```bash
sudo certbot certonly --manual --preferred-challenges dns -d git.hyzgame.com
```

按提示给 `_acme-challenge.git.hyzgame.com` 加一条 TXT 记录，等生效后回车。
缺点：续期也要手动加 TXT。想自动化就用 `acme.sh` + 你的 DNS 服务商 API。

---

## 6. 切到 HTTPS 后，仓库地址要同步改

改完后地址从 `http://git.hyzgame.com:3000/hyzboy/XXX.git` 变成 `https://git.hyzgame.com/hyzboy/XXX.git`，需要批量更新：

```bash
# 1) remotes.ini
#    hyzgame = https://git.hyzgame.com/hyzboy

# 2) 每个子仓库
git -C CMCore remote set-url hyzgame https://git.hyzgame.com/hyzboy/CMCore.git
```

ULRE 仓库里 10 个 CM 子仓库和主仓库都要改，可以用 `git_repos_cli.py` 批量做。
**注意**：如果你在 2.4 / 2.5 里把 3000 关了或改成只监听 127.0.0.1，老地址会立刻失效，必须改完再关。

---

## 7. 关于"git 非要连两次"

配了 HTTPS 之后，第一次 401、第二次带 `Authorization` 的现象**依然存在**——这是 HTTP Basic 认证的标准挑战-响应流程，跟 HTTP / HTTPS 无关（`Www-Authenticate: Basic realm="Gitea"`）。

好处是：密码不再明文传输，凭据管理器处理 HTTPS 源也更顺。

如果真想做到"一次就成功"（抢先认证），在本机配：

```bash
git config --global http.https://git.hyzgame.com/.extraHeader "Authorization: Basic <base64(用户名:Gitea访问令牌)>"
```

base64 用这个算：

```bash
printf 'hyzboy:你的token' | base64 -w0
```

建议用 Gitea 生成的**访问令牌**代替账号密码（Gitea → 设置 → 应用 → 生成令牌，勾 repo 权限）。

---

## 8. 排错清单

| 现象 | 原因 / 处理 |
|---|---|
| **`caddy.service` 起不来，`status=203/EXEC`** | systemd 执行不了该文件。依次查：`file /usr/local/bin/caddy` 是不是 ELF（不是就是下到 HTML/空文件了，加 `-L` 重下）→ `ls -l` 权限是否 755 → `uname -m` 与下载时选的 arch 是否一致 → `/usr/local` 所在分区是否带 `noexec`（`mount \| grep noexec`）|
| caddy 报 `listen tcp :443: bind: permission denied` | 以 caddy 用户运行但没有绑低端口的权限，`systemctl edit caddy` 加 `AmbientCapabilities=CAP_NET_BIND_SERVICE` + `CapabilityBoundingSet=CAP_NET_BIND_SERVICE` |
| caddy 报打开 Caddyfile / 写证书目录 `permission denied` | `sudo chmod 0644 /etc/caddy/Caddyfile`；`sudo chown -R caddy:caddy /var/lib/caddy` |
| caddy 起不来，日志是 AppArmor DENIED | `sudo aa-status \| grep caddy`；临时 `sudo aa-complain /usr/sbin/caddy` 验证，确认后改 profile |
| caddy 签不到证书 | 80/443 必须都通（ACME HTTP-01 用 80，TLS-ALPN 用 443）；看 `journalctl -u caddy -n 50` |
| `nginx -t` 报 `map` 不允许在此 | `map` 要放 `http{}`，单独存 `/etc/nginx/conf.d/ws_upgrade.conf` |
| `listen ... http2` 告警 | nginx ≥ 1.25.1 改用 `listen 443 ssl;` + `http2 on;` |
| push 报 413 | `client_max_body_size` 没设 |
| 页面资源是 http、样式挂 | 少给 `X-Forwarded-Proto`，或 `ROOT_URL` 没改成 https |
| 网页 clone 地址还是 :3000 | `ROOT_URL` 改完没重启 Gitea |
| nginx 502 | Gitea 没起 / `HTTP_ADDR` 改成 127.0.0.1 后 `proxy_pass` 还写的别的地址 |
| nginx 连接被拒（AppArmor） | `sudo aa-status | grep nginx`，必要时 `sudo aa-complain /usr/sbin/nginx` |
| certbot 校验超时 | 80 端口没放行，或域名解析还没生效 |
