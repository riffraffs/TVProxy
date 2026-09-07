# TVProxy 技术方案与实现进度

> 关联文档：`docs/prd.md`（产品范围）、`docs/ui-prototype/index.html`（原型）
> 目标平台：华为智慧屏 V 系列（老款，鸿蒙 2.0，含 AOSP 兼容层，可侧载 APK）
> 编写日期：2026-09-04 · 最近更新：2026-09-07
> 本文由原 `docs/technical.md` 与 `docs/plan.md` 合并：前半为技术事实与设计（架构、参数、数据流、踩坑原理）；**实现进度（已完成/未做/下一步/遗留、版本记录）统一在文末「实现进度」一节**。

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
│  UI 层（TV 遥控器 DPAD 焦点）                          │
│  局域网 IP 展示 / 代理类型步进选择 / 地址·端口输入 / 保存   │
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

**TCP-only 上游（无 UDP 中继）自动回落：** 部分局域网共享 SOCKS5（实测 iOS Loon「网络共享」）对 UDP ASSOCIATE 应答 `05 00` 但 BND 为 `0.0.0.0:0`（无可用中继；对照 mihomo 返回真实中继地址）。服务启动时 `UpstreamProbe` 在后台线程做一次探测（主线程会触发 NetworkOnMainThreadException）：SOCKS5 问候成功后再发 UDP ASSOCIATE，BND 全零 / 命令被拒 / 应答缺失 ⇒ 判定无 UDP，进入 **DNS-over-TCP 模式**（配置写 `misc.dns-over-tcp: true`）；连接/问候阶段失败则保守按「支持 UDP」处理，既有上游（Clash 等）行为零回归。模式由 native 配置项 `dns-over-tcp` 控制（`hev-config.c`）。开启后 tun 读循环拦截目标 UDP:53 的查询包（新增 `hev-dns-tcp.c`，在 lwIP 之前消费，不进入 lwIP/UDP-ASSOCIATE 路径），每查询开一个 hev 任务：连上游 SOCKS5 → TCP CONNECT 到同一解析器 IP:53 → 按 RFC 7766 两字节长度帧收发 DNS → 把应答拼成 IPv4+UDP 包直接写回 tun fd（`hev_socks5_tunnel_write_packet`，与 netif 输出同锁）。上限：DNS 查询 ≤1400 B、应答 ≤1472 B、并发 8。该模式仅解决 DNS；QUIC 等其它 UDP 仍受上游无 UDP 中继限制。

**TCP-only 模式下非 DNS UDP 的处理（0.1.19）：** 该类 UDP（QUIC 443、NTP 123、mDNS 5353 等）在无 UDP 中继的上游上必然无法送达；若交给 lwIP，每个目的 UDP「流」会新建一个 UDP session 并对上游发起 SOCKS5 UDP ASSOCIATE——上游应答 BND 为空时 `hev_socks5_client_udp_set_upstream_addr` 把关联 UDP socket `connect()` 到 `0.0.0.0:0` 后必然失败，造成持续的空转会话/连接（真机实测 0.1.18 + Loon 待机日志 ~55 个/分）。因此该模式下 tun 读循环在 DNS 拦截之后、lwIP 之前，把所有其余 IPv4 UDP 数据报直接丢弃并按目的端口计数，每 10 s 汇总一条 INFO（`[udp] tcp-only drop total=… dns=… quic443=… ntp=… mdns=… other=… dns-qfull=…`）。对应用而言丢包与原先（进将死 UDP 会话后无应答）结果一致，但不产生会话/连接 churn。DNS-over-TCP 并发满（8）时查询进入有界队列（上限 32，FIFO，worker 完成一个取下一个），队列满才丢弃并记 WARN——不再落回 lwIP UDP 路径（该路径对 TCP-only 上游即静默丢失，原 0.1.18 会丢查询）。每个 DNS-over-TCP 查询完成后打一条 INFO：`[dns] tcp <解析器IP> q=<QNAME> ok|fail cost=<处理ms> wait=<排队ms>`；QNAME 从查询区第 1 个名字解析（DNS 头 12 字节之后、长度前缀标签，直至 0 字节）。TCP-only 判定经 `TvProxyVpnService.dnsOverTcpRunning`（静态 `AtomicBoolean`，探测结论得出即置位、teardown 时清位）发布给 UI：MainActivity 状态刷新按 `running && dnsOverTcpRunning` 决定显示/隐藏状态卡黄色提示（`@string/notice_tcp_only`，色 `warn`）。

