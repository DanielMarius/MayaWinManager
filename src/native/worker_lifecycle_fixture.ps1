param(
    [Parameter(Mandatory = $true)][string]$WorkerPath,
    [Parameter(Mandatory = $true)][string]$MayaRoot,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$WorkerPath = [IO.Path]::GetFullPath($WorkerPath)
$MayaRoot = [IO.Path]::GetFullPath($MayaRoot)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $WorkerPath -PathType Leaf)) { throw "Worker executable missing: $WorkerPath" }
$pythonConfig = Get-Content -LiteralPath (Join-Path $MayaRoot '.venv\pyvenv.cfg')
$homeLine = $pythonConfig | Where-Object { $_ -match '^home\s*=' } | Select-Object -First 1
$pythonMatch = [regex]::Match([string]$homeLine, '^home\s*=\s*(.+)$')
if (-not $pythonMatch.Success) { throw 'Could not resolve the installed Python home from Maya .venv/pyvenv.cfg.' }
$pythonHome = $pythonMatch.Groups[1].Value.Trim()
$python = Join-Path $pythonHome 'python.exe'
if (-not (Test-Path -LiteralPath $python -PathType Leaf)) { throw "Python executable missing: $python" }

$fixtureRoot = Join-Path $OutputDirectory ('worker-lifecycle-' + [guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $fixtureRoot) { throw "Fixture path already exists: $fixtureRoot" }
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
New-Item -ItemType Directory -Path (Join-Path $fixtureRoot '.venv') | Out-Null
New-Item -ItemType Directory -Path (Join-Path $fixtureRoot 'win-control') | Out-Null
New-Item -ItemType Directory -Path (Join-Path $fixtureRoot 'serve') | Out-Null

$probe = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$probe.Start()
$port = [int]$probe.LocalEndpoint.Port
$probe.Stop()
if ($port -in @(8080, 8082)) { throw "Unexpected Maya production port selected for fixture: $port" }
$profilePath = Join-Path $fixtureRoot 'maya-ds4-abliterated.json'
$serverPath = Join-Path $fixtureRoot 'serve\server.py'
[IO.File]::WriteAllText((Join-Path $fixtureRoot '.venv\pyvenv.cfg'),
    ('home = ' + $pythonHome + [Environment]::NewLine), [Text.UTF8Encoding]::new($false))
$profile = @{ host = '127.0.0.1'; port = $port; model_name = 'manager-lifecycle-fixture'; api_key = '' } | ConvertTo-Json -Compress
[IO.File]::WriteAllText($profilePath, $profile, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $fixtureRoot 'VERSION'), 'fixture' + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))

$vbs = @'
Option Explicit
Dim sh, root, host, port, profile, q, command
If WScript.Arguments.Count <> 4 Then WScript.Quit 2
root = WScript.Arguments(0)
host = WScript.Arguments(1)
port = WScript.Arguments(2)
profile = WScript.Arguments(3)
q = Chr(34)
Set sh = CreateObject("WScript.Shell")
sh.CurrentDirectory = root
command = q & "__PYTHON__" & q & " -u " & q & root & "\maya.py" & q
command = command & " --config " & q & root & "\" & profile & q
command = command & " --host " & q & host & q & " --port " & port
sh.Run command, 0, False
'@
$vbs = $vbs.Replace('__PYTHON__', $python)
[IO.File]::WriteAllText((Join-Path $fixtureRoot 'win-control\LaunchHidden.vbs'), $vbs, [Text.UTF8Encoding]::new($false))

$supervisor = @'
import argparse
import subprocess
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--config", required=True)
parser.add_argument("--host", required=True)
parser.add_argument("--port", type=int, required=True)
args = parser.parse_args()
server = Path(__file__).parent / "serve" / "server.py"
flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
child = subprocess.Popen(
    [sys.executable, "-u", str(server), "--config", args.config, "--host", args.host, "--port", str(args.port)],
    stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)
sys.exit(child.wait())
'@
[IO.File]::WriteAllText((Join-Path $fixtureRoot 'maya.py'), $supervisor, [Text.UTF8Encoding]::new($false))

$server = @'
import argparse
import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlsplit

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = urlsplit(self.path).path
        if path == "/health":
            payload = {"model": "manager-lifecycle-fixture", "version": "fixture",
                       "images": False, "max_context": 131072, "unloaded": False}
        elif path == "/api/update":
            payload = {"current": "fixture", "latest": {"version": "fixture"},
                       "newer": False, "can_update": False}
        else:
            self.send_error(404)
            return
        body = json.dumps(payload).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        pass

parser = argparse.ArgumentParser()
parser.add_argument("--config", required=True)
parser.add_argument("--host", required=True)
parser.add_argument("--port", type=int, required=True)
args = parser.parse_args()
HTTPServer((args.host, args.port), Handler).serve_forever()
'@
[IO.File]::WriteAllText($serverPath, $server, [Text.UTF8Encoding]::new($false))

