# =================================================================
# LoimReader Windows 一键部署脚本 (PowerShell版本)
# 功能：构建 -> 打包ZIP -> 生成version.json -> 准备OSS上传
# =================================================================
# 参数定义
Param(
    [Alias("v")]
    [string]$Version = "2.7.2",
    
    [Alias("c")]
    [switch]$Clean,
    
    [Alias("s")]
    [switch]$SkipBuild,
    
    [Alias("u")]
    [switch]$Upload,
    
    [Alias("h")]
    [switch]$Help
)

# 显示帮助
function Show-Help {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host " LoimReader Windows 一键部署脚本" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "用法: .\onekey_win_deploy.ps1 [选项]" -ForegroundColor White
    Write-Host ""
    Write-Host "选项:" -ForegroundColor Yellow
    Write-Host "  -Version, -v VERSION     指定版本号（默认: 2.7.2）" -ForegroundColor White
    Write-Host "  -Clean, -c               构建前清理" -ForegroundColor White
    Write-Host "  -SkipBuild, -s           跳过构建，仅打包" -ForegroundColor White
    Write-Host "  -Upload, -u              构建完成后上传到OSS" -ForegroundColor White
    Write-Host "  -Help, -h                显示帮助" -ForegroundColor White
    Write-Host ""
    Write-Host "示例:" -ForegroundColor Yellow
    Write-Host "  .\onekey_win_deploy.ps1                  # 完整构建和打包" -ForegroundColor Green
    Write-Host "  .\onekey_win_deploy.ps1 -c               # 清理后重新构建" -ForegroundColor Green
    Write-Host "  .\onekey_win_deploy.ps1 -v 2.7.3         # 指定版本号" -ForegroundColor Green
    Write-Host "  .\onekey_win_deploy.ps1 -u               # 构建并上传到OSS" -ForegroundColor Green
    Write-Host "  .\onekey_win_deploy.ps1 -s -u            # 跳过构建，仅打包并上传" -ForegroundColor Green
    Write-Host ""
    exit 0
}

# 如果指定了帮助参数，显示帮助后退出
if ($Help) {
    Show-Help
}

# 设置错误处理
$ErrorActionPreference = "Stop"

# 项目配置
$CLIENT_DIR = "loimreader"
$PROJECT_VERSION = $Version
$SCRIPT_DIR = $PSScriptRoot
$DEPLOY_DIR = Join-Path $SCRIPT_DIR "..\deploy"
$UPLOAD_DIR = Join-Path $DEPLOY_DIR "uploads\$CLIENT_DIR"
$BUILD_DIR = Join-Path $SCRIPT_DIR "build_release"
$DIST_DIR = Join-Path $SCRIPT_DIR "dist"

# OSS配置
$OSS_BUCKET = "limereader-releases"
$OSS_BASE_URL = "https://limereader-releases.oss-cn-hangzhou.aliyuncs.com"
$OSS_PREFIX = "updates/$CLIENT_DIR"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " LoimReader Windows 一键部署脚本" -ForegroundColor Cyan
Write-Host " 版本: $PROJECT_VERSION" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查Qt环境
if (-not $env:QT_DIR) {
    Write-Host "[ERROR] QT_DIR 未设置，请设置Qt安装路径" -ForegroundColor Red
    Write-Host "示例: `$env:QT_DIR='C:\Qt\6.10.0\mingw_64'" -ForegroundColor Yellow
    Write-Host "或在 PowerShell 中运行: `$env:QT_DIR='你的Qt路径'; .\onekey_win_deploy.ps1" -ForegroundColor Yellow
    exit 1
}

$qmakePath = Join-Path $env:QT_DIR "bin\qmake.exe"
if (-not (Test-Path $qmakePath)) {
    Write-Host "[ERROR] Qt未找到: $env:QT_DIR" -ForegroundColor Red
    exit 1
}

Write-Host "[INFO] 使用Qt路径: $env:QT_DIR" -ForegroundColor Green