**实测参考（0.1.19 真机 THAL-560，TCP-only 上游=PC 上以 Python 起的纯 CONNECT SOCKS5 中继、链到 Clash 7897 出口，复刻 Loon 的 TCP-only 约束且出口与 Clash 完全一致）：** 会话期间 UDP churn 为 0；DNS-over-TCP 对 223.5.5.5/119.29.29.29 快速失败（CONNECT 阶段即被拒/断开，≤340 ms）后由 netd 级联到 8.8.8.8 成功，单查询 cost 约 210–275 ms（含 LAN 握手 + 经出口的 DNS RTT）；QUIC/UDP443 尝试计数为 0（YouTube 应用在该网络不持续探测 QUIC）。同出口下 UDP 路径（Clash 直连）YouTube 播放流畅、TCP-only 路径明显变慢——差异归因于上游无 UDP 中继（QUIC 不可用、每域名一次 DNS-over-TCP），而非 tun2socks 栈。残余边界：无法为 TCP-only 上游提供 QUIC/其它 UDP；向应用回 ICMP Port Unreachable 促 QUIC 快速回落的方案因缺少持续 QUIC 探测场景而未采用。

**运行期吞吐打点（0.1.20）：** tun 读循环按 IP 协议（`ip[9]`）累加上下行 TCP/UDP 字节与包数，由 lwIP 定时任务（250ms tick，有会话时）每 10s 打一条 INFO：`[stats] up tcp=… udp=… down tcp=… udp=… quic_drop=… icmp=…`（`quic_drop/icmp` 两字段随 0.1.21 drop-quic 加入；`hev-socks5-tunnel.c`，仅日志不改数据路径）。用于归因速度问题：实测 LAN DIRECT 8 并发下行 168 Mbps、单连接 ~43 Mbps；YouTube（Fire TV 应用/TV Bro）流量几乎全走 UDP(QUIC)，下行 1–4.6MB/s 锯齿 —— 隧道本身不是瓶颈。

**drop-quic（0.1.21 加、0.1.22 修复后生效）：** 在**上游有 UDP 中继**（`dns-over-tcp=false`）但出口 UDP/QUIC 路不稳时把 QUIC 禁掉的开关（背景实测见上：电视端应用优先 QUIC 只能拿到 1–4.6 MB/s 锯齿、TCP 路稳）。配置键 `misc.drop-quic`（`hev-config.c` 解析，getter `hev_config_get_misc_drop_quic`）；Kotlin 侧 `ProxyPrefs.dropQuic()` 读 SharedPreferences `drop_quic`（默认 true）随配置写入。开启时 tun 读循环在 lwIP 之前丢弃目的端口 443 的 IPv4 UDP 数据报，并回 ICMP Destination Unreachable/Port Unreachable（RFC 792：新 IP 头 + ICMP 头 + 引用原 IP 头及 8 字节载荷，自算 IP/ICMP 校验和，直接写回 tun；仅对无 IP 选项（IHL=20）的原包回，带选项的丢弃不回；限速 1 s 窗口 8 个；计数 `stat_quic_dropped/stat_quic_icmp` 入 `[stats]` 行）——目的：客户端（Chromium/ExoPlayer）收到端口不可达即放弃 QUIC、回落 TCP。只丢 UDP:443，其余 UDP（NTP/mDNS 等）照常走 UDP 中继；与 TCP-only 模式互斥（后者静默丢全部非 DNS UDP，见上，该模式下此键不参与判定）。**踩坑（0.1.22）：** 端口判定复用 `hev_dns_tcp_udp_dst_port`，初版误读 UDP 头 `udp[0..1]`（源端口）当目的端口 ⇒ 443 永不匹配，0.1.21 实际未拦到任何包；改读 `udp[2..3]` 后 `quic_drop/icmp` 计数才出现、QUIC 流量真正全部落 TCP。分片续片无 UDP 头，该函数返回 0，不命中 443，放行进 lwIP。

### 3.4 DNS

国内 DNS 不可用作 VPN DNS：电视 Wi-Fi 下发的运营商/路由器 DNS 会把国外域名解析成被污染的假 IP（实测 `www.google.com → 104.244.42.197 / 2001::1`），浏览器连错服务器导致「网页打开失败」。

当前方案：

