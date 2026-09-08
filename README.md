# TVProxy

项目主要针对华为智慧屏v65进行测试。
但理论上，其他安卓电视也可以使用。
> **电视不装代理软件，只连一个“共享代理地址”，就能让电视 App 的流量走你的代理。**
---

## 1. 项目介绍

- **目标平台**：华为智慧屏 V 系列（鸿蒙 2.0，AOSP 兼容层，约 API 29）；同类 Android TV 亦可。遥控器操作，适配 720p / 1080p / 4K。
- **解决的问题**：电视「Wi-Fi 没有配置高级代理」入口；不想在电视上安装、维护 Clash；手边已有电脑 **Clash Verge（mihomo）** 或 iPhone **Loon** 局域网共享代理。
- **原理**：`VpnService` 建 tun（`10.0.0.2/24`，路由 `0.0.0.0/0`，仅 IPv4，MTU 1500）→ fake-ip 把 DNS 还原成域名 → `hev-socks5-tunnel`（lwIP）→ 上游 SOCKS5 带域名，或 HTTP CONNECT（请求目标为域名）。

**兼容性与边界**

| 项目 | 说明 |
|------|------|
| 上游协议 | SOCKS5（TCP + UDP）或 HTTP CONNECT（仅 TCP）。4K / QUIC 用 SOCKS5；HTTP 无 UDP，SmartTube 会落到 OkHttp |
| 界面 | 左侧状态卡「本机局域网地址」行右侧显示当前版本（如 `v1.0.0`） |
| 已测试上游 | **Clash Verge（mihomo）局域网共享、iOS Loon 局域网共享** |
| DNS | 仅 fake-ip：拦截 tun 上 UDP:53，本地答 `198.18.x`，SOCKS5 把**域名**交给 Clash/Loon 解析 |

**工作原理（简）**

```
电视 App ──► tun（全局 IPv4）
          ├─ UDP:53 ──► fake-ip（198.18.x ↔ 域名）
          └─ TCP/UDP ──► hev-socks5-tunnel
                          SOCKS5 ATYP=域名 ──► Clash / Loon
                          HTTP CONNECT 带域名 ──► Clash HTTP 口（非 DNS UDP 丢弃）
```

---

## 2. 授权工具使用方法

**为什么需要**：鸿蒙 2.0 老款 V 系列缺少系统 VPN 授权确认组件（`com.android.vpndialogs`），TVProxy 请求建立 VPN 时得不到授权弹窗，无法开启全局代理。电视无 root、也无法安装需平台签名的系统级弹窗，因此**通过 ADB 直接授予 TVProxy 的 `ACTIVATE_VPN` 权限**是当前唯一可行的预授权方式。

**使用步骤**：

1. 电视开启开发者模式与调试：「设置 → 系统 → 关于」在型号一栏使用遥控器 左左右右左左右右左左右右，打开hdc调试。
2. 把 `TVProxy-VPN授权工具/` 整个目录复制到任意 Windows 电脑，双击运行 `grant_vpn.bat`。
3. 同一局域网，按提示输入电视 IP（默认 ADB 端口 5555；端口不同请输 `IP:端口`）。
4. 连接成功、电视弹出授权确认时选择「允许」；脚本随后执行授权并回显 `ACTIVATE_VPN: allow`。
5. 回到电视打开 TVProxy，点「保存并启动」即可，之后不再需要系统授权弹窗。

**常见问题**

- 连接失败/超时：确认电脑与电视在同一局域网、电视调试已开启、「纯净模式」已关闭；部分电视需先用 USB 连接一次完成信任。
- 授权失效（重装 App 或系统重置后）：重新运行一次脚本即可（脚本幂等）。
- 提示「未找到应用」：先安装好 TVProxy 再执行授权。

目录内 `adb.exe` 及其运行库为便携副本，可将整个目录拷走使用，不依赖电脑预装 adb。

---

## 3. 项目使用方法

**快速开始（电视侧）**

1. **准备上游共享代理**
   - Clash Verge：开启「允许局域网连接」，记录电脑局域网 IP 与**混合端口**（如 `192.168.x.x:10801`）；要 4K/QUIC 选类型 SOCKS5；纯 HTTP 口才选 HTTP。
   - Loon：仪表页点右上角开启「网络共享」（● 变绿），使用其 **SOCKS5 端口**（如 `192.168.x.x:7221`），并保持 Loon 运行。
2. **安装 APK**：到 [Releases](https://github.com/riffraffs/TVProxy/releases) 按 ABI 下载当前版（如 `TVProxy-1.0.0-arm64-v8a-debug.apk` / `…-armeabi-v7a-debug.apk`），U 盘侧载或 `adb install`。
3. **（老款鸿蒙电视）先授予 VPN 权限**——见第 2 节；其它有系统授权弹窗的设备直接在弹窗里选「允许」。
4. 打开 TVProxy，填上游 IP 与端口 →「保存并启动」。状态胶囊变为「运行中」即成功。

**从源码构建（开发者）**

1. 环境要求：JDK 17、Android SDK（platform 34/35）、NDK **r27**。
2. 配置 `local.properties` 指向本机 SDK/NDK。
3. 构建（仓库已含标准 Gradle wrapper，首次运行会自动下载 Gradle 8.x）：

   ```bat
   gradlew.bat :app:assembleDebug                     :: 默认编 armeabi-v7a / arm64-v8a（真机）
   gradlew.bat :app:assembleDebug -Pabi=arm64-v8a     :: 仅编目标 ABI，更快
   ```

   产物：`app/build/outputs/apk/debug/app-debug.apk`（调试签名，侧载即可）。
   Linux / macOS 用 `./gradlew :app:assembleDebug`。

---

## 4. 文件结构

```
TVProxy/
├─ app/                              # Android 应用（Kotlin + JNI/NDK）
│  ├─ src/main/java/com/tvproxy/     # Kotlin 源码（UI / 配置 / VpnService / VPN 授权）
│  ├─ src/main/jni/hev-socks5-tunnel/ # native 引擎（lwIP + SOCKS5 + fake-ip）
│  ├─ src/main/res/                  # 布局 / 文案 / 颜色
│  └─ build.gradle.kts
├─ TVProxy-VPN授权工具/              # 电脑侧一键授权（grant_vpn.bat + 便携 adb）
├─ gradlew / gradle/                 # 标准 Gradle wrapper（仓库内可直接构建）
├─ build.gradle.kts                  # 根构建脚本
├─ settings.gradle.kts
├─ gradle.properties
└─ README.md
```

> 不纳入版本控制：`app/build/`（构建产物）、`dist/`（APK 分发）、`tool/`（本地 SDK/NDK/工具链）、`local.properties`。安装包在 [GitHub Releases](https://github.com/riffraffs/TVProxy/releases)。

---

## 5. 性能

**Clash Verge（mihomo）——推荐，YouTube 4K 可流畅播放**

**Loon**
经测试后猜想，ios系统在息屏后可能会进入wifi休眠等省电状态，导致连接loon的socks连接会断联，如果想使用，需要保持ios系统不息屏。
安卓手机没有这个问题，但是没有测试是否支持。