# 检查MinGW
if (-not $env:MINGW_DIR) {
    Write-Host "[WARNING] MINGW_DIR 未设置，尝试自动检测 Qt 附带的 MinGW" -ForegroundColor Yellow

    # 计算 Qt 根目录，例如 D:\\Qt\\6.8.1\\mingw_64 -> D:\\Qt
    $qtVersionDir = Split-Path $env:QT_DIR -Parent          # D:\\Qt\\6.8.1
    $qtRootDir    = Split-Path $qtVersionDir -Parent        # D:\\Qt
    $toolsDir     = Join-Path $qtRootDir "Tools"           # D:\\Qt\\Tools

    if (Test-Path $toolsDir) {
        $cand = Get-ChildItem -Path $toolsDir -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like "mingw*_64" } |
                Sort-Object Name -Descending |
                Select-Object -First 1

        if ($cand) {
            $env:MINGW_DIR = $cand.FullName
        }
    }
}

if (-not $env:MINGW_DIR) {
    Write-Host "[ERROR] 未能自动找到 MinGW，请手动设置环境变量 MINGW_DIR" -ForegroundColor Red
    Write-Host "示例: `$env:MINGW_DIR='C:\\Qt\\Tools\\mingw1310_64' 或 'C:\\Qt\\Tools\\mingw1120_64'" -ForegroundColor Yellow
    exit 1
}

$gccPath = Join-Path $env:MINGW_DIR "bin\gcc.exe"
if (-not (Test-Path $gccPath)) {
    Write-Host "[ERROR] MinGW 无效，未找到: $gccPath" -ForegroundColor Red
    Write-Host "请确认 MINGW_DIR 指向的是 Qt Tools 下的 mingw*_64 目录" -ForegroundColor Yellow
    exit 1
}

Write-Host "[INFO] 使用MinGW路径: $env:MINGW_DIR" -ForegroundColor Green

# 设置PATH
$env:Path = "$env:QT_DIR\bin;$env:MINGW_DIR\bin;$env:Path"

# 清理旧文件（如果指定了 -Clean 参数）
if ($Clean) {
    Write-Host ""
    Write-Host "[INFO] 清理旧的构建文件..." -ForegroundColor Cyan
    if (Test-Path $BUILD_DIR) { Remove-Item -Recurse -Force $BUILD_DIR }
    if (Test-Path $DIST_DIR) { Remove-Item -Recurse -Force $DIST_DIR }
    if (Test-Path $UPLOAD_DIR) { Remove-Item -Recurse -Force $UPLOAD_DIR }
    Write-Host "[SUCCESS] 清理完成" -ForegroundColor Green
} elseif (-not $SkipBuild) {
    # 如果不是跳过构建，清理上传目录
    Write-Host ""
    Write-Host "[INFO] 清理上传目录..." -ForegroundColor Cyan
    if (Test-Path $UPLOAD_DIR) { Remove-Item -Recurse -Force $UPLOAD_DIR }
}