1. VPN DNS 设为 `8.8.8.8`、`1.1.1.1`（`dnsServers()` 先加这两个，DHCP DNS 仅作最后兜底）。
2. 查询本身因默认路由 `0.0.0.0/0` 进 tun → hev 以 SOCKS5 UDP 转发到上游 → Clash 规则把公共 DNS 放行进代理（实测 `8.8.8.8:53 → 新加坡节点`），在**代理出口**解析，返回真实未污染 IP。
3. 应用再用真实 IP 建连，Clash 按 IP/规则转发。

> 备选：Clash 的 fake-ip DNS（`198.18.x`，按域名分流更优）需要它监听 53 端口，而 Android 只能查 53；可用 PC 端 53→7874 中继接入（该方案的取舍见下方「开发计划」P4 段，已放弃）。当前方案为公共 DNS 走代理出口，**无需任何本地中继**。

**UDP 中继不可用时的替代（DNS-over-TCP 模式，见 3.3）：** 该模式 VPN DNS 列表改为
`[223.5.5.5(AliDNS), 119.29.29.29(DNSPod), 8.8.8.8, 1.1.1.1]`，**不再带 DHCP 路由器 DNS 兜底**。
国内干净公共解析器对国外域名返回真实（未污染）IP，且本身是国内 IP——代理端规则无论 DIRECT 还是把公共
DNS 放行进代理都由出口解析，两条路都不受污染。UDP 可用的上游（Clash/mihomo）仍用
`8.8.8.8/1.1.1.1 + DHCP 兜底` 的原列表（0.1.18 前行为不变）。

---

## 四、华为老款 V 系列（鸿蒙 2.0）适配分析

| 维度 | 结论 | 说明 |
|------|------|------|
| Android 兼容 | ✅ 可用 | 鸿蒙 2.0 官方兼容 Android 软件，对应 API 29，`VpnService` / `ForegroundService` 可用 |
| APK 安装 | ✅ 可侧载 | 老款 V 系列可通过 U 盘安装 APK（用户已确认） |
| 后台保活 | ✅ 利好 | 鸿蒙 2.0 后台保活优于原生 Android，利于常驻，但仍需前台服务兜底 |
| 遥控器交互 | ✅ 支持 | 纯 Activity + 自绘焦点控件（非 Leanback 组件库），DPAD 焦点导航 |
| 分辨率适配 | ✅ 支持 | Leanback 布局自适应 720p / 1080p / 4K |
| VPN 授权入口 | ⚠️ 缺失，需兜底 | 老款鸿蒙 2.0 无 `com.android.vpndialogs` 系统授权弹窗；先尝试 `com.huawei.vpndialogs` / `com.huawei.android.vpndialogs` 组件，仍无则 `VpnGrant.tryActivate()`（hidden `prepareVpn` + AppOps）兜底；都失败时 `establish()` 报错并提示用 ADB/HDC 执行 `tool\grant-vpn.bat` |
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

### 5.5 UI 交互（DPAD 焦点）

- 局域网 IP 用 `NetworkInterface` 枚举 WiFi 接口获取，展示为只读文本；
- 代理类型是步进控件（`‹ SOCKS5 ›`）：类型行获焦后遥控器**左右键**循环切换 SOCKS5 / HTTP（环绕），上下键按 类型 → 地址 → 端口 → 保存 → 停止 移动焦点；左右箭头视觉随行聚焦态通过 `duplicateParentState` 联动；
- 地址、端口用 `EditText`，获焦后系统自动弹出 TV 键盘；
- 所有控件支持遥控器 DPAD 焦点导航。

### 5.6 弹窗与鸿蒙默认窗框

- 自绘弹窗（首次授权说明、保活提示）用 `Dialog` + `card_tv` 深色圆角卡片。
- 踩坑：鸿蒙 2.0 TV 上 `Dialog` 的默认窗口背景会在自定义卡片**外侧**再画一圈浅灰辉光/描边（真机 4K 逐像素实测：四边约数 px 外出现峰值 ~#505050 的光带，卡边与光带间还有一圈更暗的缝）；Google ATV 模拟器无此现象，属系统默认 Dialog 外观而非卡片自身描边。
- 处理：弹窗改用 `Theme.TVProxy.Dialog`（`windowBackground=transparent`、`windowFrame=@null`、`windowContentOverlay=@null`），并显式 `window?.setBackgroundDrawable(透明)`。去掉系统窗框后，卡片外直接是遮罩，仅剩卡片自身 1dp 细描边。

