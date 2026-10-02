<#
.SYNOPSIS
    Lists KitProg3 probes and flashes a specific KIT_PSC3M5_EVK.

.DESCRIPTION
    With two boards on one machine, OpenOCD has to be told which probe to
    use, otherwise it grabs whichever it enumerates first. This wraps that
    selection so each board can be targeted deliberately.

    Nothing about the local setup is hardcoded: the OpenOCD and fw-loader
    executables are discovered at runtime, and probe serials are always
    read from the connected hardware. Serial numbers are unique per board,
    so they differ for every instructor and every kit.

    The board's OpenOCD config also honours _ZEPHYR_BOARD_SERIAL, which
    `west flash --dev-id <serial>` sets. Prefer plain `west flash` when you
    build on the same machine as the boards. Use this script when the build
    happens elsewhere (for example a remote Linux build host) and all you
    have locally is a .hex.

.PARAMETER Serial
    Probe serial to target. Optional. If omitted and exactly one probe is
    connected it is used automatically; if several are connected you get a
    numbered list to choose from.

.EXAMPLE
    .\flash-board.ps1 -List

    Show every connected KitProg3 with its serial, plus the USB-UART COM
    ports currently enumerated.

.EXAMPLE
    .\flash-board.ps1 -HexFile .\command.hex

    Flash the only connected board, or pick from a list if there are more.

.EXAMPLE
    .\flash-board.ps1 -HexFile .\telemetry.hex -Serial <serial-from--List>

    Flash one specific board non-interactively - use this in scripts.
#>
[CmdletBinding(DefaultParameterSetName = 'Flash')]
param(
    [Parameter(ParameterSetName = 'List')]
    [switch]$List,

    [Parameter(ParameterSetName = 'Flash', Mandatory = $true)]
    [string]$HexFile,

    [Parameter(ParameterSetName = 'Flash')]
    [string]$Serial,

    # Only needed if auto-discovery fails or several installs are present.
    [string]$OpenOcdRoot,
    [string]$FwLoader
)

$ErrorActionPreference = 'Stop'

function Find-OpenOcdRoot {
    <#
        Infineon ships OpenOCD inside both the ModusToolbox Programming
        Tools and full ModusToolbox installs, each under a versioned
        directory. Probe the usual locations and prefer the newest.
    #>
    param([string]$Explicit)

    if ($Explicit) {
        if (-not (Test-Path (Join-Path $Explicit 'bin\openocd.exe'))) {
            throw "No openocd.exe under '$Explicit'."
        }
        return $Explicit
    }

    if ($env:OPENOCD_PATH -and (Test-Path (Join-Path $env:OPENOCD_PATH 'bin\openocd.exe'))) {
        return $env:OPENOCD_PATH
    }

    $candidates = @(
        "$env:ProgramFiles\Infineon\Tools\ModusToolboxProgtools-*\openocd"
        "${env:ProgramFiles(x86)}\Infineon\Tools\ModusToolboxProgtools-*\openocd"
        'C:\Infineon\Tools\ModusToolboxProgtools-*\openocd'
        "$env:USERPROFILE\Infineon\Tools\ModusToolboxProgtools-*\openocd"
        "$env:USERPROFILE\ModusToolbox\tools_*\openocd"
        'C:\ModusToolbox\tools_*\openocd'
    )

    $found = $candidates |
        ForEach-Object { Get-Item -Path $_ -ErrorAction SilentlyContinue } |
        Where-Object { Test-Path (Join-Path $_.FullName 'bin\openocd.exe') } |
        Sort-Object -Property FullName -Descending

    if (-not $found) {
        throw 'Could not find Infineon OpenOCD. Install ModusToolbox or the ModusToolbox Programming Tools, set OPENOCD_PATH, or pass -OpenOcdRoot.'
    }

    return $found[0].FullName
}

function Find-FwLoader {
    param([string]$Explicit)

    if ($Explicit) {
        if (-not (Test-Path $Explicit)) {
            throw "fw-loader not found at '$Explicit'."
        }
        return $Explicit
    }

    $candidates = @(
        "$env:ProgramFiles\Infineon\Tools\ModusToolboxProgtools-*\fw-loader\bin\fw-loader.exe"
        "${env:ProgramFiles(x86)}\Infineon\Tools\ModusToolboxProgtools-*\fw-loader\bin\fw-loader.exe"
        'C:\Infineon\Tools\ModusToolboxProgtools-*\fw-loader\bin\fw-loader.exe'
        "$env:USERPROFILE\Infineon\Tools\ModusToolboxProgtools-*\fw-loader\bin\fw-loader.exe"
        "$env:USERPROFILE\ModusToolbox\tools_*\fw-loader\bin\fw-loader.exe"
        'C:\ModusToolbox\tools_*\fw-loader\bin\fw-loader.exe'
    )

    $found = $candidates |
        ForEach-Object { Get-Item -Path $_ -ErrorAction SilentlyContinue } |
        Sort-Object -Property FullName -Descending

    if (-not $found) {
        throw 'Could not find fw-loader.exe. Pass -FwLoader with an explicit path.'
    }

    return $found[0].FullName
}

