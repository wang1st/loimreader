param(
    [Parameter(Mandatory = $true)]
    [string]$MsiPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [Parameter(Mandatory = $true)]
    [ValidateSet('amd64', 'arm64')]
    [string]$Architecture,
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$') {
    throw "Invalid semantic version: $Version"
}

$resolvedMsi = (Resolve-Path -LiteralPath $MsiPath).Path
$resolvedSource = (Resolve-Path -LiteralPath $SourceDirectory).Path
$expectedMsiName = "LoimReader_${Version}_windows_${Architecture}.msi"
if ([IO.Path]::GetFileName($resolvedMsi) -cne $expectedMsiName) {
    throw "Unexpected MSI name. Expected $expectedMsiName"
}

$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$archiveName = "LoimReader_${Version}_windows_${Architecture}.zip"
$archivePath = Join-Path $outputRoot $archiveName
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}

$stagingDirectory = Join-Path ([IO.Path]::GetTempPath()) (
    'loimreader-windows-package-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($stagingDirectory) | Out-Null

try {
    $stagedMsi = Join-Path $stagingDirectory $expectedMsiName
    Copy-Item -LiteralPath $resolvedMsi -Destination $stagedMsi
    Copy-Item -LiteralPath (
        Join-Path $resolvedSource 'packaging/windows/LoimReader-EULA.txt'
    ) -Destination (Join-Path $stagingDirectory 'LoimReader-EULA.txt')
    Copy-Item -LiteralPath (
        Join-Path $resolvedSource 'packaging/windows/安装说明.txt'
    ) -Destination (Join-Path $stagingDirectory '安装说明.txt')

    $hash = (Get-FileHash -LiteralPath $stagedMsi -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $expectedMsiName" | Set-Content -LiteralPath (
        Join-Path $stagingDirectory 'SHA256SUMS.txt'
    ) -Encoding utf8 -NoNewline

    Compress-Archive -Path (Join-Path $stagingDirectory '*') -DestinationPath $archivePath -CompressionLevel Optimal
    if (-not (Test-Path -LiteralPath $archivePath) -or (Get-Item -LiteralPath $archivePath).Length -le 0) {
        throw 'Windows ZIP package was not created'
    }
}
finally {
    if (Test-Path -LiteralPath $stagingDirectory) {
        Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
    }
}

Write-Output $archivePath