# 构建应用程序
if (-not $SkipBuild) {
    Write-Host ""
    Write-Host "[INFO] 开始构建应用程序..." -ForegroundColor Cyan
    
    # 创建构建目录
    Write-Host "[INFO] 创建构建目录..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path $BUILD_DIR | Out-Null
    Set-Location $BUILD_DIR

    <# 使用 CMake (替代 qmake)
      构建目录: build_release
      生成目录: build_release
    #>
    Write-Host ""
    Write-Host "[INFO] 配置项目 (CMake)..." -ForegroundColor Cyan

    # 定位 CMake 可执行文件（优先系统 PATH，其次 Qt 自带 Tools/CMake_64/bin/cmake.exe）
    $cmakeExe = (Get-Command cmake -ErrorAction SilentlyContinue)?.Source
    if (-not $cmakeExe) {
        $qtVersionDir = Split-Path $env:QT_DIR -Parent
        $qtRootDir    = Split-Path $qtVersionDir -Parent
        $cmakeCandidate = Join-Path $qtRootDir "Tools\CMake_64\bin\cmake.exe"
        if (Test-Path $cmakeCandidate) { $cmakeExe = $cmakeCandidate }
    }
    if (-not $cmakeExe) {
        Write-Host "[ERROR] 未找到 cmake，请安装 CMake 或在 Qt 维护工具中勾选 Tools/CMake_64" -ForegroundColor Red
        Write-Host "提示: 可以将 CMake 路径加入 PATH，如 C:\\Qt\\Tools\\CMake_64\\bin" -ForegroundColor Yellow
        exit 1
    }

    # 让 CMake 在 Qt 安装中找到 Qt6（依赖于 CMAKE_PREFIX_PATH）
    $cmakeEnv = @{ CMAKE_PREFIX_PATH = "$env:QT_DIR" }

    # 生成 MinGW Makefiles（传入版本号）
    & $cmakeExe -G "MinGW Makefiles" -S .. -B . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$($cmakeEnv.CMAKE_PREFIX_PATH)" -DPROJECT_VERSION_OVERRIDE="$PROJECT_VERSION"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] CMake 配置失败" -ForegroundColor Red
        exit 1
    }

    Write-Host ""
    Write-Host "[INFO] 编译应用 (cmake --build)..." -ForegroundColor Cyan
    & $cmakeExe --build . --config Release -j 4
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] 编译失败" -ForegroundColor Red
        exit 1
    }

    Write-Host "[SUCCESS] 应用编译完成" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "[INFO] 跳过构建，使用现有构建结果..." -ForegroundColor Yellow
    
    # 检查构建产物是否存在
    $builtExe = Join-Path $BUILD_DIR "bin\LoimReader.exe"
    if (-not (Test-Path $builtExe)) {
        Write-Host "[ERROR] 找不到已构建的程序: $builtExe" -ForegroundColor Red
        Write-Host "        请先运行完整构建或移除 -SkipBuild 参数" -ForegroundColor Yellow
        exit 1
    }
    Write-Host "[SUCCESS] 找到已构建的程序" -ForegroundColor Green
}

# 创建发布目录
Write-Host ""
Write-Host "[INFO] 准备发布文件..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path "$DIST_DIR\bin" | Out-Null

# CMake 输出到 build_release/bin/LoimReader(.exe)
$builtExe = Join-Path $BUILD_DIR "bin\LoimReader.exe"
if (-not (Test-Path $builtExe)) {
    # 兼容 qmake 旧产物
    $builtExe = Join-Path $BUILD_DIR "release\ctdy123.exe"
}
Copy-Item $builtExe "$DIST_DIR\bin\LoimReader.exe" -Force

# 部署Qt依赖
Write-Host ""
Write-Host "[INFO] 部署Qt依赖库..." -ForegroundColor Cyan
Set-Location "$DIST_DIR\bin"
& windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw LoimReader.exe
if ($LASTEXITCODE -ne 0) {
    Write-Host "[WARNING] windeployqt执行有警告" -ForegroundColor Yellow
}

# 复制MinGW运行时
Write-Host "[INFO] 复制MinGW运行时库..." -ForegroundColor Cyan
Copy-Item "$env:MINGW_DIR\bin\libgcc_s_seh-1.dll" . -Force
Copy-Item "$env:MINGW_DIR\bin\libstdc++-6.dll" . -Force
Copy-Item "$env:MINGW_DIR\bin\libwinpthread-1.dll" . -Force

# 生成Inno Setup安装程序
Write-Host ""
Write-Host "[INFO] 生成安装程序..." -ForegroundColor Cyan

# 查找Inno Setup编译器
$isccPaths = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
)

$isccExe = $null
foreach ($path in $isccPaths) {
    if (Test-Path $path) {
        $isccExe = $path
        break
    }
}