function Invoke-FixtureWorker([string]$Action) {
    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $WorkerPath
    $start.WorkingDirectory = $fixtureRoot
    $start.Arguments = '"' + $Action + '" "' + $fixtureRoot + '" "' + $profilePath + '"'
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::Start($start)
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(45000)) {
        $process.Kill()
        $process.WaitForExit()
        throw "Worker action '$Action' timed out after 45 seconds."
    }
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    if ($process.ExitCode -ne 0) {
        throw "Worker action '$Action' failed (exit $($process.ExitCode)): $stdout $stderr"
    }
    try { return ($stdout | ConvertFrom-Json -ErrorAction Stop) }
    catch { throw "Worker action '$Action' returned invalid JSON: $stdout $stderr" }
}

function Wait-FixtureHealth {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        try {
            $health = Invoke-RestMethod -Uri "http://127.0.0.1:$port/health" -TimeoutSec 1
            if ($health.model -eq 'manager-lifecycle-fixture') { return $health }
        } catch {}
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'The isolated Worker did not make the fixture health endpoint ready.'
}

function Get-FixtureProcesses {
    @((Get-CimInstance Win32_Process -ErrorAction Stop) | Where-Object {
        $_.CommandLine -and $_.CommandLine.IndexOf($fixtureRoot, [StringComparison]::OrdinalIgnoreCase) -ge 0
    })
}

function Get-FixtureServerProcesses {
    @((Get-CimInstance Win32_Process -Filter "Name='python.exe'" -ErrorAction Stop) | Where-Object {
        $_.CommandLine -and
        $_.CommandLine.IndexOf($serverPath, [StringComparison]::OrdinalIgnoreCase) -ge 0 -and
        $_.CommandLine.IndexOf($profilePath, [StringComparison]::OrdinalIgnoreCase) -ge 0
    })
}

function Wait-PortReleased([int]$Seconds = 5) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        $client = [System.Net.Sockets.TcpClient]::new()
        try {
            $client.Connect([System.Net.IPAddress]::Loopback, $port)
            $client.Dispose()
        } catch {
            $client.Dispose()
            return $true
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    return $false
}

function Wait-FixtureStopped([int]$Seconds = 5) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        if ((Wait-PortReleased 1) -and @(Get-FixtureProcesses).Count -eq 0) { return $true }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    return $false
}

$startAttempted = $false
$primaryFailure = $null
$cleanupFailure = $null
try {
    $startAttempted = $true
    $null = Invoke-FixtureWorker 'start'
    $null = Wait-FixtureHealth
    if (@(Get-FixtureServerProcesses).Count -ne 1) { throw 'Start did not create exactly one fixture server.' }
    Write-Output 'PASS Worker Start: isolated health endpoint is ready.'

    $null = Invoke-FixtureWorker 'start'
    $null = Wait-FixtureHealth
    if (@(Get-FixtureServerProcesses).Count -ne 1) { throw 'Idempotent Start created more than one fixture server.' }
    Write-Output 'PASS Worker Start again: one fixture server remains.'

    $restart = Invoke-FixtureWorker 'restart'
    if ($restart.online -ne $true) { throw 'Worker Restart did not report an online fixture.' }
    $health = Wait-FixtureHealth
    if ($health.model -ne 'manager-lifecycle-fixture' -or @(Get-FixtureServerProcesses).Count -ne 1) {
        throw 'Worker Restart did not restore exactly one healthy fixture server.'
    }
    Write-Output 'PASS Worker Restart: the isolated service returned healthy.'

    $null = Invoke-FixtureWorker 'stop'
    if (-not (Wait-FixtureStopped 8)) { throw 'Worker Stop left the fixture port or processes running.' }
    $startAttempted = $false
    Write-Output 'PASS Worker Stop: the fixture port and process tree are closed.'
} catch {
    $primaryFailure = $_.Exception.Message
}

if ($startAttempted) {
    $stopped = $false
    for ($attempt = 0; $attempt -lt 3 -and -not $stopped; $attempt++) {
        try { $null = Invoke-FixtureWorker 'stop' } catch { $cleanupFailure = $_.Exception.Message }
        $stopped = Wait-FixtureStopped 5
        if (-not $stopped) { Start-Sleep -Milliseconds 250 }
    }
    if (-not $stopped -and -not $cleanupFailure) { $cleanupFailure = 'Fixture cleanup left its port or process tree running.' }
}
if ($primaryFailure) {
    if ($cleanupFailure) { throw "$primaryFailure Cleanup also failed: $cleanupFailure" }
    throw $primaryFailure
}
if ($cleanupFailure) { throw "Fixture cleanup failed: $cleanupFailure" }
Write-Output "Worker lifecycle fixture passed on isolated loopback port $port."
