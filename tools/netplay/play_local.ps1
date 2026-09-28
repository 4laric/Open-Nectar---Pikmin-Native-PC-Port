# Local two-window netplay test (issue #887): one human, two visible windows
# on one PC. The host plays on the keyboard, the joiner on the first
# gamepad. No environment variables needed.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\netplay\play_local.ps1 `
#     -Exe <path\to\nectar.exe> [-Bootstrap <seed-bootstrap.txt>] `
#     [-Stun none] [-OutDir <dir>]
#
# How it works: the host (--netplay-host-ice) gathers an offer code that
# already carries the session setup (seed, host settings, bootstrap bytes),
# so the joiner needs no file from the host. The script moves the codes
# between the two processes through files (offer/answer .txt next to the
# script's output dir) -- the same files a human would copy-paste through.
#
# The owner uses this for the first human test. Automated evidence must
# NEVER run it visibly: pass -Hidden (sets PIKMIN_RANDOMIZER_TEST_BACKGROUND=1
# + SDL_AUDIODRIVER=dummy and adds --netplay-test-hidden --netplay-test-ticks)
# so both peers run hidden and quit after N ticks.
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [string]$Bootstrap = "",
    [string]$Stun = "",
    [string]$OutDir = "",
    [switch]$Hidden,
    [int]$Ticks = 3000
)

$ErrorActionPreference = "Stop"
$exePath = (Resolve-Path $Exe).Path
$root = if ($OutDir -ne "") { $OutDir } else { Join-Path ([IO.Path]::GetTempPath()) "nectar-netplay-local" }
New-Item -ItemType Directory -Force -Path $root | Out-Null
$offerFile  = Join-Path $root "local_offer.txt"
$answerFile = Join-Path $root "local_answer.txt"
Remove-Item -Force -ErrorAction SilentlyContinue $offerFile, $answerFile

$hostArgs = @("--netplay-host-ice", "--netplay-code-out", $offerFile,
              "--netplay-answer-in", $answerFile, "--netplay-input", "keyboard")
$joinArgs = @("--netplay-join-ice", "@$offerFile", "--netplay-code-out", $answerFile,
              "--netplay-input", "gamepad:0")
if ($Bootstrap -ne "") {
    $hostArgs += @("--bootstrap", (Resolve-Path $Bootstrap).Path)
}
if ($Hidden) {
    $hostArgs += @("--netplay-test-hidden", "--netplay-test-ticks", "$Ticks")
    $joinArgs += @("--netplay-test-hidden", "--netplay-test-ticks", "$Ticks")
    $env:PIKMIN_RANDOMIZER_TEST_BACKGROUND = "1"
    $env:SDL_AUDIODRIVER = "dummy"
}
if ($Stun -ne "") { $env:PIKMIN_NETPLAY_STUN = $Stun }

Write-Host "play_local: exe=$exePath"
Write-Host "play_local: codes in $root"
Write-Host "play_local: starting host (keyboard) ..."
$hostProc = Start-Process -FilePath $exePath -ArgumentList $hostArgs -PassThru
try {
    $deadline = [DateTime]::Now.AddMinutes(10)
    while (-not (Test-Path $offerFile) -and [DateTime]::Now -lt $deadline -and -not $hostProc.HasExited) {
        Start-Sleep -Milliseconds 500
    }
    if (-not (Test-Path $offerFile)) { throw "host produced no offer code (exit=$($hostProc.HasExited))" }
    Write-Host "play_local: offer ready ($((Get-Item $offerFile).Length) bytes), starting joiner (gamepad:0) ..."
    $joinProc = Start-Process -FilePath $exePath -ArgumentList $joinArgs -PassThru
    try {
        if ($Hidden) {
            $joinProc.WaitForExit(1200000) | Out-Null
            $hostProc.WaitForExit(60000) | Out-Null
            Write-Host "play_local: hidden run done host_exit=$($hostProc.ExitCode) join_exit=$($joinProc.ExitCode)"
        } else {
            Write-Host "play_local: both windows up -- close them to finish (or Ctrl+C here)."
            Wait-Process -Id $hostProc.Id, $joinProc.Id
        }
    } finally {
        if (-not $Hidden) {
            foreach ($p in @($hostProc, $joinProc)) {
                if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
            }
        }
    }
} finally {
    if ($hostProc -and -not $hostProc.HasExited -and $Hidden) {
        Stop-Process -Id $hostProc.Id -Force -ErrorAction SilentlyContinue
    }
}