if (-not $isccExe) {
    Write-Host "[WARNING] 未找到Inno Setup编译器 (ISCC.exe)" -ForegroundColor Yellow
    Write-Host "          跳过安装程序生成，仅创建ZIP包" -ForegroundColor Yellow
    Write-Host "          下载地址: https://jrsoftware.org/isdl.php" -ForegroundColor Yellow
} else {
    Write-Host "[INFO] 使用Inno Setup: $isccExe" -ForegroundColor Green
    
    # 返回项目根目录编译ISS脚本
    Set-Location $SCRIPT_DIR
    
    # 编译安装程序，传入版本号参数
    & $isccExe "installer.iss" /DAppVersion=$PROJECT_VERSION
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[SUCCESS] 安装程序生成成功" -ForegroundColor Green
    } else {
        Write-Host "[WARNING] 安装程序生成失败，继续创建ZIP包" -ForegroundColor Yellow
    }
}

# 创建上传目录
Write-Host ""
Write-Host "[INFO] 创建上传目录..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $UPLOAD_DIR | Out-Null

# 预取远程 version.json 以便合并（如果存在）
$versionJsonPath = Join-Path $UPLOAD_DIR "version.json"
$remoteVersionUrl = "$OSS_BASE_URL/$OSS_PREFIX/version.json"
try {
    Write-Host "[INFO] 尝试从服务器获取现有 version.json..." -ForegroundColor Cyan
    $resp = Invoke-WebRequest -Uri $remoteVersionUrl -UseBasicParsing -Headers @{"Cache-Control"="no-cache"} -TimeoutSec 15
    if ($resp.StatusCode -eq 200 -and $resp.Content) {
        $content = $resp.Content.Trim()
        if ($content.StartsWith("{") -and $content.EndsWith("}")) {
            $content | Set-Content -Path $versionJsonPath -Encoding UTF8
            Write-Host "[SUCCESS] 已下载远程 version.json，稍后将合并 Windows 段" -ForegroundColor Green
        } else {
            Write-Host "[WARNING] 远程 version.json 内容格式异常，跳过下载" -ForegroundColor Yellow
        }
    } else {
        Write-Host "[INFO] 未从服务器获取到有效的 version.json（状态码: $($resp.StatusCode)），将创建新文件" -ForegroundColor Yellow
    }
} catch {
    Write-Host "[INFO] 远程 version.json 不存在或无法访问，将创建新文件" -ForegroundColor Yellow
}

# 复制安装程序到上传目录（如果生成成功）
$SETUP_NAME = "LoimReader-Setup-v${PROJECT_VERSION}.exe"
$setupSourcePath = Join-Path $DIST_DIR "LoimReader-Setup-v${PROJECT_VERSION}.exe"
$setupDestPath = Join-Path $UPLOAD_DIR $SETUP_NAME
$hasSetup = $false

if (Test-Path $setupSourcePath) {
    Copy-Item $setupSourcePath $setupDestPath -Force
    Write-Host "[SUCCESS] 安装程序已复制到上传目录" -ForegroundColor Green
    $hasSetup = $true
} else {
    Write-Host "[INFO] 未找到安装程序，仅打包ZIP" -ForegroundColor Yellow
}

# 打包ZIP
Write-Host ""
Write-Host "[INFO] 创建ZIP包..." -ForegroundColor Cyan
$ZIP_NAME = "LoimReader_v${PROJECT_VERSION}_Windows.zip"
$zipPath = Join-Path $UPLOAD_DIR $ZIP_NAME

Compress-Archive -Path "$DIST_DIR\bin\*" -DestinationPath $zipPath -Force
Write-Host "[SUCCESS] ZIP包创建成功: $ZIP_NAME" -ForegroundColor Green

# 计算文件信息
Write-Host ""
Write-Host "[INFO] 计算文件信息..." -ForegroundColor Cyan

# ZIP文件信息
$zipFile = Get-Item $zipPath
$ZIP_SIZE_BYTES = $zipFile.Length
$ZIP_SIZE_MB = [math]::Round($ZIP_SIZE_BYTES / 1MB, 2)
$md5 = Get-FileHash -Path $zipPath -Algorithm MD5
$ZIP_HASH = $md5.Hash.ToLower()

Write-Host "[INFO] ZIP包大小: $ZIP_SIZE_MB MB" -ForegroundColor Green
Write-Host "[INFO] ZIP包MD5: $ZIP_HASH" -ForegroundColor Green