---

## 六、风险与对策

| 风险 | 等级 | 对策 |
|------|------|------|
| VpnService 在鸿蒙 2.0 的可用性 / 授权弹窗体验 | 🔴 高 | 以「建立 VPN + 弹窗授权」最小闭环优先验证（结果见文末「实现进度」） |
| JNI / NDK 集成复杂度 | 🟡 中 | 优先用 hev-socks5-tunnel 官方 Android 集成；备选 go-tun2socks（gomobile AAR） |
| 后台被系统清理 | 🟡 中 | 前台服务 + 常驻通知 + 引导用户加入「应用启动管理」白名单 |
| 国内 DNS 污染导致国外打不开 | 🟡 中 | VPN DNS 用 8.8.8.8 / 1.1.1.1，查询经隧道到上游、由代理出口解析（见 3.4） |
| TV 芯片转发性能 | 🟢 低 | C 实现引擎开销极小 |
| 部分 App 检测/绕过 VPN | 🟢 低 | 属边界场景，MVP 不处理 |

---

## 七、技术要点小结

`VpnService + hev-socks5-tunnel`：tun `10.0.0.2/24`、路由 `0.0.0.0/0`（仅 IPv4）、MTU 1500；上游仅 SOCKS5（无 HTTP CONNECT）；不对上游 socket 调 protect()；DNS 为 8.8.8.8 + 1.1.1.1（DHCP 兜底），经代理出口解析，无本地 DNS 中继。阶段结果见文末「实现进度」。

---

## 八、开发计划

> 本节与下节原属 `docs/plan.md`，合并后进度统一维护在第九节。

### 8.1 原则

1. 先验证 VpnService 授权弹窗，再接 native（第六节风险，最高优先级）。
2. 先跑通 SOCKS5，再做 HTTP CONNECT。
3. UI 以原型为准：鸿蒙智慧屏深色风格、遥控器 DPAD 焦点、不支持认证。
4. native 用 ndk-build + `hev-socks5-tunnel`；ABI：`armeabi-v7a`、`arm64-v8a`（真机）、`x86_64`（模拟器）。

包名 `com.tvproxy`；VpnService 类名 `TvProxyVpnService`；`minSdk 21` / `compileSdk 35` / `targetSdk 29`（对齐鸿蒙 2.0 AOSP 层）。

### 8.2 原型范围（要做）

| 区域 | 内容 |
|------|------|
| 标题 | TVProxy、副标题「智慧屏全局网络代理」 |
| 代理状态卡 | 状态胶囊（未开启 / 运行中）、本机局域网 IP（只读、大号） |
| 代理设置卡 | 顶边与状态卡对齐；代理类型、服务器地址、端口 |
| 操作 | 「保存并启动」（运行中改为「保存并重启」）、「停止代理」（未开启时不可用） |
| 弹层 | 首次保存弹出 VPN 授权说明；地址/端口获焦后系统 TV 键盘 |
| 焦点 | 类型 → 地址 → 端口 → 保存 → 停止；蓝框高亮 |

### 8.3 原型范围（不做进 App）

- HTML 电视外框、模拟状态栏时钟 / Wi-Fi 图标
- 底部「HarmonyOS 2.0 · HTML 原型」角标
- 自绘数字键盘（真机用系统 IME）
- 已从原型删除的说明文案与流量路径图

### 8.4 阶段（P0–P6）

**P0 工程脚手架**：用 `tool/env.bat` 建 Kotlin + Leanback 模块 `app`；`local.properties` 指向 `tool/android-sdk` 与 NDK `27.3.13750724`；Manifest 声明 `LEANBACK_LAUNCHER`/`INTERNET`/`FOREGROUND_SERVICE` 与 `TvProxyVpnService`+`BIND_VPN_SERVICE`；`ndk.abiFilters` 三种 ABI；空 `MainActivity` 能在 AVD 打开。完成：`gradlew assembleDebug` 成功、模拟器装上 APK。

**P1 VpnService 冒烟**：不接 tun2socks，只验证授权与 tun。`ProxyConfig` 含 `protocol`/`host`/`port`；`TvProxyVpnService` 前台通知 + `Builder`（session=`TVProxy`、`10.0.0.2/24`、`0.0.0.0/0`、MTU 1500）；Activity `prepare()`→授权→`startService`，提供停止。完成：授权可同意、系统 VPN 列表出现本应用、回桌面通知仍在。

