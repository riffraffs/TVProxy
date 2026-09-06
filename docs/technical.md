# TVProxy 技术方案设计：VpnService + tun2socks

> 关联文档：`docs/prd.md`（产品范围）、`docs/plan.md`（实现状态）
> 本文只记录技术事实与设计（架构、参数、数据流、踩坑原理）；「已完成/未做/下一步」统一见 `docs/plan.md`。
> 目标平台：华为智慧屏 V 系列（老款，鸿蒙 2.0，含 AOSP 兼容层，可侧载 APK）
> 编写日期：2026-09-04

---

## 一、方案概述与可行性结论

**结论：方案可行，可作为正式技术路线落地。**

- 采用 **Android VpnService + tun2socks 引擎** 实现全局流量代理，免 root。
- 目标平台华为老款 V 系列运行 **鸿蒙 2.0（HarmonyOS 2）**，官方明确兼容 Android 软件（AOSP 兼容层，约对应 Android 10 / API 29），`VpnService` 等核心 API 可用；且鸿蒙 2.0 后台保活能力优于原生 Android，契合「常驻后台」的性能要求。
- 唯一需要在真机上验证的点：**VpnService 在鸿蒙 2.0 上的实际可用性与 VPN 授权弹窗体验**（见第六节风险）。

---

## 二、技术架构

分层架构如下：

```
┌─────────────────────────────────────────────────────┐
│  UI 层（Leanback，遥控器可操作）                        │
│  局域网 IP 展示 / 代理类型下拉 / 地址·端口输入 / 保存      │
├─────────────────────────────────────────────────────┤
│  业务层（Kotlin）                                     │
│  配置模型 ProxyConfig / 配置持久化 SharedPreferences    │
├─────────────────────────────────────────────────────┤
│  VpnService（前台服务）                               │
│  建立 tun 虚拟网卡，读写原始 IP 包                      │
├─────────────────────────────────────────────────────┤
│  JNI 桥接层                                          │
│  将 tun fd 与上游代理配置传入 native 层                 │
├─────────────────────────────────────────────────────┤
│  tun2socks 引擎（native，hev-socks5-tunnel）          │
│  用户态 TCP/IP 栈解析 → 转 SOCKS5/CONNECT 流           │
├─────────────────────────────────────────────────────┤
│  上游代理（用户自建）                                   │
│  HTTP 代理 / SOCKS5 代理                              │
└─────────────────────────────────────────────────────┘
```

**核心数据流：**

```
电视 App 出站流量
   │ 系统路由（0.0.0.0/0 指向 tun）
   ▼
VpnService 虚拟网卡（tun，L3 IP 包）
   │ 读 fd 得到原始 IP 包
   ▼
tun2socks 引擎：用户态 TCP/IP 栈
   │ 解析 TCP/UDP 连接、目标地址/端口
   ▼
SOCKS5 CONNECT / UDP ASSOCIATE（无 HTTP CONNECT）
   ▼
上游代理（自建）──► 目标服务器
```

---

## 三、核心组件

### 3.1 VpnService

`VpnService` 是 Android 官方 API，通过 `Builder` 创建一张虚拟网卡并接管全部出站流量：

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `setSession` | "TVProxy" | 会话名 |
| `addAddress` | `10.0.0.2/24` | 虚拟网卡地址（私有网段，避开局域网冲突） |
| `addRoute` | `0.0.0.0/0` | 路由所有流量进 tun，实现全局代理（**仅 IPv4**，实测发现 IPv6 进 hev 会在本机触发原生崩溃） |
| `setMtu` | 1500 | 最大传输单元 |
| `addDnsServer` | `8.8.8.8`、`1.1.1.1`，DHCP DNS 兜底 | 公共解析器经 `0.0.0.0/0` 进 tun 到上游，由代理出口解析（见 3.4） |

`establish()` 返回 `ParcelFileDescriptor`，应用通过它的文件描述符读写原始 IP 包。

**要点**：`VpnService` 拿到的是 L3 层的 IP 包，**不含** TCP 连接语义，因此需要 tun2socks 引擎内置用户态 TCP/IP 栈来「终结」连接、再通过代理重建连接。