# 安装程序信息（如果存在）
$SETUP_SIZE_MB = 0
$SETUP_HASH = ""
if ($hasSetup -and (Test-Path $setupDestPath)) {
    $setupFile = Get-Item $setupDestPath
    $SETUP_SIZE_BYTES = $setupFile.Length
    $SETUP_SIZE_MB = [math]::Round($SETUP_SIZE_BYTES / 1MB, 2)
    $setupMd5 = Get-FileHash -Path $setupDestPath -Algorithm MD5
    $SETUP_HASH = $setupMd5.Hash.ToLower()
    
    Write-Host "[INFO] 安装程序大小: $SETUP_SIZE_MB MB" -ForegroundColor Green
    Write-Host "[INFO] 安装程序MD5: $SETUP_HASH" -ForegroundColor Green
}

# 生成version.json
Write-Host ""
Write-Host "[INFO] 生成version.json..." -ForegroundColor Cyan

# 获取当前UTC时间
$RELEASE_DATE = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")

# 创建Windows包信息（仅安装程序，不包含ZIP）
if ($hasSetup) {
    $windowsPackage = @{
        url = $SETUP_NAME
        size = "$SETUP_SIZE_MB MB"
        hash = $SETUP_HASH
        downloadUrl = "$OSS_BASE_URL/$OSS_PREFIX/$SETUP_NAME"
        description = "Windows 安装程序"
        minSystemVersion = "Windows 10 或更高版本"
    }
} else {
    Write-Host "[WARNING] 未找到安装程序，跳过 version.json 更新" -ForegroundColor Yellow
    $windowsPackage = $null
}

if ($windowsPackage) {
    # 尝试读取现有的 version.json（如果存在）
    $existingData = $null
    if (Test-Path $versionJsonPath) {
        try {
            $existingJson = Get-Content $versionJsonPath -Raw -Encoding UTF8
            $existingData = $existingJson | ConvertFrom-Json
            Write-Host "[INFO] 找到现有 version.json，将合并信息" -ForegroundColor Cyan
        } catch {
            Write-Host "[WARNING] 现有 version.json 格式错误，将创建新文件" -ForegroundColor Yellow
        }
    }

    # 创建或更新 version.json
    if ($existingData) {
        # 合并模式：保留其他平台的信息
        $versionData = @{
            latestVersion = $PROJECT_VERSION
            releaseDate = $RELEASE_DATE
            releaseNotes = "LoimReader 版本 $PROJECT_VERSION`n- 性能优化`n- Bug修复`n- 功能改进"
            forceUpdate = $false
            packages = @{}
        }
        
        # 复制现有的其他平台信息
        if ($existingData.packages) {
            foreach ($platform in $existingData.packages.PSObject.Properties) {
                if ($platform.Name -ne "windows") {
                    $versionData.packages[$platform.Name] = $platform.Value
                    Write-Host "[INFO] 保留平台信息: $($platform.Name)" -ForegroundColor Green
                }
            }
        }
        
        # 添加/更新 Windows 信息
        $versionData.packages.windows = $windowsPackage
    } else {
        # 新建模式
        $versionData = @{
            latestVersion = $PROJECT_VERSION
            releaseDate = $RELEASE_DATE
            releaseNotes = "LoimReader 版本 $PROJECT_VERSION`n- 性能优化`n- Bug修复`n- 功能改进"
            forceUpdate = $false
            packages = @{
                windows = $windowsPackage
            }
        }
    }

    # 保存为JSON文件
    $versionData | ConvertTo-Json -Depth 10 | Set-Content -Path $versionJsonPath -Encoding UTF8
    Write-Host "[SUCCESS] version.json 已更新（Windows 平台）" -ForegroundColor Green
}

# 生成OSS上传脚本
Write-Host ""
Write-Host "[INFO] 生成OSS上传脚本..." -ForegroundColor Cyan

