# tests/run_e2e_snake.ps1
$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $workspaceRoot "build\src"
$snakeExe = Join-Path $buildDir "SnakeSimulator\Debug\SnakeSimulator.exe"
$cliExe = Join-Path $buildDir "TrackerCLI\Debug\TrackerCLI.exe"

Write-Host "--- AutogeekAI E2E Test (SnakeSimulator + TrackerCLI) ---" -ForegroundColor Cyan

# 1. Verification of binaries
if (-Not (Test-Path $snakeExe)) {
    Write-Error "SnakeSimulator.exe not found. Did you build in Debug mode?"
}
if (-Not (Test-Path $cliExe)) {
    Write-Error "TrackerCLI.exe not found. Did you build in Debug mode?"
}

# 2. Generate temporary JSON configuration
$configPath = Join-Path $workspaceRoot "test_snake_config.json"
$recordingsDir = Join-Path $workspaceRoot "test_recordings"
if (Test-Path $recordingsDir) { Remove-Item -Recurse -Force $recordingsDir }

$configJson = @"
{
  "target": {
    "mode": "monitor_crop",
    "monitor_index": 0,
    "process_name": "",
    "window_title": "AutogeekAI - Snake Debug Simulator",
    "wait_for_target": true
  },
  "recording": {
    "video": {
      "target_fps": 30,
      "width": 800,
      "height": 600,
      "encoder": "h264_nvenc"
    },
    "audio": {
      "enabled": false
    }
  },
  "storage": {
    "base_output_path": "$($recordingsDir.Replace('\', '\\'))"
  }
}
"@

Set-Content -Path $configPath -Value $configJson

# 3. Launch Snake Simulator
Write-Host "[1/4] Launching SnakeSimulator in background..." -ForegroundColor Yellow
$snakeProcess = Start-Process -FilePath $snakeExe -PassThru -NoNewWindow
Start-Sleep -Seconds 2

if ($snakeProcess.HasExited) {
    Write-Error "SnakeSimulator crashed immediately upon startup."
}

# 4. Launch TrackerCLI
Write-Host "[2/4] Executing TrackerCLI to record Snake for 3 seconds..." -ForegroundColor Yellow
$cliProcess = Start-Process -FilePath $cliExe -ArgumentList "--config `"$configPath`"" -PassThru -NoNewWindow -Wait

# 5. Cleanup Processes
Write-Host "[3/4] Stopping target application..." -ForegroundColor Yellow
Stop-Process -Id $snakeProcess.Id -Force

# 6. Verification
Write-Host "[4/4] Verifying generated artifacts..." -ForegroundColor Yellow

$sessions = Get-ChildItem -Path $recordingsDir -Directory -Filter "session_*"
if ($sessions.Count -eq 0) {
    Write-Error "Test Failed: No session folder was created."
}

$sessionDir = $sessions[0].FullName
$videoFile = Join-Path $sessionDir "video.mkv"
$eventsFile = Join-Path $sessionDir "events.jsonl"

$testPassed = $true

if (Test-Path $videoFile) {
    $size = (Get-Item $videoFile).Length
    Write-Host " - video.mkv found ($size bytes)" -ForegroundColor Green
    if ($size -eq 0) { Write-Error "video.mkv is empty!"; $testPassed = $false }
} else {
    Write-Error "video.mkv not found!"; $testPassed = $false
}

if (Test-Path $eventsFile) {
    $size = (Get-Item $eventsFile).Length
    Write-Host " - events.jsonl found ($size bytes)" -ForegroundColor Green
    if ($size -eq 0) { Write-Error "events.jsonl is empty!"; $testPassed = $false }
} else {
    Write-Error "events.jsonl not found!"; $testPassed = $false
}

# Cleanup
Remove-Item -Force $configPath

if ($testPassed) {
    Write-Host "SUCCESS: End-to-End Test Passed." -ForegroundColor Green
} else {
    Write-Error "FAILED: End-to-End Test failed."
}
