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

下一步：HTTP CONNECT。不做：认证、电视侧分流、CMake、手机版 UI。
P6 遗留：保活时长、720p/1080p/4K 分辨率与 Release 签名 APK 尚未验收。编译经 ASCII 目录 `C:\TVProxy` + `tool\env.bat` 已可正常出包（`dist\TVProxy-0.1.13-...apk`）。
