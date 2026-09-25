param(
    [Parameter(Mandatory = $false)]
    [string]$Exe = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($Exe)) {
    $portableExecutables = @([System.IO.Directory]::GetFiles($PSScriptRoot, '*.exe'))
    $sourceExe = Join-Path (Split-Path -Parent $PSScriptRoot) 'dist\JSONDictionaryEditor-Windows-x64.exe'
    if ($portableExecutables.Count -eq 1) {
        $Exe = $portableExecutables[0]
    }
    else {
        $Exe = $sourceExe
    }
}

if (-not (Test-Path -LiteralPath $Exe)) {
    throw "Executable not found: $Exe"
}

$Exe = (Resolve-Path -LiteralPath $Exe).Path
$fileVersion = (Get-Item -LiteralPath $Exe).VersionInfo
if (($fileVersion.FileMajorPart -ne 1) -or ($fileVersion.FileMinorPart -ne 1) -or
    ($fileVersion.FileBuildPart -ne 1)) {
    throw "Expected JSON Dictionary Editor 1.1.1; found $($fileVersion.FileVersion)"
}
$sampleName = 'JSONDictionaryEditor-verify-{0}.json' -f ([Guid]::NewGuid().ToString('N'))
$sample = Join-Path ([System.IO.Path]::GetTempPath()) $sampleName
$gui = $null

try {
    $version = Start-Process -FilePath $Exe -ArgumentList '--version' -PassThru -Wait
    if ($version.ExitCode -ne 0) {
        throw "Version diagnostic failed with exit code $($version.ExitCode)"
    }

    $process = Start-Process -FilePath $Exe -ArgumentList '--self-test' -PassThru -Wait
    if ($process.ExitCode -ne 0) {
        throw "Integrated self-test failed with exit code $($process.ExitCode)"
    }

    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText(
        $sample,
        '{"\u4E2D\u6587":"\u4F60\u597D","precise":1.2300e+04,"nested":{"array":[true,null,"\uD83D\uDE42"]}}',
        $utf8NoBom
    )

    $quotedSample = '"' + $sample + '"'
    $validate = Start-Process -FilePath $Exe -ArgumentList @('--validate-json', $quotedSample) -PassThru -Wait
    if ($validate.ExitCode -ne 0) {
        throw "Sample JSON validation failed with exit code $($validate.ExitCode)"
    }

    $gui = Start-Process -FilePath $Exe -ArgumentList @($quotedSample) -PassThru
    Start-Sleep -Seconds 2
    $gui.Refresh()
    if ($gui.HasExited) {
        throw "GUI exited too early with exit code $($gui.ExitCode)"
    }

    Write-Host 'Automated checks passed: self-test, strict JSON validation, GUI process alive.'
    Write-Host 'Continue with the real-window checklist in the package guide or source README.'
}
finally {
    if (($null -ne $gui) -and (-not $gui.HasExited)) {
        $null = $gui.CloseMainWindow()
        $null = $gui.WaitForExit(3000)
        if (-not $gui.HasExited) {
            Stop-Process -Id $gui.Id -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path -LiteralPath $sample) {
        Remove-Item -LiteralPath $sample -Force -ErrorAction SilentlyContinue
    }
}
