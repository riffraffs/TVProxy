# TVProxy 开发计划

> 依据：`docs/prd.md`（产品范围）、`docs/technical.md`（技术事实）、`docs/ui-prototype/index.html`
> 目标平台：华为智慧屏 V 系列（鸿蒙 2.0）
> 包名 `com.tvproxy`；`minSdk 21` / `compileSdk 35` / `targetSdk 29`
> **本文是实现状态的唯一维护处**（已完成 / 未做 / 下一步 / 遗留）；prd 与 technical 不维护进度。

---

## 原则

1. 先验证 VpnService 授权弹窗，再接 native（技术方案第六节，最高优先级风险）。
2. 先跑通 SOCKS5，再做 HTTP CONNECT。
3. UI 以原型为准：鸿蒙智慧屏深色风格、遥控器 DPAD 焦点、不支持认证。
4. native 用 ndk-build + `hev-socks5-tunnel`；ABI：`armeabi-v7a`、`arm64-v8a`（真机）、`x86_64`（模拟器）。

包名：`com.tvproxy`  
VpnService 类名：`TvProxyVpnService`  
`minSdk 21` / `compileSdk 34` / `targetSdk 29`（对齐鸿蒙 2.0 AOSP 层）

---

## 原型范围（要做）

| 区域 | 内容 |
|------|------|
| 标题 | TVProxy、副标题「智慧屏全局网络代理」 |
| 代理状态卡 | 状态胶囊（未开启 / 运行中）、本机局域网 IP（只读、大号） |
| 代理设置卡 | 顶边与状态卡对齐；代理类型、服务器地址、端口 |
| 操作 | 「保存并启动」（运行中改为「保存并重启」）、「停止代理」（未开启时不可用） |
| 弹层 | 首次保存弹出 VPN 授权说明；地址/端口获焦后系统 TV 键盘 |
| 焦点 | 类型 → 地址 → 端口 → 保存 → 停止；蓝框高亮 |

## 原型范围（不做进 App）

- HTML 电视外框、模拟状态栏时钟 / Wi-Fi 图标
- 底部「HarmonyOS 2.0 · HTML 原型」角标
- 自绘数字键盘（真机用系统 IME）
- 已从原型删除的说明文案与流量路径图

---

## P0 工程脚手架

- 用 `tool/env.bat` 建 Kotlin + Leanback 模块 `app`。
- `local.properties` 指向 `tool/android-sdk` 与 NDK `27.3.13750724`。
- Manifest：`LEANBACK_LAUNCHER`、`INTERNET`、`FOREGROUND_SERVICE`；声明 `TvProxyVpnService` + `BIND_VPN_SERVICE`。
- `ndk.abiFilters`：`armeabi-v7a, arm64-v8a, x86_64`。
- 空 `MainActivity`，能在 AVD `tvproxy-tv-1080p` 上打开。

**完成：** `gradlew assembleDebug` 成功，模拟器装上 APK。

---

## P1 VpnService 冒烟

不接 tun2socks，只验证授权与 tun。

- `ProxyConfig`：`protocol` / `host` / `port`（本阶段可写死 SOCKS5）。
- `TvProxyVpnService`：前台通知 + `Builder`（session=`TVProxy`，地址 `10.0.0.2/24`，路由 `0.0.0.0/0`，MTU 1500）。
- Activity：`VpnService.prepare()` → 系统授权 → `startService`；提供停止。
- 优先真机（华为 V 系列），模拟器同步。

**完成：** 授权框可同意；系统 VPN 列表出现本应用；回桌面通知仍在。鸿蒙弹窗异常则停在本阶段，不进入 P2。

---

## P2 SOCKS5 闭环

- submodule：`app/src/main/jni/hev-socks5-tunnel`。
- 修改 `hev-jni.c`：`PKGNAME "com/tvproxy"`，`CLSNAME "TvProxyVpnService"`。
- `ndk-build` 产出 `.so`；`nativeStartTun2socks` / `nativeStop`。
- `establish()` 后 `detachFd()` 交给 native。
- 本机 `tool\start-gost.bat`（`socks5://:10800`，避开 Hyper-V 占用的 1080）。模拟器填 `10.0.2.2:10800`，真机填电脑局域网 IP。

**完成：** 电视浏览器走 gost；停 VPN 后恢复直连。失败则改备选 `go-tun2socks`。

