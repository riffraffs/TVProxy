# ===== TVProxy tool environment loader (Git Bash / sh) =====
# Usage:  source tool/env.sh
# Then java, adb, ndk-build, sdkmanager, emulator, gradle, gost are on PATH.

export TOOL_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
# Emulator/SDK break on non-ASCII paths. Prefer the ASCII junction if present.
if [ -f "/c/TVProxy/tool/env.sh" ]; then
  export TOOL_ROOT="/c/TVProxy/tool"
elif [ -f "C:/TVProxy/tool/env.sh" ]; then
  export TOOL_ROOT="C:/TVProxy/tool"
fi
export JAVA_HOME="$TOOL_ROOT/jdk/17"
export ANDROID_HOME="$TOOL_ROOT/android-sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export NDK_HOME="$ANDROID_HOME/ndk/27.3.13750724"
export ANDROID_NDK_HOME="$NDK_HOME"
export ANDROID_AVD_HOME="$TOOL_ROOT/avd"
export ANDROID_USER_HOME="$TOOL_ROOT/android-user"
export GRADLE_HOME="$TOOL_ROOT/gradle"
export STUDIO_HOME="$TOOL_ROOT/android-studio"

export PATH="$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$ANDROID_HOME/emulator:$ANDROID_HOME/cmdline-tools/latest/bin:$NDK_HOME:$GRADLE_HOME/bin:$TOOL_ROOT/gost:$STUDIO_HOME/bin:$PATH"

# Windows-style paths (needed by .bat/.cmd launchers and Gradle)
if command -v cygpath >/dev/null 2>&1; then
  export JAVA_HOME_WIN="$(cygpath -w "$JAVA_HOME")"
  export ANDROID_HOME_WIN="$(cygpath -w "$ANDROID_HOME")"
else
  export JAVA_HOME_WIN="$JAVA_HOME"
  export ANDROID_HOME_WIN="$ANDROID_HOME"
fi

# ndk-build is a Windows .cmd; expose a bash-friendly wrapper (needs win JAVA_HOME)
ndk-build() { JAVA_HOME="$JAVA_HOME_WIN" "$NDK_HOME/ndk-build.cmd" "$@"; }
export -f ndk-build 2>/dev/null || true

# sdkmanager / avdmanager / emulator are .bat/.exe; expose bash-friendly wrappers
sdkmanager() { JAVA_HOME="$JAVA_HOME_WIN" "$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager.bat" "$@"; }
export -f sdkmanager 2>/dev/null || true

avdmanager() { JAVA_HOME="$JAVA_HOME_WIN" "$ANDROID_HOME/cmdline-tools/latest/bin/avdmanager.bat" "$@"; }
export -f avdmanager 2>/dev/null || true