$uploadScript = @"
# OSS上传脚本 - 自动生成
`$ErrorActionPreference = "Stop"

`$CLIENT_DIR = "$CLIENT_DIR"
`$VERSION = "$PROJECT_VERSION"
`$BUCKET = "$OSS_BUCKET"
`$ENDPOINT = "oss-cn-hangzhou.aliyuncs.com"
`$OSS_PREFIX = "$OSS_PREFIX"
`$UPLOAD_DIR = `$PSScriptRoot
`$DEPLOY_DIR = Join-Path `$UPLOAD_DIR "..\.."

Write-Host "开始上传 LoimReader v`$VERSION 到阿里云OSS..." -ForegroundColor Cyan
Write-Host ""

# 加载OSS配置（如果存在）
`$ossConfigPath = Join-Path `$DEPLOY_DIR "oss-config.ps1"
if (Test-Path `$ossConfigPath) {
    Write-Host "✓ 加载OSS配置: `$ossConfigPath" -ForegroundColor Green
    . `$ossConfigPath
}

# 检查ossutil（2.x版本）
if (-not (Get-Command ossutil -ErrorAction SilentlyContinue)) {
    Write-Host "❌ 错误: 未找到ossutil" -ForegroundColor Red
    Write-Host "安装方法:" -ForegroundColor Yellow
    Write-Host "  下载地址: https://help.aliyun.com/zh/oss/developer-tools/ossutil" -ForegroundColor Yellow
    Write-Host "  或参考: OSSUTIL_INSTALL.md" -ForegroundColor Yellow
    exit 1
}

# 验证ossutil版本
`$ossutilVersion = & ossutil version 2>&1
if (`$ossutilVersion -notmatch '2\.\d+\.\d+') {
    Write-Host "⚠️  警告: 建议使用 ossutil 2.x 版本" -ForegroundColor Yellow
    Write-Host "  当前版本: `$ossutilVersion" -ForegroundColor Yellow
    Write-Host "  参考: OSSUTIL_UPDATE.md" -ForegroundColor Yellow
}

# 构建认证参数（使用2.x版本的长参数格式）
`$authParams = @()
if (`$env:OSS_ACCESS_KEY_ID -and `$env:OSS_ACCESS_KEY_SECRET) {
    Write-Host "✓ 使用环境变量中的OSS密钥" -ForegroundColor Green
    `$authParams = @(
        "--access-key-id", `$env:OSS_ACCESS_KEY_ID,
        "--access-key-secret", `$env:OSS_ACCESS_KEY_SECRET,
        "--endpoint", "https://`$ENDPOINT"
    )
} else {
    Write-Host "✓ 使用ossutil配置文件中的密钥" -ForegroundColor Green
    Write-Host "提示: 如需使用环境变量，请设置 OSS_ACCESS_KEY_ID 和 OSS_ACCESS_KEY_SECRET" -ForegroundColor Yellow
}

# 上传安装程序
`$setupLocalPath = Join-Path `$UPLOAD_DIR "$SETUP_NAME"
if (Test-Path `$setupLocalPath) {
    Write-Host ""
    Write-Host "上传安装程序..." -ForegroundColor Cyan
    Write-Host "  上传: $SETUP_NAME" -ForegroundColor White
    `$setupOssPath = "oss://`$BUCKET/`$OSS_PREFIX/$SETUP_NAME"
    
    if (`$authParams.Count -gt 0) {
        & ossutil cp `$setupLocalPath `$setupOssPath @authParams
    } else {
        & ossutil cp `$setupLocalPath `$setupOssPath
    }
    
    if (`$LASTEXITCODE -eq 0) {
        Write-Host "  ✓ 上传成功" -ForegroundColor Green
    } else {
        Write-Host "  ❌ 上传失败" -ForegroundColor Red
        exit 1
    }
}

# 上传version.json
Write-Host ""
Write-Host "上传version.json..." -ForegroundColor Cyan
`$versionLocalPath = Join-Path `$UPLOAD_DIR "version.json"
`$versionOssPath = "oss://`$BUCKET/`$OSS_PREFIX/version.json"

if (`$authParams.Count -gt 0) {
    & ossutil cp `$versionLocalPath `$versionOssPath @authParams
} else {
    & ossutil cp `$versionLocalPath `$versionOssPath
}

