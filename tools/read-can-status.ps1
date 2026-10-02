<#
.SYNOPSIS
    Read and decode the PSOC Control CAN controller's error state over SWD.

.DESCRIPTION
    Reads the M_CAN Error Counter (ECR) and Protocol Status (PSR) registers
    from a running board without halting it, and decodes them into plain
    English. This is the only way to tell CAN fault classes apart on this
    lab: almost every wiring fault produces the same "CAN send failed"
    console message, but the Last Error Code in PSR distinguishes them.

    Requires KitProg3 (Infineon OpenOCD, shipped with ModusToolbox or the
    ModusToolbox Programming Tools) and a board attached over USB.

.PARAMETER Serial
    KitProg3 probe serial. Optional when only one board is attached; use
    flash-board.ps1 -List to enumerate them.

.PARAMETER Samples
    Number of reads to take (default 3). PSR.LEC latches the last error and
    then reads back as "no change", so several samples over a few seconds
    give a far better picture than one.

.PARAMETER IntervalSeconds
    Delay between samples (default 2).

.EXAMPLE
    .\read-can-status.ps1 -Serial <board-serial>

.EXAMPLE
    .\read-can-status.ps1 -Samples 5 -IntervalSeconds 1
#>
[CmdletBinding()]
param(
    [string]$Serial,
    [int]$Samples = 3,
    [int]$IntervalSeconds = 2,
    [string]$OpenOcdRoot
)

$ErrorActionPreference = 'Stop'

# m_can (can1) register block on PSC3M5.
$CAN_BASE = 0x52800200
$ECR_ADDR = $CAN_BASE + 0x40
$PSR_ADDR = $CAN_BASE + 0x44

$LEC_TEXT = @{
    0 = 'no error - a frame was transferred successfully'
    1 = 'STUFF error - more than 5 equal bits in a row'
    2 = 'FORM error - a fixed-format field had the wrong value'
    3 = 'ACK error - nobody acknowledged our frame (we are alone on the bus)'
    4 = 'BIT1 error - we sent recessive but read back dominant (BUS STUCK DOMINANT)'
    5 = 'BIT0 error - we sent dominant but read back recessive (BUS STUCK RECESSIVE / open)'
    6 = 'CRC error'
    7 = 'no change since this register was last read'
}

$ACT_TEXT = @{
    0 = 'synchronising'
    1 = 'idle'
    2 = 'receiving'
    3 = 'transmitting'
}

function Find-OpenOcdRoot {
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

function Show-Sample {
    param([uint32]$Ecr, [uint32]$Psr, [int]$Index)

    $tec = $Ecr -band 0xFF
    $rec = ($Ecr -shr 8) -band 0x7F
    $rp = ($Ecr -shr 15) -band 1
    $cel = ($Ecr -shr 16) -band 0xFF

    $lec = $Psr -band 7
    $act = ($Psr -shr 3) -band 3
    $ep = ($Psr -shr 5) -band 1
    $ew = ($Psr -shr 6) -band 1
    $bo = ($Psr -shr 7) -band 1
    $dlec = ($Psr -shr 8) -band 7

    Write-Host ""
    Write-Host ("--- sample {0} --- ECR=0x{1:X8}  PSR=0x{2:X8}" -f $Index, $Ecr, $Psr)
    Write-Host ("  TEC (transmit errors) : {0}" -f $tec)
    Write-Host ("  REC (receive errors)  : {0}{1}" -f $rec, $(if ($rp) { ' (receive-error passive)' } else { '' }))
    Write-Host ("  CEL (CAN error logging): {0}" -f $cel)
    Write-Host ("  Activity              : {0}" -f $ACT_TEXT[[int]$act])
    Write-Host ("  Last error (LEC)      : {0}" -f $LEC_TEXT[[int]$lec])
    Write-Host ("  Last data-phase error : {0}" -f $LEC_TEXT[[int]$dlec])

    $flags = @()
    if ($ew) { $flags += 'ERROR WARNING (>=96 errors)' }
    if ($ep) { $flags += 'ERROR PASSIVE' }
    if ($bo) { $flags += 'BUS OFF' }
    if ($flags.Count) {
        Write-Host ("  Fault flags           : {0}" -f ($flags -join ', ')) -ForegroundColor Yellow
    } else {
        Write-Host "  Fault flags           : none"
    }

    # Plain-English interpretation, aimed at the wiring faults this lab can hit.
    $verdict = switch ($lec) {
        3 { 'Nothing is acknowledging us. Expected if the partner board is unpowered, unprogrammed, or the bus is open.' }
        4 { 'The bus is being held DOMINANT. Classic signature of CANL shorted to GND - check for a CANL/GND swap at the screw terminals.' }
        5 { 'We drove dominant but read back recessive. The bus is stuck RECESSIVE or open - a broken/missing wire, or CANH and CANL crossed.' }
        1 { 'Stuff error. Usually corruption from a marginal or intermittent connection.' }
        2 { 'Form error. Usually corruption from a marginal or intermittent connection.' }
        6 { 'CRC error. Usually corruption from a marginal or intermittent connection.' }
        0 { 'A frame moved successfully - the physical layer is healthy.' }
        default { 'No new error since the previous read.' }
    }
    Write-Host ("  Interpretation        : {0}" -f $verdict) -ForegroundColor Cyan
}

$ocdRoot = Find-OpenOcdRoot -Explicit $OpenOcdRoot
$ocd = Join-Path $ocdRoot 'bin\openocd.exe'
$scripts = Join-Path $ocdRoot 'scripts'

$cfg = @(
    'source [find interface/kitprog3.cfg]'
    $(if ($Serial) { "adapter serial $Serial" })
    'set ENABLE_ACQUIRE 0'
    'transport select swd'
    'source [find target/infineon/psc3.cfg]'
) | Where-Object { $_ } | Join-String -Separator '; '

Write-Host ("Reading CAN controller status{0} ..." -f $(if ($Serial) { " from board $Serial" } else { '' }))

for ($i = 1; $i -le $Samples; $i++) {
    # Read without halting: the application keeps running, so the counters
    # reflect live bus behaviour rather than a frozen snapshot.
    # The probe sometimes needs a moment to be released between back-to-back
    # OpenOCD sessions, so retry rather than failing the whole run.
    $words = @()
    for ($attempt = 1; $attempt -le 4; $attempt++) {
        $out = & $ocd -s $scripts -c $cfg `
            -c "init; mdw $ECR_ADDR 1; mdw $PSR_ADDR 1; shutdown" 2>&1

        $words = @($out | Select-String -Pattern '^0x[0-9a-fA-F]+:\s+([0-9a-fA-F]{8})' |
            ForEach-Object { [uint32]::Parse($_.Matches[0].Groups[1].Value, 'HexNumber') })

        if ($words.Count -ge 2) { break }
        Write-Verbose "Probe busy (attempt $attempt), retrying ..."
        Start-Sleep -Milliseconds 700
    }

    if ($words.Count -lt 2) {
        Write-Host ($out -join "`n")
        throw 'Could not read the CAN registers. Is the board attached and powered?'
    }

    Show-Sample -Ecr $words[0] -Psr $words[1] -Index $i

    if ($i -lt $Samples) { Start-Sleep -Seconds $IntervalSeconds }
}

Write-Host ""
Write-Host 'Note: PSR.LEC latches the last error and then reads back as "no change",'
Write-Host 'so treat the first sample as the most informative one.'