### 3.2 tun2socks 引擎选型

| 库 | 语言 | 上游支持 | 内存占用 | Android 集成方式 | 推荐度 |
|----|------|---------|---------|-----------------|--------|
| **hev-socks5-tunnel** | C | SOCKS5 / HTTP(CONNECT) | 极低 | JNI（官方提供 Android 支持） | ⭐ 首选 |
| go-tun2socks（xjasonlyu/tun2socks） | Go | SOCKS5 / HTTP | 低-中 | gomobile 打包 AAR | 备选 |
| badvpn tun2socks | C | SOCKS5 | 低 | 自行 NDK 编译，集成繁琐 | 不推荐 |

**首选 `hev-socks5-tunnel` 的理由：**
1. C 实现，**内存占用极低**，契合「内存占用极小」；
2. 官方 Android JNI，落地成本低；
3. SOCKS5 TCP/UDP 可用；HTTP CONNECT 非 hev 内置能力。

### 3.3 上游代理适配

- **SOCKS5**：tun2socks 输出 SOCKS5，连 Clash 混合端口或 gost `:10800`。
- **HTTP CONNECT**：UI 提供 HTTP 选项，native 仍发 SOCKS5；Clash 混合端口上 SOCKS5 一般也能通。
- **不支持认证**。配置只有 `{ 协议, 地址, 端口 }`。分流在 Clash，不在本应用。

本应用 `addDisallowedApplication(自身)`，连上游的套接字走 Wi-Fi，避免套圈。

> **约束：不对上游 socket 调 `VpnService.protect()`**（hev-socks5-session-tcp.c/-udp.c 的 bind 路径曾加过该 JNI 调用）：在鸿蒙电视上保存启动约 0.5 s 后触发原生 SIGSEGV。`addDisallowedApplication(自身)` 已保证应用自己连上游的流量绕过 tun，protect 冗余且是崩溃源，故已移除。

### 3.4 DNS

国内 DNS 不可用作 VPN DNS：电视 Wi-Fi 下发的运营商/路由器 DNS 会把国外域名解析成被污染的假 IP（实测 `www.google.com → 104.244.42.197 / 2001::1`），浏览器连错服务器导致「网页打开失败」。

当前方案：

1. VPN DNS 设为 `8.8.8.8`、`1.1.1.1`（`dnsServers()` 先加这两个，DHCP DNS 仅作最后兜底）。
2. 查询本身因默认路由 `0.0.0.0/0` 进 tun → hev 以 SOCKS5 UDP 转发到上游 → Clash 规则把公共 DNS 放行进代理（实测 `8.8.8.8:53 → 新加坡节点`），在**代理出口**解析，返回真实未污染 IP。
3. 应用再用真实 IP 建连，Clash 按 IP/规则转发。

> 备选：Clash 的 fake-ip DNS（`198.18.x`，按域名分流更优）需要它监听 53 端口，而 Android 只能查 53；可用 PC 端 53→7874 中继接入（该方案的取舍记录在 `docs/plan.md`）。当前方案为公共 DNS 走代理出口，**无需任何本地中继**。

---

## 四、华为老款 V 系列（鸿蒙 2.0）适配分析

| 维度 | 结论 | 说明 |
|------|------|------|
| Android 兼容 | ✅ 可用 | 鸿蒙 2.0 官方兼容 Android 软件，对应 API 29，`VpnService` / `ForegroundService` 可用 |
| APK 安装 | ✅ 可侧载 | 老款 V 系列可通过 U 盘安装 APK（用户已确认） |
| 后台保活 | ✅ 利好 | 鸿蒙 2.0 后台保活优于原生 Android，利于常驻，但仍需前台服务兜底 |
| 遥控器交互 | ✅ 支持 | Leanback UI + DPAD 焦点导航 |
| 分辨率适配 | ✅ 支持 | Leanback 布局自适应 720p / 1080p / 4K |
| VPN 授权弹窗 | ⚠️ 需真机验证 | 首次启动需系统弹窗授权，鸿蒙下弹窗样式/文案需实测 |
| 后台限制 | ⚠️ 需引导 | 建议引导用户在「应用启动管理」中允许本应用后台运行 |