---

## P3 UI 与配置（对齐原型）

按 `docs/ui-prototype/index.html` 落地 Leanback 页，DPAD 全程可操作。

- 左：标题 + 状态卡（胶囊、局域网 IP）。
- 右：设置卡（类型 Spinner、地址、端口），顶边与状态卡对齐。
- 保存写入 `SharedPreferences` 并（重新）启动 VpnService；启动回填。
- 获焦地址/端口弹出系统 TV 键盘。
- 视觉：深色底、宇宙蓝 `#0A59F7`、大圆角卡片、焦点蓝框；适配 720p / 1080p / 4K。
- 状态：未开启 / 运行中；保存按钮文案随状态切换。

**完成：** 遥控器可配可存可开关；杀进程再开配置仍在。

---

## P4 HTTP 与 DNS

对齐「手机 Wi‑Fi 手动代理」：电视填 Clash 电脑 IP + 混合端口；分流只在 Clash。

**一期（已完成，2026-09-05 真机修正）：** SOCKS5 全局进上游。DNS 初始沿用 Wi-Fi DHCP（真机发现国内 DNS 污染国外域名导致打不开，已改为 VPN DNS = `8.8.8.8` + `1.1.1.1`，查询经隧道到 Clash、由代理出口解析；DHCP 仅兜底）。HTTP 类型仍按 SOCKS5 连接同一地址端口。

**二期（未做）：** native HTTP CONNECT（对齐手机 Wi‑Fi 代理协议）。

**已放弃：** mapdns / 本地 DNS 中继（0.1.12 曾用 PC 53→7874 中继实现 Clash fake-ip，公共 DNS 走代理出口已满足需求，无需中继）。

---

## P5 保活与鸿蒙适配

- 前台通知：「TVProxy 运行中」。
- `START_STICKY`；被杀后按已存配置拉起。
- 首次成功后短文案：到「应用启动管理」允许后台运行。
- 不做认证、不做分应用绕过。

**完成：** 回桌面、开其他 App，代理仍在（需真机确认通知不被清）。

---

## P6 真机验收

- 华为 V 系列：授权弹窗、U 盘侧载、纯净模式拦截。
- 遥控器焦点、键盘、返回键不误停 VPN。
- 720p / 1080p / 4K 各看一眼。
- Release 签名 APK；真机 ABI 至少 `armeabi-v7a` + `arm64-v8a`。
- 对照 PRD 走一遍：IP、类型、地址、端口、保存、全局代理。

---

## 建议目录

```
app/src/main/
  java/com/tvproxy/
    MainActivity.kt
    ProxyConfig.kt
    ProxyPrefs.kt
    TvProxyVpnService.kt
  jni/hev-socks5-tunnel/     ← submodule
  res/layout/
  AndroidManifest.xml
```

---

## 当前进度

