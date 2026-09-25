$ErrorActionPreference = 'Stop'
$SourceDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'src'
$App = [System.IO.File]::ReadAllText((Join-Path $SourceDir 'windows_app.cpp'), [System.Text.Encoding]::UTF8)
$Catalogue = [System.IO.File]::ReadAllText((Join-Path $SourceDir 'localization.hpp'), [System.Text.Encoding]::UTF8)
$Header = [System.IO.File]::ReadAllText((Join-Path $SourceDir 'json_core.hpp'), [System.Text.Encoding]::UTF8)

$Entries = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($Match in [regex]::Matches($Catalogue, '\{L"((?:\\.|[^"\\])*)",\s*L"')) {
    $null = $Entries.Add($Match.Groups[1].Value)
}
$Required = New-Object 'System.Collections.Generic.HashSet[string]'
$Patterns = @(
    'app_l10n::text\(L"((?:\\.|[^"\\])*)"\)',
    'set_label\([^,]+,\s*L"((?:\\.|[^"\\])*)"\)',
    'AppendLocalizedMenu\([^;]*?,\s*L"((?:\\.|[^"\\])*)"\)',
    '(?:CreateChild|CreateWindowExW)\s*\([^;]*?L"(?:STATIC|BUTTON|EDIT)"\s*,\s*L"((?:\\.|[^"\\])*)"',
    'SetStatusKey\(L"((?:\\.|[^"\\])*)"\)',
    'L"((?:\\.|[^"\\])*)"\s*,\s*MB_ICON'
)
foreach ($Pattern in $Patterns) {
    foreach ($Match in [regex]::Matches($App, $Pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
        $Key = $Match.Groups[1].Value
        if ($Key -match '[^\x00-\x7F]') { $null = $Required.Add($Key) }
    }
}
foreach ($Key in $Required) {
    if (-not $Entries.Contains($Key)) { throw "Missing English translation: $Key" }
}

$ReasonBlock = [regex]::Match($Header, 'enum class ErrorReason\s*\{([\s\S]*?)\};').Groups[1].Value
$Reasons = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($Match in [regex]::Matches($ReasonBlock, '\b([A-Z][A-Za-z]+)\b')) {
    if ($Match.Groups[1].Value -ne 'Unknown') { $null = $Reasons.Add($Match.Groups[1].Value) }
}
$Translated = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($Match in [regex]::Matches($App, 'case jsondict::ErrorReason::([A-Za-z]+):')) {
    $null = $Translated.Add($Match.Groups[1].Value)
}
foreach ($Reason in $Reasons) {
    if (-not $Translated.Contains($Reason)) { throw "Missing structured error translation: $Reason" }
}
Write-Host "LOCALE_OK: $($Required.Count) UI keys, $($Reasons.Count) error reasons"