---

## 五、关键实现要点

### 5.1 权限与清单声明

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />

<service
    android:name=".TvProxyVpnService"
    android:permission="android.permission.BIND_VPN_SERVICE"
    android:exported="false">
    <intent-filter>
        <action android:name="android.net.VpnService" />
    </intent-filter>
</service>
```

### 5.2 VpnService 骨架

```kotlin
class TvProxyVpnService : VpnService() {
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startForeground(NOTIF_ID, buildNotification())
        startProxy(intent.getParcelableExtra("config"))
        return START_STICKY
    }

    private fun startProxy(config: ProxyConfig) {
        val builder = Builder()
            .setSession("TVProxy")
            .addAddress("10.0.0.2", 24)
            .addRoute("0.0.0.0", 0)
            .setMtu(1500)
            .addDnsServer("8.8.8.8")          // 公共 DNS，查询进 tun 到上游走代理出口
            .addDnsServer("1.1.1.1")          // 兜底另加 DHCP DNS（见 dnsServers()）
        val vpnFd = builder.establish()          // 返回 ParcelFileDescriptor
        nativeStartTun2socks(vpnFd!!.detachFd(), config.protocol, config.host, config.port)
    }

    private external fun nativeStartTun2socks(
        tunFd: Int, protocol: Int, host: String, port: Int
    ): Int
}
```

### 5.3 JNI 与 native 层

- 在 native 层加载 `hev-socks5-tunnel` 的静态/动态库；
- 把 `establish()` 得到的 tun fd 与上游代理地址、端口传入 native 层；
- native 层启动 tun2socks 主循环，持续从 tun 读 IP 包、解析、经上游代理转发；
- 提供 `nativeStop()` 用于关闭隧道、释放 fd。

### 5.4 配置持久化

- 用 `SharedPreferences` 保存 `{ 协议, 地址, 端口 }`；
- App 启动时读取并回填 UI；「保存」时写入配置并（重新）启动 VpnService。

### 5.5 UI 交互（Leanback）

- 局域网 IP 用 `NetworkInterface` 枚举 WiFi 接口获取，展示为只读文本；
- 代理类型用 `Spinner`（HTTP / SOCKS5）；
- 地址、端口用 `EditText`，获焦后系统自动弹出 TV 键盘；
- 所有控件支持遥控器 DPAD 焦点导航。

---

## 六、风险与对策

| 风险 | 等级 | 对策 |
|------|------|------|
| VpnService 在鸿蒙 2.0 的可用性 / 授权弹窗体验 | 🔴 高 | 以「建立 VPN + 弹窗授权」最小闭环优先验证（结果见 `docs/plan.md`） |
| JNI / NDK 集成复杂度 | 🟡 中 | 优先用 hev-socks5-tunnel 官方 Android 集成；备选 go-tun2socks（gomobile AAR） |
| 后台被系统清理 | 🟡 中 | 前台服务 + 常驻通知 + 引导用户加入「应用启动管理」白名单 |
| 国内 DNS 污染导致国外打不开 | 🟡 中 | VPN DNS 用 8.8.8.8 / 1.1.1.1，查询经隧道到上游、由代理出口解析（见 3.4） |
| TV 芯片转发性能 | 🟢 低 | C 实现引擎开销极小 |
| 部分 App 检测/绕过 VPN | 🟢 低 | 属边界场景，MVP 不处理 |

---

## 七、技术要点小结

`VpnService + hev-socks5-tunnel`：tun `10.0.0.2/24`、路由 `0.0.0.0/0`（仅 IPv4）、MTU 1500；上游仅 SOCKS5（无 HTTP CONNECT）；不对上游 socket 调 protect()；DNS 为 8.8.8.8 + 1.1.1.1（DHCP 兜底），经代理出口解析，无本地 DNS 中继。阶段结果见 `docs/plan.md`。