**P2 SOCKS5 闭环**：submodule `hev-socks5-tunnel`；`hev-jni.c` 改 `PKGNAME "com/tvproxy"`、`CLSNAME "TvProxyVpnService"`；`ndk-build` 出 `.so`；`establish()` 后 `detachFd()` 交 native；本机 `tool\start-gost.bat`（`socks5://:10800`，避开 Hyper-V 占用的 1080），模拟器填 `10.0.2.2:10800`、真机填电脑局域网 IP。完成：电视浏览器走 gost、停 VPN 恢复直连。

**P3 UI 与配置（对齐原型）**：左标题+状态卡（胶囊、局域网 IP）；右设置卡（类型、地址、端口）顶边对齐；保存写 `SharedPreferences` 并（重）启服务、启动回填；获焦地址/端口弹系统 TV 键盘；深色底 + 宇宙蓝 `#0A59F7` + 大圆角卡片 + 焦点蓝框，适配 720p/1080p/4K；保存按钮文案随状态切换。完成：遥控器可配可存可开关、杀进程重开配置仍在。

**P4 HTTP 与 DNS**：对齐手机 Wi-Fi 手动代理，电视填 Clash 电脑 IP + 混合端口，分流只在 Clash。一期（已完成，2026-09-05 真机修正）：SOCKS5 全局进上游，DNS 初始沿用 DHCP，真机发现污染后改 VPN DNS=`8.8.8.8`+`1.1.1.1` 走代理出口（DHCP 仅兜底）；HTTP 类型仍按 SOCKS5 连同一地址端口。二期（未做）：native HTTP CONNECT。已放弃：mapdns / 本地 DNS 中继（0.1.12 曾用 PC 53→7874 中继实现 Clash fake-ip，公共 DNS 走代理出口已满足需求）。

**P5 保活与鸿蒙适配**：前台通知「TVProxy 运行中」；`START_STICKY` 被杀后按已存配置拉起；首次成功后短文案引导去「应用启动管理」允许后台运行；不做认证、不做分应用绕过。完成：回桌面、开其他 App 代理仍在（需真机确认通知不被清）。

**P6 真机验收**：华为 V 系列授权弹窗 / U 盘侧载 / 纯净模式拦截；遥控器焦点、键盘、返回键不误停 VPN；720p / 1080p / 4K 各看一眼；Release 签名 APK、真机 ABI 至少 `armeabi-v7a`+`arm64-v8a`；对照 PRD 走一遍 IP/类型/地址/端口/保存/全局代理。

### 8.5 建议目录

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

## 九、实现进度

> 图例：✅ 已完成 · 🟡 部分完成 · ⬜ 未做 · ⚠️ 遗留/待办
> 最近更新：2026-09-07。当前运行状态：TVProxy 运行中，上游=PC Clash `192.168.3.201:10801`，`drop_quic=true`。

### 9.1 阶段总览

| 阶段 | 状态 | 结果 |
|------|------|------|
| 开发环境 `tool/` | ✅ | 本地工具链可用 |
| UI 原型 | ✅ | `docs/ui-prototype/` |
| P0 工程脚手架 | ✅ | `assembleDebug` 出包、模拟器安装 |
| P1 VpnService 冒烟 | ✅ | 授权、VPN 列表、前台通知 |
| P2 SOCKS5 闭环 | ✅ | hev + gost `:10800` |
| P3 UI 与配置 | ✅ | DPAD 可配可存可开关、重启回填 |
| P4 一期 SOCKS5 + 系统 DNS | ✅ | DNS 改 8.8.8.8/1.1.1.1 走代理出口 |
| P4 二期 HTTP CONNECT | ⬜ | native 层未实现 |
| P5 保活 / 鸿蒙 | ✅ | START_STICKY + 一次性后台引导 |
| P6 真机验收 | 🟡 | THAL-560 冒烟通过；保活时长、多分辨率、Release 签名待验收 |

### 9.2 版本修复记录

