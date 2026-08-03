# 测试打包完整性的脚本
param(
    [string]$PackageDir = "dist"
)

function Write-Info { param($msg) Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Success { param($msg) Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Warning { param($msg) Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Error { param($msg) Write-Host "[ERROR] $msg" -ForegroundColor Red }

Write-Info "Testing package integrity..."

$binDir = Join-Path $PackageDir "bin"
$exePath = Join-Path $binDir "ctdy123.exe"

if (-not (Test-Path $exePath)) {
    Write-Error "Executable not found: $exePath"
    exit 1
}

Write-Info "Found executable: $exePath"

# 检查必要的DLL文件
$requiredDlls = @(
    "Qt6Core.dll",
    "Qt6Gui.dll", 
    "Qt6Widgets.dll",
    "Qt6Network.dll",
    "Qt6PrintSupport.dll",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll"
)

Write-Info "Checking required DLLs..."
$missingDlls = @()
foreach ($dll in $requiredDlls) {
    $dllPath = Join-Path $binDir $dll
    if (Test-Path $dllPath) {
        Write-Success "Found: $dll"
    } else {
        Write-Warning "Missing: $dll"
        $missingDlls += $dll
    }
}

# 检查Qt插件目录
$pluginDirs = @("platforms", "imageformats", "iconengines", "styles")
foreach ($dir in $pluginDirs) {
    $pluginPath = Join-Path $binDir $dir
    if (Test-Path $pluginPath) {
        $files = Get-ChildItem $pluginPath -File
        Write-Success "Found $($files.Count) files in $dir/"
    } else {
        Write-Warning "Missing plugin directory: $dir"
    }
}

# 使用Dependency Walker检查依赖
Write-Info "Checking dependencies with objdump..."
try {
    $deps = & objdump -p $exePath 2>$null | Select-String "DLL Name"
    if ($deps) {
        Write-Info "Dependencies found:"
        $deps | ForEach-Object { Write-Host "  $($_.Line.Trim())" }
    }
} catch {
    Write-Warning "objdump not available, skipping dependency check"
}

# 生成测试报告
$reportFile = "package_test_report.txt"
$report = @"
Package Test Report
==================
Date: $(Get-Date)
Package Directory: $PackageDir
Executable: $exePath

Required DLLs Status:
"@

foreach ($dll in $requiredDlls) {
    $status = if (Test-Path (Join-Path $binDir $dll)) { "✓ Found" } else { "✗ Missing" }
    $report += "`n${dll}: $status"
}

if ($missingDlls.Count -gt 0) {
    $report += "`n`nMissing DLLs:"
    $missingDlls | ForEach-Object { $report += "`n- $_" }
    $report += "`n`nRecommendation: Re-run packaging with improved windeployqt settings"
} else {
    $report += "`n`n✓ All required DLLs are present"
}

$report | Out-File -FilePath $reportFile -Encoding UTF8
Write-Success "Test report saved to: $reportFile"

if ($missingDlls.Count -gt 0) {
    Write-Error "Package test failed - missing $($missingDlls.Count) DLLs"
    exit 1
} else {
    Write-Success "Package test passed - all required components present"
    exit 0
}
