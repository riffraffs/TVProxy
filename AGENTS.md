# TVProxy 项目约定

## 文档职责（改文档时遵守）

- `docs/prd.md`：保持精简稳定，**不写实现状态**（无“已实现/未做”）。
- `docs/technical.md`：技术方案 + 实现进度的**唯一维护处** —— 技术事实与设计在前，进度（已完成/未做/下一步/遗留、版本记录）统一放文末「实现进度」一节。

## 环境备注

- 真机：华为智慧屏 THAL-560，adb `192.168.3.51:5555`。
- 打包产物：`dist/TVProxy-<version>-arm64-v8a-debug.apk`。
- 构建：走 ASCII 目录 `C:\TVProxy`（到本仓库的 junction）+ `tool\env.bat`；NDK 源在 `app/src/main/jni/hev-socks5-tunnel`。
- 上游代理场景：电脑 Clash Verge（mihomo）局域网共享；关键约束见 docs/technical.md 3.3/3.4（不对上游 socket 调 protect()；DNS 用公共解析器经代理出口解析，无需本地 DNS 中继）。