if (`$LASTEXITCODE -eq 0) {
    Write-Host "✓ version.json上传成功" -ForegroundColor Green
} else {
    Write-Host "❌ version.json上传失败" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host "  上传完成！" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
Write-Host ""
Write-Host "验证链接:" -ForegroundColor Cyan
Write-Host "  version.json:" -ForegroundColor White
Write-Host "    $OSS_BASE_URL/$OSS_PREFIX/version.json" -ForegroundColor Yellow

`$setupLocalPath = Join-Path `$UPLOAD_DIR "$SETUP_NAME"
if (Test-Path `$setupLocalPath) {
    Write-Host ""
    Write-Host "  安装程序:" -ForegroundColor White
    Write-Host "    $OSS_BASE_URL/$OSS_PREFIX/$SETUP_NAME" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "验证命令:" -ForegroundColor Cyan
Write-Host "  curl -s $OSS_BASE_URL/$OSS_PREFIX/version.json" -ForegroundColor Yellow
Write-Host ""

Read-Host "按 Enter 键退出"
"@

$uploadScriptPath = Join-Path $UPLOAD_DIR "upload_to_oss.ps1"
Set-Content -Path $uploadScriptPath -Value $uploadScript -Encoding UTF8

Write-Host "[SUCCESS] 上传脚本已生成" -ForegroundColor Green

# 显示摘要
Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host " 部署完成" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "[SUCCESS] 构建成功" -ForegroundColor Green
Write-Host ""
Write-Host "生成的文件:" -ForegroundColor Cyan
if ($hasSetup) {
    Write-Host "  • 安装程序: $UPLOAD_DIR\$SETUP_NAME ($SETUP_SIZE_MB MB)" -ForegroundColor White
}
Write-Host "  • ZIP包（备份）: $UPLOAD_DIR\$ZIP_NAME ($ZIP_SIZE_MB MB)" -ForegroundColor White
Write-Host "  • version.json: $UPLOAD_DIR\version.json" -ForegroundColor White
Write-Host "  • 上传脚本: $UPLOAD_DIR\upload_to_oss.ps1" -ForegroundColor White
Write-Host ""
Write-Host "下一步:" -ForegroundColor Cyan
Write-Host "  1. 测试程序: $DIST_DIR\bin\LoimReader.exe" -ForegroundColor Yellow
if ($hasSetup) {
    Write-Host "  2. 测试安装程序: $DIST_DIR\$SETUP_NAME" -ForegroundColor Yellow
    Write-Host "  3. 上传OSS: powershell -ExecutionPolicy Bypass -File $UPLOAD_DIR\upload_to_oss.ps1" -ForegroundColor Yellow
    Write-Host "  4. 验证: curl $OSS_BASE_URL/$OSS_PREFIX/version.json" -ForegroundColor Yellow
} else {
    Write-Host "  2. 上传OSS: powershell -ExecutionPolicy Bypass -File $UPLOAD_DIR\upload_to_oss.ps1" -ForegroundColor Yellow
    Write-Host "  3. 验证: curl $OSS_BASE_URL/$OSS_PREFIX/version.json" -ForegroundColor Yellow
}
Write-Host ""

# 返回原始目录
Set-Location $SCRIPT_DIR

# 自动上传到OSS（如果指定了 -Upload 参数）
if ($Upload) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host " 开始上传到OSS" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    $uploadScriptPath = Join-Path $UPLOAD_DIR "upload_to_oss.ps1"
    if (Test-Path $uploadScriptPath) {
        & powershell -ExecutionPolicy Bypass -File $uploadScriptPath
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Write-Host "[SUCCESS] 上传完成" -ForegroundColor Green
        } else {
            Write-Host ""
            Write-Host "[ERROR] 上传失败" -ForegroundColor Red
        }
    } else {
        Write-Host "[ERROR] 找不到上传脚本: $uploadScriptPath" -ForegroundColor Red
    }
    Write-Host ""
}

# 如果没有指定 -Upload，提示用户按 Enter 退出
if (-not $Upload) {
    Read-Host "按 Enter 键退出"
}

