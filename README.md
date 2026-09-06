# TVProxy

让智慧屏/电视走「局域网共享代理」的全局代理客户端：电视上的所有流量经内置 VPN（VpnService）统一接管后，
转发到你电脑或手机开启的局域网共享代理出口。在华为智慧屏 V 系列（鸿蒙 2.0）上开发并完成真机验证。

> 一句话定位：**电视不装代理软件，只连一个“共享代理地址”，就能让电视 App 的流量走你的代理。**

---

## 它解决什么问题 / 适合谁

- 华为（HarmonyOS）智慧屏**没有「Wi-Fi 手动代理」入口**，想给电视配代理只能走 App 级 VPN 方案；
- 不想在电视上安装 Clash 等代理客户端（配置、规则、更新都麻烦）；
- 手边已经有可用的局域网共享代理：电脑上的 **Clash Verge（mihomo）**，或 iPhone 上的 **Loon**。

同类 Android TV 设备亦可使用。

## 兼容性与边界（重要）

| 项目 | 说明 |
|------|------|
| 上游协议 | **仅 SOCKS5**。界面上的“HTTP”类型暂未实现——选择 HTTP 时仍按 SOCKS5 连接同一地址与端口，因此**纯 HTTP 代理端口不可用**（例如 Loon 的 HTTP 共享端口）；只有同端口同时提供 SOCKS5（如 Clash 混合端口）才可用 |
| 已测试上游 | **Clash Verge（mihomo）局域网共享、iOS Loon 局域网共享**，两类均完成真机联调 |
| Loon 限制 | Loon 的局域网共享 SOCKS5 **没有 UDP 中继**（实测其 UDP ASSOCIATE 应答为空地址）。TVProxy 会自动回落为「DNS 走 TCP」以保证网页/普通应用可用；但 **UDP 流量（QUIC、游戏等）仍无法经 Loon 转发**。且经手机转发受续航与稳定性影响，长时间/大流量建议用电脑端 Clash Verge |
| DNS | 上游支持 UDP 时：8.8.8.8/1.1.1.1 查询经隧道由代理出口解析（防污染）；上游不支持 UDP 时：自动切换为 DNS-over-TCP（优先 223.5.5.5 / 119.29.29.29，无需任何配置） |
| 不支持 | 上游认证、电视侧规则分流、IPv6 路由、域名/主机名形式的上游地址 |

## 工作原理（简）

```
电视 App 流量 ──► VpnService 虚拟网卡(tun, 全局 IPv4 路由)
                ──► hev-socks5-tunnel（内置 lwIP 协议栈）
                ──► 上游 SOCKS5（Clash Verge 混合端口 / Loon 共享端口）
```

- 每次「保存并启动」会**自动探测上游是否支持 UDP 中继**并选择对应的 DNS 通道，Clash ↔ Loon 之间切换只需改地址/端口再保存一次。
- 架构与踩坑细节见 `docs/technical.md`。

---

## 快速开始（电视侧）

1. **准备上游共享代理**
   - Clash Verge：开启「允许局域网连接」，记录电脑局域网 IP 与**混合端口**（如 `192.168.x.x:7897`）；
   - Loon：仪表页点右上角开启「网络共享」（● 变绿），使用其 **SOCKS5 端口**（如 `192.168.x.x:7221`），并保持 Loon 运行。
2. **安装 APK**：U 盘侧载，或 `adb install`。
3. **（老款鸿蒙电视）先授予 VPN 权限**——见下方「VPN 授权工具」；其它有系统授权弹窗的设备直接在弹窗里选“允许”。
4. 打开 TVProxy，填上游 IP 与端口 →「保存并启动」。状态胶囊变为「运行中」即成功。

## VPN 授权工具（TVProxy-VPN授权工具/）

**为什么需要**：鸿蒙 2.0 老款 V 系列缺少系统 VPN 授权确认组件（`com.android.vpndialogs`），
TVProxy 请求建立 VPN 时得不到授权弹窗，无法开启全局代理。电视无 root、也无法安装需要平台签名授权的系统级
弹窗，因此**通过 ADB 直接授予 TVProxy 的 `ACTIVATE_VPN` 权限**是当前唯一可行的预授权方式。

**使用步骤**：

1. 电视开启开发者模式与调试：
   「设置 → 系统 → 关于」连续点击「版本号」进入开发者选项，打开「USB 调试」或「网络调试（无线调试）」，记下电视局域网 IP。
2. 把 `TVProxy-VPN授权工具/` 整个目录复制到任意 Windows 电脑，双击运行 `grant_vpn.bat`。
3. 按提示输入电视 IP（默认 ADB 端口 5555；端口不同请输 `IP:端口`）。
4. 连接成功、电视弹出授权确认时选择「允许」；脚本随后执行授权并回显 `ACTIVATE_VPN: allow`。
5. 回到电视打开 TVProxy，点「保存并启动」即可，之后不再需要系统授权弹窗。

**等价手动命令**（电脑已装 adb 时）：

```bat
adb connect 电视IP:5555
adb -s 电视IP:5555 shell cmd appops set com.tvproxy ACTIVATE_VPN allow
adb -s 电视IP:5555 shell cmd appops get com.tvproxy ACTIVATE_VPN   :: 应显示 allow
```

**常见问题**

- 连接失败/超时：确认电脑与电视在同一局域网、电视调试已开启、「纯净模式」已关闭；部分电视需先用 USB 连接一次完成信任。
- 授权失效（重装 App 或系统重置后）：重新运行一次脚本即可（脚本幂等）。
- 提示「未找到应用」：先安装好 TVProxy 再执行授权。

目录内 `adb.exe` 及其运行库为便携副本，可将整个目录拷走使用，不依赖电脑预装 adb。

## 从源码构建

仓库只保留**源码与文档**（构建产物 `app/build/`、本地工具链 `tool/`、`local.properties` 等均不入库），
克隆后按以下步骤即可在本地出包：

1. 环境要求：JDK 17、Android SDK（platform 34/35）、NDK **r27**（版本以 `app/build.gradle.kts` 为准）；
   具体搭建步骤见 `docs/dev-env.md`。
2. 配置 `local.properties` 指向本机 SDK/NDK（参考仓库内已忽略的本地副本）。
3. 构建：

   ```bat
   gradlew.bat :app:assembleDebug                       :: 默认编 armeabi-v7a / arm64-v8a / x86_64
   gradlew.bat :app:assembleDebug -Pabi=arm64-v8a       :: 仅编电视用 ABI，更快
   ```

   产物：`app/build/outputs/apk/debug/app-debug.apk`（调试签名，侧载即可）。
4. 提示：`gradlew.bat` 在本仓库是本地工具链桥接（不入库）；Windows 下 NDK 对含非 ASCII 字符的路径敏感，
   建议把仓库放到纯 ASCII 路径下构建（开发机常以 `C:\TVProxy` junction 到仓库目录，配合 `tool\env.bat`）。

## 文档导航

| 文档 | 内容 |
|------|------|
| `docs/prd.md` | 产品范围 |
| `docs/technical.md` | 架构、数据流、DNS/鸿蒙适配等技术事实 |
| `docs/plan.md` | 实现状态与版本记录（唯一进度维护处） |
| `docs/dev-env.md` | 开发/调试环境搭建 |
| `docs/ui-prototype/` | UI 原型（HTML） |

最新版本：**0.1.18**。
