# TVProxy

让华为智慧屏 / 电视走「局域网共享代理」的全局代理客户端：电视上的所有流量经内置 VPN（VpnService）统一接管后，
转发到电脑或手机开启的局域网共享代理出口。免 root、免在电视上装 Clash等软件进行分流导致性能不足。

> **电视不装代理软件，只连一个“共享代理地址”，就能让电视 App 的流量走你的代理。**
---

## 1. 项目介绍

- **目标平台**：华为智慧屏 V 系列（鸿蒙 2.0，AOSP 兼容层，约 API 29）；同类 Android TV 亦可。遥控器操作，适配 720p / 1080p / 4K。
- **解决的问题**：电视「Wi-Fi 没有配置高级代理」入口；不想在电视上安装、维护 Clash；手边已有电脑 **Clash Verge（mihomo）** 或 iPhone **Loon** 局域网共享代理。
- **原理**：`VpnService` 建 tun（`10.0.0.2/24`，路由 `0.0.0.0/0`，仅 IPv4，MTU 1500）→ `hev-socks5-tunnel`（内置 lwIP 协议栈）→ 上游 SOCKS5。

**兼容性与边界**

| 项目 | 说明 |
|------|------|
| 上游协议 | **仅 SOCKS5**。界面上的“HTTP”暂未实现 |
| 已测试上游 | **Clash Verge（mihomo）局域网共享、iOS Loon 局域网共享**，两类均完成真机联调 |
| Loon 限制 | Loon 共享 SOCKS5 **没有 UDP 中继**（UDP ASSOCIATE 返回空地址）。TVProxy 自动回落为「DNS 走 TCP」以保证网页/普通应用可用； |
| DNS | 上游支持 UDP：`8.8.8.8`/`1.1.1.1` 查询经隧道由代理出口解析（防污染）；无 UDP 中继：自动切 DNS-over-TCP（`223.5.5.5`/`119.29.29.29`/`8.8.8.8`/`1.1.1.1`），无需任何配置 |

**工作原理（简）**

```
电视 App 流量 ──► VpnService 虚拟网卡（tun，全局 IPv4 路由）
                ──► hev-socks5-tunnel（内置 lwIP 协议栈）
                ──► 上游 SOCKS5（Clash Verge 混合端口 / Loon 共享端口）
```

- 每次「保存并启动」会**自动探测上游是否支持 UDP 中继**并选择对应的 DNS 通道，Clash ↔ Loon 之间切换只需改地址/端口再保存一次。
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
   - Clash Verge：开启「允许局域网连接」，记录电脑局域网 IP 与**混合端口**（如 `192.168.x.x:7897`）；
   - Loon：仪表页点右上角开启「网络共享」（● 变绿），使用其 **SOCKS5 端口**（如 `192.168.x.x:7221`），并保持 Loon 运行。
2. **安装 APK**：U 盘侧载，或 `adb install`。
3. **（老款鸿蒙电视）先授予 VPN 权限**——见第 2 节；其它有系统授权弹窗的设备直接在弹窗里选「允许」。
4. 打开 TVProxy，填上游 IP 与端口 →「保存并启动」。状态胶囊变为「运行中」即成功。

**从源码构建（开发者）**

1. 环境要求：JDK 17、Android SDK（platform 34/35）、NDK **r27**；具体搭建见 `docs/dev-env.md`。
2. 配置 `local.properties` 指向本机 SDK/NDK。
3. 构建：

   ```bat
   gradlew.bat :app:assembleDebug                     :: 默认编 armeabi-v7a / arm64-v8a / x86_64
   gradlew.bat :app:assembleDebug -Pabi=arm64-v8a     :: 仅编电视用 ABI，更快
   ```

   产物：`app/build/outputs/apk/debug/app-debug.apk`（调试签名，侧载即可）。

4. 提示：`gradlew.bat` 在本仓库是本地工具链桥接（不入库）；NDK 对含非 ASCII 字符的路径敏感，建议把仓库放到纯 ASCII 路径下构建（开发机常以 `C:\TVProxy` junction 到仓库目录，配合 `tool\env.bat`）。

---

## 4. 文件结构

```
TVProxy/
├─ app/                              # Android 应用（Kotlin + JNI/NDK）
│  ├─ src/main/java/com/tvproxy/     # Kotlin 源码（UI / 配置 / VpnService / 上游探测 / VPN 授权）
│  ├─ src/main/jni/hev-socks5-tunnel/ # native 引擎（lwIP + SOCKS5，含 DNS-over-TCP、drop-quic）
│  ├─ src/main/res/                  # 布局 / 文案 / 颜色
│  └─ build.gradle.kts
├─ TVProxy-VPN授权工具/              # 电脑侧一键授权（grant_vpn.bat + 便携 adb）
├─ docs/                             # 文档
│  ├─ prd.md                         # 产品范围
│  ├─ technical.md                   # 技术方案 + 实现进度（进度在文末）
│  ├─ dev-env.md                     # 开发/调试环境
│  └─ ui-prototype/                  # UI 原型（HTML）
├─ AGENTS.md                         # 项目约定
├─ build.gradle.kts                  # 根构建脚本
├─ settings.gradle.kts
├─ gradle.properties
└─ README.md
```

> 不纳入版本控制：`app/build/`（构建产物）、`dist/`（APK 分发）、`tool/`（本地 SDK/NDK/工具链）、`local.properties`。

---

## 5. 性能

**Clash Verge（mihomo）——推荐，YouTube 4K 可流畅播放**

**Loon——不适合看视频，只适合轻量/网页**
- 考虑优化