| 项 | 状态 |
|----|------|
| 开发环境 `tool/` | 已完成 |
| UI 原型 | 已完成（`docs/ui-prototype/`） |
| P0 脚手架 | 已完成 |
| P1 VpnService | 已完成（模拟器） |
| P2 SOCKS5 闭环 | 已完成（hev + gost `:10800`） |
| P3 UI 与配置 | 已完成 |
| P4 一期 SOCKS5 + 系统 DNS | 已完成（真机修正：DNS 改 8.8.8.8/1.1.1.1 走代理出口，见 P4 段） |
| P4 二期 HTTP CONNECT | 未做 |
| P5 保活 / 鸿蒙 | 已完成（通知 + START_STICKY 早前已具备；补首次成功「应用启动管理」一次性引导） |
| P6 真机验收 | 部分完成：华为 THAL-560 冒烟通过（授权、保存启动不闪退、国内/国外站点可访问，0.1.13）；保活时长等留待正式验收 |
| 真机闪退（native protect 调用） | 已修复：不对上游 socket 调 `VpnService.protect()`（0.1.11） |
| 国外网站打不开（DNS 污染） | 已修复：VPN DNS 改 8.8.8.8 / 1.1.1.1，查询经 Clash 代理出口解析（0.1.13） |
| 代理类型 UI 对齐原型 | 已完成（0.1.14）：下拉框改为原型左右箭头步进；类型行获焦时遥控器左右键循环 SOCKS5/HTTP，上下键照常移焦点；真机 THAL-560 验证切换/环绕/保存持久化（logcat `establishing tun for http …`） |
| 首启弹窗文案与白框 | 已完成（0.1.15）：说明正文 4+ 行精简到 2 行；Dialog 改用透明窗口背景主题（`Theme.TVProxy.Dialog`）去掉鸿蒙 TV 默认窗框——真机逐像素确认卡片外侧浅灰辉光环消失，模拟器+真机均验证 |
| 服务器地址改 4 段 IPv4 输入 | 已完成（0.1.16）：地址行由整段文本框拆为 4 个数字段（各 1–3 位，中间自动带点），点击/OK 只重输当前段，段间左右键一次一跳、键盘 Next 顺段跳转；保存时逐段去前导零并校验 0–255（段 999 保存时红字提示、不误启）。取舍：按需求确认只填局域网 IPv4，放弃域名/主机名输入；端口维持整段输入。原型 index.html 已同步为四段式。验证：模拟器 + 真机 THAL-560 均跑通「192.168.3.101 只把末段改成 201」（段4 全选替换，其余三段不变），真机 OK 弹出系统数字键盘、键入选段生效、重启回填正常 |
| 报错红字位置（按钮被挤出设置卡） | 已修复（0.1.17）：红字报错原内联在右侧设置卡端口行与按钮之间——长报错（如 VPN 未授权指引）会把卡内弹性空间压到 0，把「保存并启动/停止代理」按钮推出卡底（该屏按钮上方仅约 22dp 余量，卡底紧贴屏幕底）。现把报错区移到左侧状态卡 `lan_ip` 下方（左卡只占左列顶部、下方有大段空白）：错误时左卡向下生长、完整多行红字可读，消失时缩回，右侧设置卡与按钮布局零改动。验证：真机 THAL-560（3840×2160/density 640）`appops ACTIVATE_VPN` deny 后点保存并启动，长报错落在左卡 `[256,1116][1489,1720]`、两按钮 bounds 与空闲态完全一致（`btn_save [1817,1744][2796,1968]`、`btn_stop [2860,1744][3560,1968]`）；allow 后保存启动→运行中→停止→重开均正常、错误自动清除 |
| Loon 适配（TCP-only 上游 DNS-over-TCP 回落） | 已完成（0.1.18）：实测 iOS Loon 局域网共享的 SOCKS5 无 UDP 中继（UDP ASSOCIATE 应答 BND=0.0.0.0:0），而其 TCP CONNECT 正常 → 原「DNS 经 SOCKS5-UDP 走代理出口」方案失效，国外域名解析挂。启动时 `UpstreamProbe`（后台线程）探测上游 UDP 能力；无 UDP 则进入 DNS-over-TCP 模式：native 在 tun 读循环拦截 dst UDP:53 查询，经上游 TCP CONNECT 到解析器做 DNS-over-TCP（RFC 7766 帧），应答拼回 UDP 写 tun（新增 `hev-dns-tcp.c`，配置键 `misc.dns-over-tcp`），DNS 列表切为 223.5.5.5/119.29.29.29/8.8.8.8/1.1.1.1（不带 DHCP 兜底）。验证（真机 THAL-560）：Loon 场景探测 `dns-over-tcp=true`，`ping www.google.com/baidu/qq` 均解析出真实 IP（此前 unknown host）；TV Bro 浏览器端到端打开 Google 首页，hev 日志 `[dns] tcp ok` 且本段 0 错误；Clash 回归：上游=PC mihomo 7897 探测 BND 真实（`c0 a8 03 c9 1e d9`）→ 仍走 UDP 模式、DNS 列表不变、解析正常。边界（不做）：QUIC 等非 DNS UDP 经 TCP-only 上游仍不可用；IPv6 DNS/分片包/超 1472B 应答不支持；上游需认证不支持 |
| Loon 网速慢（对比 Clash）分析与修复 | 已完成（0.1.19）：① 实测基线：0.1.18 + Loon 上游待机 4 分钟日志 ~327 行/分、`client udp construct` ~55/分、`io timeout` ~70/分 —— 每个非 DNS UDP 包（QUIC/NTP/系统探测等）都建一个注定失败的 SOCKS5 UDP-ASSOCIATE 会话，空转刷屏还给上游添连接压力。② 修复：`dns-over-tcp` 模式下非 DNS UDP 在 tun 读循环直接丢弃并按端口计数（每 10s 汇总一条 `[udp] tcp-only drop total=… dns=… quic443=…`，杜绝将死会话，UDP 模式完全不受影响）；DNS 并发满（8）时从「落回 lwIP UDP 死路（静默丢失）」改为有界排队（32，满才丢并记 WARN）；每个 DNS-over-TCP 查询打 INFO（解析器 IP、qname、cost/wait ms）。③ UI：运行中且 TCP-only 上游时状态卡显示黄色提示（QUIC/UDP 不可用，视频类回落 TCP，建议换支持 UDP 的中继）。验证（真机 THAL-560 + PC 复刻）：Clash（UDP）回归——探测 BND 真实、`dns-over-tcp=false`、YouTube 播放与 0.1.18 一样快、hev.log 0 条 tcp-only drop；TCP-only 复刻（PC 起 TCP-only SOCKS5 中继链到 Clash 出口，模拟 Loon 约束且出口与 Clash 完全一致）——0 UDP 会话 churn、DNS-over-TCP 正常（223.5.5.5/119.29.29.29 快速失败→8.8.8.8 成功约 210-275ms/次）、YouTube 能播但用户反馈明显比 UDP 路径慢 ⇒ 主因确认为上游无 UDP（QUIC 不可用 + 每域名 DNS-over-TCP 开销），非 Loon 设备或 TVProxy 栈本身。遗留：真机 Loon 直连 A/B 未跑成（TV↔iPhone 不同频段/休眠）；qname 打点偏移已修源码；`UpstreamProbe` connect 超时 1.5s 对慢接受的上游可能误判 |
| Clash 路径 YouTube 2160p 打不开诊断与修复 | 已完成 0.1.20/0.1.21/0.1.22（2026-09-06 晚 ~09-07）：① 0.1.20 加 `[stats]` per-protocol 吞吐打点；② 0.1.21 加 `drop-quic`（UDP 模式下丢 UDP:443 + ICMP Port Unreachable，Prefs 键 `drop_quic`）；③ 0.1.22 修复 `hev_dns_tcp_udp_dst_port` 端口字节偏移 bug（之前取成源端口，drop-quic 一直没拦到）。测量结论：隧道/Wi-Fi 非瓶颈（LAN 下行 168Mbps/多、43Mbps/单）；**WAN 经节点：单 TCP 32–52Mbps、8 连接 100–136Mbps**（都够 2160p）；问题根因是**出口节点 UDP(QUIC) 路不稳（电视端应用优先 QUIC 只能拿到 1–4.6MB/s 锯齿），TCP 路稳**，PC Chrome 走 TCP 所以 2160p 流畅。修复后（drop-quic 生效）：`quic_drop/icmp` 计数出现、YouTube/TV Bro 全走 TCP；**TV Bro 1440p 已无加载卡顿**；**2160p 仍偶卡，剩余疑为 TV Bro/电视解码渲染能力，而非链路**（TCP 突发 23–45Mbps）。环境：Clash 混合端口因 Windows 排除段 7802–7901 无法再绑 7897，**已改为 10801**。当前：TVProxy 运行中、上游 192.168.3.201:10801、`drop_quic=true`。**收尾验证（2026-09-07）：安装 SmartTube（org.smarttube.stable 32.38）后 4K/2160p 播放「完美、不卡顿」——链路与 TVProxy 达标，之前 2160p 卡顿归因于 YouTube/TV Bro 客户端（死咬 QUIC 或浏览器解码性能），已解决**。遗留：`drop-quic` 暂无 UI 开关——改值暂需经 SharedPreferences `drop_quic` 键或代码默认值，后续加开关不用动 native |

下一步：HTTP CONNECT。不做：认证、电视侧分流、CMake、手机版 UI、主机名/域名输入。
P6 遗留：保活时长、720p/1080p/4K 分辨率与 Release 签名 APK 尚未验收。编译经 ASCII 目录 `C:\TVProxy` + `tool\env.bat` 已可正常出包（最新 `dist\TVProxy-0.1.22-arm64-v8a-debug.apk`，真机已装验 0.1.19–0.1.22）。当前 TVProxy 运行中、上游=PC Clash 192.168.3.201:**10801**（Windows 排除段 7802–7901 导致 7897 绑定失败）、`drop_quic=true`。
