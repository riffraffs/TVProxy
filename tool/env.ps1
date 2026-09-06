# ===== TVProxy tool environment =====
# Usage (PowerShell):  . .\tool\env.ps1
# Then java / adb / ndk-build / sdkmanager / emulator / gradle / gost are on PATH.

$ToolRoot = $PSScriptRoot
# Emulator/SDK break on non-ASCII paths. Prefer the ASCII junction if present.
if (Test-Path "C:\TVProxy\tool\env.ps1") {
    $ToolRoot = "C:\TVProxy\tool"
}
$env:TOOL_ROOT = $ToolRoot
$env:JAVA_HOME = Join-Path $ToolRoot "jdk\17"
$env:ANDROID_HOME = Join-Path $ToolRoot "android-sdk"
$env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
$env:NDK_HOME = Join-Path $env:ANDROID_HOME "ndk\27.3.13750724"
$env:ANDROID_NDK_HOME = $env:NDK_HOME
$env:ANDROID_AVD_HOME = Join-Path $ToolRoot "avd"
$env:ANDROID_USER_HOME = Join-Path $ToolRoot "android-user"
$env:GRADLE_HOME = Join-Path $ToolRoot "gradle"
$env:STUDIO_HOME = Join-Path $ToolRoot "android-studio"

$prepend = @(
    (Join-Path $env:JAVA_HOME "bin"),
    (Join-Path $env:ANDROID_HOME "platform-tools"),
    (Join-Path $env:ANDROID_HOME "emulator"),
    (Join-Path $env:ANDROID_HOME "cmdline-tools\latest\bin"),
    $env:NDK_HOME,
    (Join-Path $env:GRADLE_HOME "bin"),
    (Join-Path $ToolRoot "gost"),
    (Join-Path $env:STUDIO_HOME "bin")
) -join ";"

$env:PATH = "$prepend;$env:PATH"
