param(
    [Parameter(Mandatory = $true)]
    [string]$MsiPath,
    [Parameter(Mandatory = $true)]
    [string]$ZipPath,
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [Parameter(Mandatory = $true)]
    [ValidateSet('amd64', 'arm64')]
    [string]$Architecture
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-MsiRows {
    param(
        [Parameter(Mandatory = $true)]$Database,
        [Parameter(Mandatory = $true)][string]$Query
    )

    $invoke = [Reflection.BindingFlags]::InvokeMethod
    $get = [Reflection.BindingFlags]::GetProperty
    $view = $Database.GetType().InvokeMember('OpenView', $invoke, $null, $Database, @($Query))
    $view.GetType().InvokeMember('Execute', $invoke, $null, $view, $null) | Out-Null
    $rows = @()
    try {
        while ($true) {
            $record = $view.GetType().InvokeMember('Fetch', $invoke, $null, $view, $null)
            if ($null -eq $record) { break }
            $fieldCount = $record.GetType().InvokeMember('FieldCount', $get, $null, $record, $null)
            $fields = @(for ($index = 1; $index -le $fieldCount; $index += 1) {
                $record.GetType().InvokeMember('StringData', $get, $null, $record, @($index))
            })
            $rows += ,$fields
        }
    }
    finally {
        $view.GetType().InvokeMember('Close', $invoke, $null, $view, $null) | Out-Null
    }
    return ,$rows
}

$resolvedMsi = (Resolve-Path -LiteralPath $MsiPath).Path
$resolvedZip = (Resolve-Path -LiteralPath $ZipPath).Path
$expectedMsiName = "LoimReader_${Version}_windows_${Architecture}.msi"
$expectedZipName = "LoimReader_${Version}_windows_${Architecture}.zip"
if ([IO.Path]::GetFileName($resolvedMsi) -cne $expectedMsiName) {
    throw "Unexpected MSI name: $resolvedMsi"
}
if ([IO.Path]::GetFileName($resolvedZip) -cne $expectedZipName) {
    throw "Unexpected ZIP name: $resolvedZip"
}

$installer = New-Object -ComObject WindowsInstaller.Installer
$database = $installer.GetType().InvokeMember(
    'OpenDatabase',
    [Reflection.BindingFlags]::InvokeMethod,
    $null,
    $installer,
    @($resolvedMsi, 0)
)

$productLanguage = Get-MsiRows $database "SELECT ``Value`` FROM ``Property`` WHERE ``Property``='ProductLanguage'"
if ($productLanguage.Count -ne 1 -or $productLanguage[0][0] -ne '2052') {
    throw 'MSI product language must be Simplified Chinese (2052)'
}
$optionsTitle = Get-MsiRows $database "SELECT ``Text`` FROM ``Control`` WHERE ``Dialog_``='LoimReaderOptionsDlg' AND ``Control``='Title'"
if ($optionsTitle.Count -ne 1 -or $optionsTitle[0][0] -ne '安装选项') {
    throw 'LoimReader installation options dialog is not localized in Chinese'
}

$shortcutProperty = Get-MsiRows $database "SELECT ``Value`` FROM ``Property`` WHERE ``Property``='CREATE_DESKTOP_SHORTCUT'"
if ($shortcutProperty.Count -ne 1 -or $shortcutProperty[0][0] -ne '1') {
    throw 'CREATE_DESKTOP_SHORTCUT must exist and default to 1'
}
$shortcutCondition = Get-MsiRows $database "SELECT ``Condition`` FROM ``Component`` WHERE ``Component``='CM_SHORTCUT_DESKTOP_Runtime'"
if ($shortcutCondition.Count -ne 1 -or $shortcutCondition[0][0] -notmatch 'CREATE_DESKTOP_SHORTCUT') {
    throw 'Desktop shortcut component is not controlled by the installer option'
}
$optionsDialog = Get-MsiRows $database "SELECT ``Dialog`` FROM ``Dialog`` WHERE ``Dialog``='LoimReaderOptionsDlg'"
if ($optionsDialog.Count -ne 1) {
    throw 'LoimReader installation options dialog is missing'
}
$launchAction = Get-MsiRows $database "SELECT ``Action`` FROM ``CustomAction`` WHERE ``Action``='LaunchLoimReader'"
if ($launchAction.Count -ne 1) {
    throw 'Unelevated launch custom action is missing'
}
$shortcutRows = Get-MsiRows $database "SELECT ``Name`` FROM ``Shortcut``"
if (-not ($shortcutRows | Where-Object { $_[0] -match 'LoimReader' })) {
    throw 'MSI does not contain the LoimReader shortcut'
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($resolvedZip)
try {
    $entryNames = @($archive.Entries | ForEach-Object { $_.FullName })
    $expectedEntries = @(
        $expectedMsiName,
        'LoimReader-EULA.txt',
        'SHA256SUMS.txt',
        '安装说明.txt'
    )
    if ($entryNames.Count -ne $expectedEntries.Count) {
        throw "Unexpected ZIP entry count: $($entryNames.Count)"
    }
    foreach ($entry in $expectedEntries) {
        if ($entryNames -cnotcontains $entry) {
            throw "ZIP is missing $entry"
        }
    }

    $hashEntry = $archive.GetEntry('SHA256SUMS.txt')
    $reader = New-Object IO.StreamReader($hashEntry.Open())
    try { $hashText = $reader.ReadToEnd().Trim() }
    finally { $reader.Dispose() }
    $actualHash = (Get-FileHash -LiteralPath $resolvedMsi -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hashText -cne "$actualHash  $expectedMsiName") {
        throw 'SHA256SUMS.txt does not match the packaged MSI'
    }
}
finally {
    $archive.Dispose()
}

Write-Output "Windows MSI and ZIP package verified: $Architecture"