function Get-KitProgProbe {
    param([string]$FwLoaderPath)

    & $FwLoaderPath --device-list 2>&1 |
        Select-String -Pattern 'KitProg3\s+CMSIS-DAP\s+\S+?-(\S+)\s+FW Version\s+(\S+)' |
        ForEach-Object {
            [pscustomobject]@{
                Serial    = $_.Matches[0].Groups[1].Value
                FwVersion = $_.Matches[0].Groups[2].Value
            }
        }
}

function Show-UartPort {
    # Windows exposes no reliable probe-serial-to-COM-port mapping, so
    # list the ports and let the operator match them by plug order.
    Write-Host 'KitProg3 USB-UART ports currently enumerated:'
    $ports = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
        Where-Object { $_.FriendlyName -match 'KitProg3 USB-UART' } |
        Select-Object -ExpandProperty FriendlyName

    if ($ports) {
        $ports | ForEach-Object { Write-Host "  $_" }
    } else {
        Write-Host '  (none detected)'
    }
}

$fwLoaderPath = Find-FwLoader -Explicit $FwLoader
$probes = @(Get-KitProgProbe -FwLoaderPath $fwLoaderPath)

if ($List) {
    if (-not $probes) {
        Write-Warning 'No KitProg3 probes detected. Check the USB cables and that the boards are powered.'
        return
    }

    $probes | Format-Table -AutoSize
    Show-UartPort
    return
}

if (-not (Test-Path $HexFile)) {
    throw "Hex file not found: '$HexFile'"
}

if (-not $probes) {
    throw 'No KitProg3 probes detected. Check the USB cables and that the boards are powered.'
}

if ($Serial) {
    if ($probes.Serial -notcontains $Serial) {
        $available = ($probes.Serial -join ', ')
        throw "No connected probe with serial '$Serial'. Connected: $available"
    }
} elseif ($probes.Count -eq 1) {
    $Serial = $probes[0].Serial
    Write-Host "Using the only connected probe: $Serial"
} else {
    Write-Host 'Several boards are connected - choose which one to flash:'
    for ($i = 0; $i -lt $probes.Count; $i++) {
        Write-Host ("  [{0}] {1}  (KitProg3 FW {2})" -f $i, $probes[$i].Serial, $probes[$i].FwVersion)
    }
    Show-UartPort

    $choice = Read-Host "Board index to flash with '$HexFile' (0-$($probes.Count - 1))"
    $index = 0
    if (-not [int]::TryParse($choice, [ref]$index) -or $index -lt 0 -or $index -ge $probes.Count) {
        throw "Invalid selection '$choice'."
    }
    $Serial = $probes[$index].Serial
}

$ocdRoot = Find-OpenOcdRoot -Explicit $OpenOcdRoot
$ocd = Join-Path $ocdRoot 'bin\openocd.exe'
$scripts = Join-Path $ocdRoot 'scripts'

# OpenOCD's TCL parser wants forward slashes, even on Windows.
$hexPath = (Resolve-Path $HexFile).Path -replace '\\', '/'

$configCmd = @(
    'source [find interface/kitprog3.cfg]'
    "adapter serial $Serial"
    'set ENABLE_ACQUIRE 0'
    'transport select swd'
    'source [find target/infineon/psc3.cfg]'
) -join '; '

Write-Host "Flashing '$HexFile' to board $Serial ..."
Write-Verbose "Using OpenOCD at '$ocd'"

& $ocd -s $scripts -c $configCmd -c "init; reset init; program $hexPath verify reset; shutdown" 2>&1 |
    Tee-Object -Variable ocdOutput |
    Select-String -Pattern 'Detected Device|wrote|Verified OK|Resetting Target|Error|error:'

if (($ocdOutput -join "`n") -notmatch 'Verified OK') {
    throw "Flash failed for board $Serial. Check that no debug session holds the probe, then retry."
}

Write-Host "Board $Serial programmed and running." -ForegroundColor Green
