# Git on Windows often checks out submodule symlinks as text files.
# Copy the real headers over those placeholders so ndk-build can compile.
$root = Join-Path $PSScriptRoot "..\app\src\main\jni\hev-socks5-tunnel"
function Materialize-GitLinks([string]$repo) {
    if (-not (Test-Path $repo)) { return }
    $lines = git -C $repo ls-files -s | Where-Object { $_ -match '^120000' }
    foreach ($line in $lines) {
        $relPath = ($line -split '\s+', 4)[3]
        $linkFile = Join-Path $repo $relPath
        $rel = (Get-Content -LiteralPath $linkFile -Raw).Trim()
        $target = [IO.Path]::GetFullPath((Join-Path (Split-Path $linkFile -Parent) $rel))
        if (Test-Path -LiteralPath $target) {
            Copy-Item -LiteralPath $target -Destination $linkFile -Force
        }
    }
}
Materialize-GitLinks (Join-Path $root "src\core")
Materialize-GitLinks (Join-Path $root "third-part\hev-task-system")
Materialize-GitLinks (Join-Path $root "third-part\yaml")
Materialize-GitLinks $root
