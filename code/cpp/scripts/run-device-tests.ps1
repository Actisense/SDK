<#
.SYNOPSIS
    Build and run the Actisense SDK device-targeted integration tests against a
    real device (or a Product Emulator) on a serial port.

.DESCRIPTION
    The SDK integration tests read their target hardware from ACTISENSE_TEST_*
    environment variables and skip themselves when the required port(s) are not
    supplied. This wrapper sets those variables, (optionally) builds the test
    targets, and runs CTest.

    Only -Port is required. The optional parameters enable the multi-device /
    live-bus tests; omit them and those tests simply skip rather than fail:

      -RxPort      second device (receiver) for the two-device Tx-PGN blocking
                   sweeps (GIT-89/97/98/99)
      -NgtPort     NGT-1 port for the ISO-Request divergence diagnostic (GIT-106)
      -NgxPort     NGX-1 port for the ISO-Request divergence diagnostic (GIT-106)
      -RemoteAddr  N2K source address of a remote device for the remote-BEM
                   sweep (GIT-92); the test defaults to 200 when omitted

.PARAMETER Port
    Serial port of the device under test (e.g. COM20 or /dev/ttyUSB0). Required.

.PARAMETER Baud
    Serial baud rate. Default 115200.

.PARAMETER RxPort
    Optional receiver port for the two-device Tx-PGN blocking sweeps.

.PARAMETER NgtPort
    Optional NGT-1 port for the GIT-106 ISO-Request diagnostic.

.PARAMETER NgxPort
    Optional NGX-1 port for the GIT-106 ISO-Request diagnostic.

.PARAMETER RemoteAddr
    Optional N2K source address (0-251) of a remote device for the remote-BEM
    sweep.

.PARAMETER Config
    CMake build configuration. Default Release.

.PARAMETER BuildDir
    CMake build directory. Default 'build' under the cpp root.

.PARAMETER Filter
    Optional CTest -R regex restricting which tests run. Matches the gtest test
    names (Suite.Test), NOT the executable name, e.g. 'BemDeviceTest' runs the
    single-device BEM suite.

.PARAMETER AllowReboot
    Set ACTISENSE_TEST_REBOOT_OK=1 to enable the destructive reboot test.

.PARAMETER PgnDiag
    Set ACTISENSE_TEST_PGN_DIAG=1 to enable the opt-in PGN-list diagnostics.

.PARAMETER SkipBuild
    Run CTest without rebuilding first.

.EXAMPLE
    ./run-device-tests.ps1 -Port COM20

.EXAMPLE
    ./run-device-tests.ps1 -Port COM20 -Baud 115200 -Filter BemDeviceTest

.EXAMPLE
    ./run-device-tests.ps1 -Port COM20 -RxPort COM21 -RemoteAddr 200
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Port,

    [int]    $Baud = 115200,
    [string] $RxPort,
    [string] $NgtPort,
    [string] $NgxPort,
    [int]    $RemoteAddr,
    [string] $Config = 'Release',
    [string] $BuildDir,
    [string] $Filter,
    [switch] $AllowReboot,
    [switch] $PgnDiag,
    [switch] $SkipBuild
)

$ErrorActionPreference = 'Stop'

# cpp root is the parent of this scripts/ directory.
$CppRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    $BuildDir = Join-Path $CppRoot 'build'
}

if (-not (Test-Path $BuildDir)) {
    Write-Error ("Build directory '{0}' not found. Configure it first, e.g.:`n" +
        "  cmake -B `"{0}`" -S `"{1}`" -DACTISENSE_BUILD_TESTS=ON" -f $BuildDir, $CppRoot)
}

# --- Required target ------------------------------------------------------- #
$env:ACTISENSE_TEST_PORT = $Port
$env:ACTISENSE_TEST_BAUD = "$Baud"

# --- Optional multi-device / live-bus rigs --------------------------------- #
if ($RxPort)              { $env:ACTISENSE_TEST_RX_PORT     = $RxPort }
if ($NgtPort)             { $env:ACTISENSE_TEST_NGT_PORT    = $NgtPort }
if ($NgxPort)             { $env:ACTISENSE_TEST_NGX_PORT    = $NgxPort }
if ($PSBoundParameters.ContainsKey('RemoteAddr')) { $env:ACTISENSE_TEST_REMOTE_ADDR = "$RemoteAddr" }

# --- Optional opt-in / destructive gates ----------------------------------- #
if ($AllowReboot)         { $env:ACTISENSE_TEST_REBOOT_OK   = '1' }
if ($PgnDiag)             { $env:ACTISENSE_TEST_PGN_DIAG     = '1' }

Write-Host "Device test target : $Port @ $Baud" -ForegroundColor Cyan
if ($RxPort)  { Write-Host "Rx receiver port   : $RxPort" }
if ($NgtPort) { Write-Host "NGT-1 port         : $NgtPort" }
if ($NgxPort) { Write-Host "NGX-1 port         : $NgxPort" }
if ($PSBoundParameters.ContainsKey('RemoteAddr')) { Write-Host "Remote N2K address : $RemoteAddr" }
if ($AllowReboot) { Write-Host "Reboot test        : ENABLED (destructive)" -ForegroundColor Yellow }
Write-Host "Build dir / config : $BuildDir [$Config]"
Write-Host ""

# --- Build ----------------------------------------------------------------- #
if (-not $SkipBuild) {
    Write-Host "Building test targets..." -ForegroundColor Cyan
    cmake --build $BuildDir --config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed (exit $LASTEXITCODE)."
    }
}

# --- Run ------------------------------------------------------------------- #
$ctestArgs = @('--test-dir', $BuildDir, '-C', $Config, '--output-on-failure')
if ($Filter) {
    $ctestArgs += @('-R', $Filter)
}

Write-Host "Running: ctest $($ctestArgs -join ' ')" -ForegroundColor Cyan
ctest @ctestArgs
exit $LASTEXITCODE
