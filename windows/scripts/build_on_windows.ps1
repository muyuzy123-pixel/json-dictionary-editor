param(
    [ValidateSet('x64', 'ARM64')]
    [string]$Architecture = 'x64',
    [string]$BuildDir = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try {
    $ProjectDir = Split-Path -Parent $PSScriptRoot
    & (Join-Path $PSScriptRoot 'verify_localizations.ps1')
    if ([string]::IsNullOrWhiteSpace($BuildDir)) {
        $BuildDir = Join-Path $ProjectDir ('build-windows-' + $Architecture.ToLowerInvariant())
    }
    $cmakeCommand = Get-Command cmake -CommandType Application -ErrorAction Stop
    $ctestCommand = Get-Command ctest -CommandType Application -ErrorAction Stop

    & $cmakeCommand.Source -S $ProjectDir -B $BuildDir -G 'Visual Studio 17 2022' -A $Architecture -DJDE_BUILD_GUI=ON
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed with exit code $LASTEXITCODE" }

    & $cmakeCommand.Source --build $BuildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE" }

    & $ctestCommand.Source --test-dir $BuildDir -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Core tests failed with exit code $LASTEXITCODE" }

    $metadata = Join-Path $BuildDir 'build-info-Release.txt'
    foreach ($tool in @($cmakeCommand.Source, $ctestCommand.Source)) {
        $hash = Get-FileHash -LiteralPath $tool -Algorithm SHA256
        Add-Content -LiteralPath $metadata -Value ('tool_sha256={0} {1}' -f $hash.Hash, [System.IO.Path]::GetFileName($tool))
    }
    $Exe = Join-Path $BuildDir 'Release\JSONDictionaryEditor.exe'
    $hash = Get-FileHash -LiteralPath $Exe -Algorithm SHA256
    Add-Content -LiteralPath $metadata -Value ('output_sha256={0} JSONDictionaryEditor.exe' -f $hash.Hash)

    # A GUI-subsystem executable must be explicitly awaited for reliable exit codes.
    $version = Start-Process -FilePath $Exe -ArgumentList '--version' -Wait -PassThru
    if ($version.ExitCode -ne 0) { throw "Version diagnostic failed with exit code $($version.ExitCode)" }
    Write-Host "Build completed: $Exe"
    Write-Host "Metadata: $metadata"
    Write-Host 'Core tests passed. Integrated file tests and GUI acceptance remain separate.'
    exit 0
}
catch {
    Write-Error -Message $_ -ErrorAction Continue
    exit 1
}