| 版本 | 事项 | 状态 |
|------|------|------|
| 0.1.11 | 真机闪退：不对上游 socket 调 `protect()` | ✅ |
| 0.1.13 | DNS 污染：VPN DNS 改 8.8.8.8/1.1.1.1 走代理出口 | ✅ |
| 0.1.14 | 代理类型下拉改左右箭头步进 | ✅ |
| 0.1.15 | 首启弹窗精简文案、去鸿蒙默认窗框 | ✅ |
| 0.1.16 | 地址改 4 段 IPv4 输入 | ✅ |
| 0.1.17 | 报错区移入左状态卡 | ✅ |
| 0.1.18 | TCP-only 上游 DNS-over-TCP 自动回落 | ✅ |
| 0.1.19 | 非 DNS UDP 丢弃计数、DNS 有界排队、黄色提示 | ✅ |
| 0.1.20 | per-protocol `[stats]` 吞吐打点 | ✅ |
| 0.1.21 | `drop-quic` 拦 UDP:443 回 ICMP | ✅ |
| 0.1.22 | 修复端口字节偏移，drop-quic 生效 | ✅ |

### 9.3 版本详情

**0.1.14 代理类型 UI 对齐原型**：下拉框改为原型左右箭头步进；类型行获焦时遥控器左右键循环 SOCKS5/HTTP，上下键照常移焦点；真机 THAL-560 验证切换/环绕/保存持久化（logcat `establishing tun for http …`）。

**0.1.15 首启弹窗文案与白框**：说明正文 4+ 行精简到 2 行；Dialog 改用透明窗口背景主题（`Theme.TVProxy.Dialog`）去掉鸿蒙 TV 默认窗框——真机逐像素确认卡片外侧浅灰辉光环消失，模拟器+真机均验证。

**0.1.16 服务器地址改 4 段 IPv4 输入**：地址行由整段文本框拆为 4 个数字段（各 1–3 位，中间自动带点），点击/OK 只重输当前段，段间左右键一次一跳、键盘 Next 顺段跳转；保存时逐段去前导零并校验 0–255（段 999 保存时红字提示、不误启）。取舍：按需求确认只填局域网 IPv4，放弃域名/主机名输入；端口维持整段输入。原型 index.html 已同步为四段式。验证：模拟器 + 真机 THAL-560 均跑通「192.168.3.101 只把末段改成 201」（段4 全选替换，其余三段不变），真机 OK 弹出系统数字键盘、键入选段生效、重启回填正常。

**0.1.17 报错红字位置（按钮被挤出设置卡）**：红字报错原内联在右侧设置卡端口行与按钮之间——长报错（如 VPN 未授权指引）会把卡内弹性空间压到 0，把「保存并启动/停止代理」按钮推出卡底。现把报错区移到左侧状态卡 `lan_ip` 下方：错误时左卡向下生长、完整多行红字可读，消失时缩回，右侧设置卡与按钮布局零改动。验证：真机 THAL-560 `appops ACTIVATE_VPN` deny 后点保存并启动，长报错落在左卡 `[256,1116][1489,1720]`、两按钮 bounds 与空闲态完全一致；allow 后保存启动→运行中→停止→重开均正常、错误自动清除。

**0.1.18 Loon 适配（TCP-only 上游 DNS-over-TCP 回落）**：实测 iOS Loon 局域网共享的 SOCKS5 无 UDP 中继（UDP ASSOCIATE 应答 BND=0.0.0.0:0），而其 TCP CONNECT 正常 → 原「DNS 经 SOCKS5-UDP 走代理出口」方案失效，国外域名解析挂。启动时 `UpstreamProbe`（后台线程）探测上游 UDP 能力；无 UDP 则进入 DNS-over-TCP 模式：native 在 tun 读循环拦截 dst UDP:53 查询，经上游 TCP CONNECT 到解析器做 DNS-over-TCP（RFC 7766 帧），应答拼回 UDP 写 tun（新增 `hev-dns-tcp.c`，配置键 `misc.dns-over-tcp`），DNS 列表切为 223.5.5.5/119.29.29.29/8.8.8.8/1.1.1.1（不带 DHCP 兜底）。验证（真机 THAL-560）：Loon 场景探测 `dns-over-tcp=true`，`ping www.google.com/baidu/qq` 均解析出真实 IP（此前 unknown host）；TV Bro 打开 Google 首页，hev 日志 `[dns] tcp ok` 且本段 0 错误；Clash 回归：上游=PC mihomo 7897 探测 BND 真实 → 仍走 UDP 模式、DNS 列表不变、解析正常。边界（不做）：QUIC 等非 DNS UDP 经 TCP-only 上游仍不可用；IPv6 DNS/分片包/超 1472B 应答不支持；上游需认证不支持。

