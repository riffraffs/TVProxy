# TVProxy 开发环境搭建

> 依据：`docs/technical.md`、`docs/prd.md`
> 目标平台：华为智慧屏 V 系列（老款，鸿蒙 2.0）
> 本文档目的：从零搭建 TVProxy 的开发与调试环境

---

## 1. 硬件准备

| 项目 | 要求 |
|------|------|
| 开发机 | Windows / macOS / Linux，内存建议 16 GB 及以上 |
| 真机 | 华为智慧屏 V 系列（老款，鸿蒙 2.0） |

---

## 2. 安装基础工具

| 工具 | 版本 | 安装方式 |
|------|------|---------|
| JDK | 17 | Android Studio 自带 JBR（基于 JDK 17），通常无需单独安装；命令行构建时确保 `JAVA_HOME` 指向 17 |
| Android Studio | 最新稳定版 | 官网下载安装，首次启动按引导完成 SDK 初始化 |
| Git | 2.x | 官网或包管理器安装，需支持 submodule（`--recursive`） |

验证：

```bash
java -version    # 需为 17
git --version
```

---

## 3. 安装 Android SDK 组件

Android Studio → Settings → SDK Manager 中安装：

| 组件 | 版本 | 用途 |
|------|------|------|
| Android SDK Platform | API 34+（compileSdk）；minSdk 21 | 编译目标 |
| Build Tools | 最新稳定版 | 打包 |
| Platform Tools | 最新 | 提供 `adb` |
| **NDK** | **r27** | native 库（hev-socks5-tunnel）编译，其 CI 用 r27d |

> native 层采用 **ndk-build（Android.mk / Application.mk）**，非 CMake，无需额外安装 CMake。

---

## 4. 配置环境变量

```bash
# Android SDK 根目录
export ANDROID_HOME=$HOME/Android/Sdk
export ANDROID_SDK_ROOT=$ANDROID_HOME

# adb 与 ndk-build 命令行工具
export PATH=$PATH:$ANDROID_HOME/platform-tools:$ANDROID_HOME/ndk/27.x/bin
```

验证：`adb --version`、`ndk-build --version` 均能输出版本。

---

## 5. 搭建调试环境 A：模拟器

1. Device Manager → Create Device → 优先选 **TV 分类**下的 Android TV 镜像（无 TV 镜像用普通 Phone 镜像亦可，`VpnService` 均可运行）；
2. 遥控器操作由模拟器侧边栏的 DPAD 方向键、确认键、返回键模拟。

本机测试代理（gost）：下载对应平台二进制放入 PATH 后运行

```bash
gost -L socks5://:1080   # SOCKS5 代理，监听 1080
gost -L http://:8080     # HTTP 代理，监听 8080
```

App 内代理地址填 `10.0.2.2`（模拟器指向宿主机 `localhost`）。

---

## 6. 搭建调试环境 B：真机（华为智慧屏 V 系列）

1. 开启开发者模式：设置 → 系统 → 关于 → 连续点击「版本号」；
2. 开启「USB 调试 / 网络调试」，记下电视局域网 IP；
3. 连接 ADB：

```bash
adb connect <电视IP>:5555
adb devices          # 确认设备在线
```

4. 安装 APK（二选一）：
   - `adb install app-debug.apk`
   - U 盘侧载（媒体中心 → 文件 → 选择 APK 安装）

5. 若提示拦截，关闭「纯净模式」并开启「允许安装未知来源应用」。

---

## 7. 拉取代码并集成 native 库

```bash
# 拉取工程（含 submodule）
git clone --recursive <repo-url>

# 编译 native 库（或在 Gradle 中配置 externalNativeBuild 自动执行）
cd app/src/main/jni/hev-socks5-tunnel && ndk-build

# 构建 APK
./gradlew assembleDebug
```

**必须修改** `src/hev-jni.c` 两个宏，匹配本工程 Service 的包名与类名：

```c
#define PKGNAME "com/yourapp/service"   // 你的包名/路径
#define CLSNAME "TvProxyVpnService"     // 你的 VpnService 类名
```

编译产物：`libs/{abi}/libhev-socks5-tunnel.so`，ABI 含 `armeabi-v7a`、`arm64-v8a`、`x86`、`x86_64`。

---

## 8. 环境就绪检查清单

- [ ] `java -version` 为 17
- [ ] `adb --version` 可用
- [ ] `ndk-build --version` 可用
- [ ] 模拟器可启动并运行 App
- [ ] 真机 `adb devices` 在线
- [ ] `./gradlew assembleDebug` 构建成功
- [ ] 冒烟验证：建立 VpnService → 弹出 VPN 授权框 → 授权后 tun 网卡建立成功