**0.1.19 Loon 网速慢（对比 Clash）分析与修复**：① 基线：0.1.18 + Loon 上游待机 4 分钟日志 ~327 行/分、`client udp construct` ~55/分、`io timeout` ~70/分——每个非 DNS UDP 包（QUIC/NTP/系统探测等）都建一个注定失败的 SOCKS5 UDP-ASSOCIATE 会话。② 修复：`dns-over-tcp` 模式下非 DNS UDP 在 tun 读循环直接丢弃并按端口计数（每 10s 汇总 `[udp] tcp-only drop`，杜绝将死会话，UDP 模式不受影响）；DNS 并发满（8）时改为有界排队（32，满才丢并记 WARN）；每个 DNS-over-TCP 查询打 INFO（解析器 IP、qname、cost/wait ms）。③ UI：运行中且 TCP-only 上游时状态卡显示黄色提示。验证（真机 THAL-560 + PC 复刻）：Clash（UDP）回归——探测 BND 真实、`dns-over-tcp=false`、YouTube 播放与 0.1.18 一样快、hev.log 0 条 tcp-only drop；TCP-only 复刻（PC 起 TCP-only SOCKS5 中继链到 Clash 出口）——0 UDP 会话 churn、DNS-over-TCP 正常（223.5.5.5/119.29.29.29 快速失败→8.8.8.8 成功约 210-275ms/次）、YouTube 能播但明显比 UDP 路径慢 ⇒ 主因确认为上游无 UDP，非 Loon 设备或 TVProxy 栈本身。遗留：真机 Loon 直连 A/B 未跑成；qname 打点偏移已修源码；`UpstreamProbe` connect 超时 1.5s 对慢接受的上游可能误判。

**0.1.20/0.1.21/0.1.22 Clash 路径 YouTube 2160p 打不开诊断与修复**（2026-09-06 晚 ~09-07）：① 0.1.20 加 `[stats]` per-protocol 吞吐打点；② 0.1.21 加 `drop-quic`（UDP 模式下丢 UDP:443 + ICMP Port Unreachable，Prefs 键 `drop_quic`）；③ 0.1.22 修复 `hev_dns_tcp_udp_dst_port` 端口字节偏移 bug（之前取成源端口，drop-quic 一直没拦到）。测量结论：隧道/Wi-Fi 非瓶颈（LAN 下行 168Mbps/多、43Mbps/单）；WAN 经节点单 TCP 32–52Mbps、8 连接 100–136Mbps（都够 2160p）；问题根因是出口节点 UDP(QUIC) 路不稳（电视端应用优先 QUIC 只能拿到 1–4.6MB/s 锯齿），TCP 路稳，PC Chrome 走 TCP 所以 2160p 流畅。修复后（drop-quic 生效）：`quic_drop/icmp` 计数出现、YouTube/TV Bro 全走 TCP；TV Bro 1440p 无加载卡顿；2160p 仍偶卡，剩余疑为 TV Bro/电视解码渲染能力（TCP 突发 23–45Mbps）。环境：Clash 混合端口因 Windows 排除段 7802–7901 无法再绑 7897，已改为 10801。收尾验证（2026-09-07）：安装 SmartTube 32.38 后 4K/2160p 播放「完美、不卡顿」——链路与 TVProxy 达标，之前 2160p 卡顿归因于 YouTube/TV Bro 客户端（死咬 QUIC 或浏览器解码性能），已解决。

### 9.4 下一步 / 不做 / 遗留

- **下一步**：HTTP CONNECT。
- **不做**：认证、电视侧分流、CMake、手机版 UI、主机名/域名输入。
- **遗留（⚠️）**：
  - P6：保活时长、720p/1080p/4K 分辨率、Release 签名 APK 尚未验收。
  - `drop-quic` 暂无 UI 开关——改值暂需经 SharedPreferences `drop_quic` 键或代码默认值。
  - `UpstreamProbe` connect 超时 1.5s 对慢接受的上游可能误判（0.1.19 遗留）。
  - 真机 Loon 直连 A/B 未跑成（TV↔iPhone 不同频段/休眠，0.1.19 遗留）。
- **构建/环境**：编译经 ASCII 目录 `C:\TVProxy` + `tool\env.bat` 出包；最新 `dist\TVProxy-0.1.22-arm64-v8a-debug.apk`，真机已装验 0.1.19–0.1.22。Clash 端口现用 `10801`（Windows 排除段 7802–7901 导致 7897 绑定失败）。
