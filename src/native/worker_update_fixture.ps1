param(
    [Parameter(Mandatory = $true)][string]$WorkerPath,
    [Parameter(Mandatory = $true)][string]$MayaRoot,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$InstallWorkerPath
)

$ErrorActionPreference = 'Stop'
$WorkerPath = [IO.Path]::GetFullPath($WorkerPath)
$InstallWorkerPath = if ($InstallWorkerPath) { [IO.Path]::GetFullPath($InstallWorkerPath) } else { '' }
$MayaRoot = [IO.Path]::GetFullPath($MayaRoot)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $WorkerPath -PathType Leaf)) { throw "Worker executable missing: $WorkerPath" }
if ($InstallWorkerPath -and -not (Test-Path -LiteralPath $InstallWorkerPath -PathType Leaf)) { throw "Install test executable missing: $InstallWorkerPath" }
$venv = Join-Path $MayaRoot '.venv'
$pythonConfig = Get-Content -LiteralPath (Join-Path $venv 'pyvenv.cfg')
$homeLine = $pythonConfig | Where-Object { $_ -match '^home\s*=' } | Select-Object -First 1
$pythonMatch = [regex]::Match([string]$homeLine, '^home\s*=\s*(.+)$')
if (-not $pythonMatch.Success) { throw 'Could not resolve Maya Python for the isolated update check.' }
$python = Join-Path $pythonMatch.Groups[1].Value.Trim() 'python.exe'
if (-not (Test-Path -LiteralPath $python -PathType Leaf)) { throw "Python executable missing: $python" }

$fixtureRoot = Join-Path $OutputDirectory ('worker-update-' + [guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $fixtureRoot) { throw "Fixture path already exists: $fixtureRoot" }
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
$serverPath = Join-Path $fixtureRoot 'mock_update_server.py'
$serverCode = @(
'import json'
'import sys'
'from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer'
'from pathlib import Path'
''
'mode, log_path = sys.argv[1], Path(sys.argv[2])'
''
'def record(value):'
'    with log_path.open("a", encoding="utf-8") as output:'
'        output.write(json.dumps(value) + "\n")'
''
'class Handler(BaseHTTPRequestHandler):'
'    def reply(self, status, payload):'
'        body = json.dumps(payload).encode("utf-8")'
'        self.send_response(status)'
'        self.send_header("Content-Type", "application/json")'
'        self.send_header("Content-Length", str(len(body)))'
'        self.end_headers()'
'        self.wfile.write(body)'
''
'    def do_GET(self):'
'        if self.path == "/fixture-health":'
'            return self.reply(200, {"ready": True})'
'        record({"method": "GET", "path": self.path})'
'        if self.path == "/health":'
'            if mode == "null-model":'
'                return self.reply(200, {"model": None, "version": None, "unloaded": None, "images": None, "max_context": None})'
'            if mode == "null-fields":'
'                return self.reply(200, {"model": "fixture-model", "version": None, "unloaded": False, "images": None, "max_context": None})'
'            return self.reply(200, {"model": "native-worker-update-fixture", "version": "1.1.56"})'
'        if self.path == "/api/update?force=1" and mode == "null-error":
            return self.reply(500, {"error": {"message": None}})
        if self.path == "/api/update?force=1":'
'            can_update = mode == "ready"'
'            return self.reply(200, {'
'                "enabled": True,'
'                "current": "1.1.56",'
'                "latest": {"version": "1.1.57", "tag": "v1.1.57", "summary": "Isolated loopback fixture"},'
'                "newer": True,'
'                "error": None,'
'                "can_update": None if mode == "null-can-update" else can_update,'
'                "blocker": None if can_update or mode == "null-can-update" else "Fixture preflight blocker"'
'            })'
'        return self.reply(404, {"error": {"message": "unexpected path"}})'
''
'    def do_POST(self):'
'        size = int(self.headers.get("Content-Length", "0"))'
'        body = self.rfile.read(size).decode("utf-8", "replace")'
'        record({"method": "POST", "path": self.path, "body": body})'
'        if self.path != "/api/update" or mode != "ready":'
'            return self.reply(403, {"error": {"message": "unexpected update POST"}})'
'        return self.reply(202, {"update": {"state": "accepted", "version": "1.1.57"}})'
''
'    def log_message(self, fmt, *args):'
'        pass'
''
'server = ThreadingHTTPServer(("127.0.0.1", int(sys.argv[3])), Handler)'
'server.serve_forever()'
) -join [Environment]::NewLine
[IO.File]::WriteAllText($serverPath, $serverCode, [Text.UTF8Encoding]::new($false))

function New-WorkerFixtureRoot([string]$name, [int]$port) {
    $root = Join-Path $fixtureRoot $name
    if (Test-Path -LiteralPath $root) { throw "Fixture root already exists: $root" }
    New-Item -ItemType Directory -Path $root | Out-Null
    New-Item -ItemType Junction -Path (Join-Path $root '.venv') -Target $venv | Out-Null
    $profile = Join-Path $root 'maya-ds4.json'
    $config = @{
        host = '127.0.0.1'
        port = $port
        model_name = 'Native Worker update fixture'
        api_key = ''
        exe = 'maya-ds4.exe'
        args = @('--pack')
        installer = @{ family = 'deepseek' }
    } | ConvertTo-Json -Depth 5 -Compress
    [IO.File]::WriteAllText($profile, $config, [Text.UTF8Encoding]::new($false))
    $mayaScript = @(
        'import sys'
        'if "--check" not in sys.argv:'
        '    raise SystemExit("unexpected fixture invocation")'
        'print("This PC can run Maya.")'
    ) -join [Environment]::NewLine
    [IO.File]::WriteAllText((Join-Path $root 'maya.py'), $mayaScript, [Text.UTF8Encoding]::new($false))
    [pscustomobject]@{ Root = $root; Profile = $profile }
}

function Start-MockServer([string]$mode) {
    $probe = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
    $probe.Start()
    $port = [int]$probe.LocalEndpoint.Port
    $probe.Stop()
    if ($port -in @(8080, 8082)) { throw "Unexpected production Maya port selected: $port" }
    $log = Join-Path $fixtureRoot ($mode + '-requests.jsonl')
    $stdout = Join-Path $fixtureRoot ($mode + '-server.stdout.log')
    $stderr = Join-Path $fixtureRoot ($mode + '-server.stderr.log')
    foreach ($path in @($log, $stdout, $stderr)) {
        if (Test-Path -LiteralPath $path) { throw "Refusing to overwrite fixture output: $path" }
    }
    $arguments = '"{0}" "{1}" "{2}" {3}' -f $serverPath, $mode, $log, $port
    $process = Start-Process -FilePath $python -ArgumentList $arguments -WorkingDirectory $fixtureRoot `
        -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(15)
        do {
            try {
                $ready = Invoke-RestMethod -Uri "http://127.0.0.1:$port/fixture-health" -TimeoutSec 1
                if ($ready.ready) { return [pscustomobject]@{ Process = $process; Port = $port; Log = $log } }
            } catch {}
            if ($process.HasExited) { throw "The $mode loopback update server exited during startup." }
            Start-Sleep -Milliseconds 200
        } while ([DateTime]::UtcNow -lt $deadline)
        throw "The $mode loopback update server did not become ready."
    }
    catch {
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit(5000) | Out-Null }
        throw
    }
}

function Stop-MockServer($server) {
    if ($server -and -not $server.Process.HasExited) {
        $server.Process.Kill()
        if (-not $server.Process.WaitForExit(5000)) { throw 'The fixture-owned update server did not stop.' }
    }
}

function Invoke-Worker([string]$action, $fixture, [string]$executable) {
    if ([string]::IsNullOrWhiteSpace($executable)) { $executable = $WorkerPath }
    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $executable
    $start.WorkingDirectory = $fixture.Root
    $start.Arguments = '"{0}" "{1}" "{2}"' -f $action, $fixture.Root, $fixture.Profile
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::Start($start)
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(90000)) {
        $process.Kill()
        $process.WaitForExit()
        throw "Worker action '$action' timed out."
    }
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    try { $result = $stdout | ConvertFrom-Json -ErrorAction Stop }
    catch { throw "Worker action '$action' returned invalid JSON: $stdout $stderr" }
    [pscustomobject]@{ ExitCode = $process.ExitCode; Result = $result; Stdout = $stdout; Stderr = $stderr }
}

function Read-Requests([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return @() }
    @((Get-Content -LiteralPath $path) | ForEach-Object { $_ | ConvertFrom-Json })
}

$oldNoBytecode = $env:PYTHONDONTWRITEBYTECODE
$env:PYTHONDONTWRITEBYTECODE = '1'
$servers = [System.Collections.Generic.List[object]]::new()
try {
    $readyServer = Start-MockServer 'ready'
    $servers.Add($readyServer)
    $readyFixture = New-WorkerFixtureRoot 'ready' $readyServer.Port
    $check = Invoke-Worker 'update-check' $readyFixture
    if ($check.ExitCode -ne 0 -or -not $check.Result.ready -or -not $check.Result.dependency_ok -or
        $check.Result.update.latest.tag -ne 'v1.1.57') {
        throw "Online update-check did not pass its isolated preflight: $($check.Stdout) $($check.Stderr)"
    }
    $start = Invoke-Worker 'update-start' $readyFixture
    if ($start.ExitCode -ne 0 -or $start.Result.update.state -ne 'accepted') {
        throw "Online update-start did not return the mock acceptance: $($start.Stdout) $($start.Stderr)"
    }
    $readyRequests = Read-Requests $readyServer.Log
    $expectedReady = @(
        'GET /health', 'GET /api/update?force=1',
        'GET /health', 'GET /api/update?force=1', 'POST /api/update {}'
    )
    $actualReady = @($readyRequests | ForEach-Object {
        if ($_.method -eq 'POST') { $_.method + ' ' + $_.path + ' ' + $_.body }
        else { $_.method + ' ' + $_.path }
    })
    if (($actualReady -join "`n") -ne ($expectedReady -join "`n")) {
        throw "Ready update route sequence differed. Actual: $($actualReady -join ', ')"
    }
    Stop-MockServer $readyServer
    Write-Output 'PASS Worker update-check and update-start: loopback preflight and one gated POST.'

    $blockedServer = Start-MockServer 'blocked'
    $servers.Add($blockedServer)
    $blockedFixture = New-WorkerFixtureRoot 'blocked' $blockedServer.Port
    $blocked = Invoke-Worker 'update-start' $blockedFixture
    $blockedRequests = Read-Requests $blockedServer.Log
    $expectedBlocked = @('GET /health', 'GET /api/update?force=1')
    $actualBlocked = @($blockedRequests | ForEach-Object { $_.method + ' ' + $_.path })
    if ($blocked.ExitCode -eq 0 -or
        $blocked.Result.status -notlike '*Fixture preflight blocker*' -or
        ($actualBlocked -join "`n") -ne ($expectedBlocked -join "`n")) {
        throw "Blocked update-start did not fail closed before POST: $($blocked.Stdout) requests=$($actualBlocked -join ', ')"
    }
    Stop-MockServer $blockedServer
    Write-Output 'PASS Worker blocked update-start: preflight failure sent no POST.'
    $nullCanUpdateServer = Start-MockServer 'null-can-update'
    $servers.Add($nullCanUpdateServer)
    $nullCanUpdateFixture = New-WorkerFixtureRoot 'null-can-update' $nullCanUpdateServer.Port
    $nullCanUpdate = Invoke-Worker 'update-start' $nullCanUpdateFixture
    $nullCanUpdatePaths = @(Read-Requests $nullCanUpdateServer.Log | ForEach-Object { $_.method + ' ' + $_.path }) -join ','
    if ($nullCanUpdate.ExitCode -eq 0 -or
        [string]::IsNullOrWhiteSpace([string]$nullCanUpdate.Result.status) -or
        $nullCanUpdatePaths -ne 'GET /health,GET /api/update?force=1') {
        throw "Null can_update did not fail closed before POST: $($nullCanUpdate.Stdout) requests=$nullCanUpdatePaths"
    }
    Stop-MockServer $nullCanUpdateServer
    Write-Output 'PASS Worker null can_update: blocked before POST.'

    $nullErrorServer = Start-MockServer 'null-error'
    $servers.Add($nullErrorServer)
    $nullErrorFixture = New-WorkerFixtureRoot 'null-error' $nullErrorServer.Port
    $nullError = Invoke-Worker 'update-check' $nullErrorFixture
    $nullErrorPaths = @(Read-Requests $nullErrorServer.Log | ForEach-Object { $_.method + ' ' + $_.path }) -join ','
    if ($nullError.ExitCode -eq 0 -or
        $nullError.Result.status -ne "Maya rejected the update request." -or
        $nullErrorPaths -ne 'GET /health,GET /api/update?force=1') {
        throw "Null error message did not return the fallback error: $($nullError.Stdout) requests=$nullErrorPaths"
    }
    Stop-MockServer $nullErrorServer
    Write-Output 'PASS Worker null error message: returned fallback without a JSON type exception.'

    $nullModelServer = Start-MockServer 'null-model'
    $servers.Add($nullModelServer)
    $nullModelFixture = New-WorkerFixtureRoot 'null-model' $nullModelServer.Port
    [IO.File]::WriteAllText((Join-Path $nullModelFixture.Root 'VERSION'), '1.1.61', [Text.UTF8Encoding]::new($false))
    $nullModel = Invoke-Worker 'status' $nullModelFixture
    if ($nullModel.ExitCode -ne 0 -or $nullModel.Result.online -ne $false -or
        $nullModel.Result.status -ne 'Maya is not responding') {
        throw "Null health model was not handled as offline: $($nullModel.Stdout) $($nullModel.Stderr)"
    }
    Stop-MockServer $nullModelServer
    Write-Output 'PASS Worker null health model: status remained readable without a JSON type exception.'

    $nullFieldsServer = Start-MockServer 'null-fields'
    $servers.Add($nullFieldsServer)
    $nullFieldsFixture = New-WorkerFixtureRoot 'null-fields' $nullFieldsServer.Port
    $nullFields = Invoke-Worker 'status' $nullFieldsFixture
    if ($nullFields.ExitCode -ne 0 -or $nullFields.Result.online -ne $true -or
        $nullFields.Result.model -ne 'Running: fixture-model' -or $nullFields.Result.version -ne 'unknown' -or
        $nullFields.Result.vision_on -ne $false -or $nullFields.Result.detail -notmatch 'Vision OFF \| 0K Context') {
        throw "Null optional health fields were not handled with safe defaults: $($nullFields.Stdout) $($nullFields.Stderr)"
    }
    Stop-MockServer $nullFieldsServer
    Write-Output 'PASS Worker null optional health fields: safe defaults used without a JSON type exception.'

    $freshReadyRoot = Join-Path $fixtureRoot 'fresh-install-ready'
    if (Test-Path -LiteralPath $freshReadyRoot) { throw "Fresh-install fixture root already exists: $freshReadyRoot" }
    New-Item -ItemType Directory -Path $freshReadyRoot | Out-Null
    $freshReadyProfile = Join-Path $freshReadyRoot 'maya-ds4.json'
    $freshReadyMayaScript = @(
        'print("This PC can run Maya.")'
    ) -join [Environment]::NewLine
    [IO.File]::WriteAllText((Join-Path $freshReadyRoot 'maya.py'), $freshReadyMayaScript, [Text.UTF8Encoding]::new($false))
    $freshReadyMarker = Join-Path $freshReadyRoot 'installer-launched.marker'
    $freshReadyInstaller = @(
        '@echo off'
        ('echo launched>"' + $freshReadyMarker + '"')
        'exit /b 0'
    ) -join [Environment]::NewLine
    [IO.File]::WriteAllText((Join-Path $freshReadyRoot 'START-MAYA.bat'), $freshReadyInstaller, [Text.Encoding]::ASCII)
    $freshReadyFixture = [pscustomobject]@{ Root = $freshReadyRoot; Profile = $freshReadyProfile }
    $previousPath = $env:PATH
    try {
        $env:PATH = (Split-Path $python -Parent) + [IO.Path]::PathSeparator + $previousPath
        $freshReady = Invoke-Worker 'update-check' $freshReadyFixture
    }
    finally {
        $env:PATH = $previousPath
    }
    if ($freshReady.ExitCode -ne 0 -or -not $freshReady.Result.ready -or -not $freshReady.Result.dependency_ok -or
        $freshReady.Result.mode -ne 'fresh' -or $freshReady.Result.installed -or
        (Test-Path -LiteralPath $freshReadyMarker)) {
        throw "Fresh-install preflight did not report ready without launching setup: $($freshReady.Stdout) $($freshReady.Stderr)"
    }
    Write-Output 'PASS Worker fresh-install check: ready preflight does not launch START-MAYA.bat.'

    if ($InstallWorkerPath) {
        $previousPath = $env:PATH
        try {
            $env:PATH = (Split-Path $python -Parent) + [IO.Path]::PathSeparator + $previousPath
            $freshLaunch = Invoke-Worker 'install-maya' $freshReadyFixture $InstallWorkerPath
        }
        finally {
            $env:PATH = $previousPath
        }
        $launchRecord = Join-Path $freshReadyRoot 'worker-install-launch.record'
        if ($freshLaunch.ExitCode -ne 0 -or -not $freshLaunch.Result.ready -or -not $freshLaunch.Result.dependency_ok -or
            $freshLaunch.Result.status -notmatch '^Maya setup opened\.' -or
            (Test-Path -LiteralPath $freshReadyMarker) -or -not (Test-Path -LiteralPath $launchRecord -PathType Leaf)) {
            throw "Ready fresh-install handoff failed or launched the real setup: $($freshLaunch.Stdout) $($freshLaunch.Stderr)"
        }
        $record = [Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($launchRecord)).TrimEnd([char[]]@([char]13, [char]10))
        $expectedRecord = @('open', (Join-Path $freshReadyRoot 'START-MAYA.bat'), '<null>', $freshReadyRoot, '1') -join "`r`n"
        if ($record -ne $expectedRecord) { throw "Fresh-install launcher received unexpected arguments: $record" }
        Write-Output 'PASS Worker successful fresh-install handoff: exact setup path and working directory captured; no real setup launched.'
    }

    $freshRoot = Join-Path $fixtureRoot 'fresh-install-blocked'
    if (Test-Path -LiteralPath $freshRoot) { throw "Fresh-install fixture root already exists: $freshRoot" }
    New-Item -ItemType Directory -Path $freshRoot | Out-Null
    $freshProfile = Join-Path $freshRoot 'maya-ds4.json'
    $freshMayaScript = @(
        'import sys'
        'print("Synthetic dependency failure")'
        'raise SystemExit(42)'
    ) -join [Environment]::NewLine
    [IO.File]::WriteAllText((Join-Path $freshRoot 'maya.py'), $freshMayaScript, [Text.UTF8Encoding]::new($false))
    $installerMarker = Join-Path $freshRoot 'installer-launched.marker'
    $installerScript = @(
        '@echo off'
        ('echo launched>"' + $installerMarker + '"')
        'exit /b 0'
    ) -join [Environment]::NewLine
    [IO.File]::WriteAllText((Join-Path $freshRoot 'START-MAYA.bat'), $installerScript, [Text.Encoding]::ASCII)
    $freshFixture = [pscustomobject]@{ Root = $freshRoot; Profile = $freshProfile }
    $freshInstall = Invoke-Worker 'install-maya' $freshFixture
    if ($freshInstall.ExitCode -eq 0 -or $freshInstall.Result.ready -or $freshInstall.Result.dependency_ok -or
        $freshInstall.Result.status -notmatch 'Fresh-install preflight is blocked' -or
        (Test-Path -LiteralPath $installerMarker)) {
        throw "Fresh-install preflight did not block setup: $($freshInstall.Stdout) $($freshInstall.Stderr)"
    }
    Write-Output 'PASS Worker fresh install: blocked prerequisites do not launch START-MAYA.bat.'

    $launcherRoot = Join-Path $fixtureRoot 'start-maya-python-fallback'
    $launcherLocal = Join-Path $launcherRoot 'localappdata'
    $launcherPath = Join-Path $launcherRoot 'path'
    $launcherWorker = Join-Path $launcherRoot 'worker'
    New-Item -ItemType Directory -Path $launcherRoot,$launcherLocal,$launcherPath,$launcherWorker | Out-Null
    $launcherPythonRoot = Join-Path $launcherLocal 'Programs\Python'
    $invalidPython = Join-Path $launcherPythonRoot 'Python313'
    New-Item -ItemType Directory -Path $launcherPythonRoot,$invalidPython | Out-Null
    $where = Join-Path $env:SystemRoot 'System32\where.exe'
    Copy-Item -LiteralPath $where -Destination (Join-Path $invalidPython 'python.exe')
    Copy-Item -LiteralPath $where -Destination (Join-Path $launcherPath 'py.exe')
    Copy-Item -LiteralPath $where -Destination (Join-Path $launcherPath 'python.exe')
    $validPython = Join-Path $launcherPythonRoot 'Python311'
    New-Item -ItemType Junction -Path $validPython -Target (Split-Path $python -Parent) | Out-Null
    $launcherMaya = @(
        'import sys'
        'if "--check" in sys.argv:'
        '    print("This PC can run Maya.")'
        'else:'
        '    print("Launcher fixture reached Maya entry point.")'
    ) -join [Environment]::NewLine
    [IO.File]::WriteAllText((Join-Path $launcherRoot 'maya.py'), $launcherMaya, [Text.UTF8Encoding]::new($false))
    Copy-Item -LiteralPath (Join-Path $MayaRoot 'START-MAYA.bat') -Destination (Join-Path $launcherRoot 'START-MAYA.bat')
    $probeWorker = Join-Path $launcherWorker 'MayaManagerWorker.exe'
    Copy-Item -LiteralPath $WorkerPath -Destination $probeWorker
    Copy-Item -LiteralPath $where -Destination (Join-Path $launcherWorker 'py.exe')
    Copy-Item -LiteralPath $where -Destination (Join-Path $launcherWorker 'python.exe')
    $launcherFixture = [pscustomobject]@{ Root = $launcherRoot; Profile = (Join-Path $launcherRoot 'maya-ds4.json') }
    $oldLocalAppData = $env:LOCALAPPDATA
    $oldPath = $env:PATH
    try {
        $env:LOCALAPPDATA = $launcherLocal
        $env:PATH = $launcherPath + [IO.Path]::PathSeparator + (Join-Path $env:SystemRoot 'System32')
        $launcherCheck = Invoke-Worker 'update-check' $launcherFixture $probeWorker
        $expectedPython = Join-Path $validPython 'python.exe'
        if ($launcherCheck.ExitCode -ne 0 -or -not $launcherCheck.Result.ready -or
            $launcherCheck.Result.dependency_report -notmatch [regex]::Escape($expectedPython)) {
            throw "Worker did not select the valid lower-version fallback: $($launcherCheck.Stdout) $($launcherCheck.Stderr)"
        }
        Push-Location $launcherRoot
        try {
            $launcherOutput = & $env:ComSpec /d /c 'call START-MAYA.bat < NUL' 2>&1
            $launcherExit = $LASTEXITCODE
        }
        finally { Pop-Location }
        if ($launcherExit -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $launcherRoot '.venv\Scripts\python.exe')) -or
            ($launcherOutput -join "`n") -notmatch 'Launcher fixture reached Maya entry point\.') {
            throw "START-MAYA.bat did not skip the invalid Python 3.13 and use the valid fallback: $($launcherOutput -join ' ')"
        }
    }
    finally {
        $env:LOCALAPPDATA = $oldLocalAppData
        $env:PATH = $oldPath
    }
    Write-Output 'PASS START-MAYA Python fallback: invalid newest interpreter skipped; Worker preflight and launcher use the valid fallback.'

    $report = [pscustomobject]@{
        passed = $true
        scope = 'MayaManagerWorker update gates and START-MAYA fresh-install Python fallback'
        production_maya_contacted = $false
        ready_requests = $actualReady
        blocked_requests = $actualBlocked
        accepted_version = $start.Result.update.version
        blocked_exit_code = $blocked.ExitCode
        fresh_install_ready = $true
        fresh_install_ready_exit_code = $freshReady.ExitCode
        fresh_ready_installer_launched = $false
        fresh_install_blocked = $true
        fresh_install_exit_code = $freshInstall.ExitCode
        fresh_installer_launched = $false
        launcher_invalid_newest_python_skipped = $true
        launcher_valid_fallback_used = $true
    } | ConvertTo-Json -Depth 6
    $reportPath = Join-Path $fixtureRoot 'result.json'
    if (Test-Path -LiteralPath $reportPath) { throw "Refusing to overwrite fixture report: $reportPath" }
    [IO.File]::WriteAllText($reportPath, $report, [Text.UTF8Encoding]::new($false))
    Write-Output "Worker update fixture passed; no production Maya request was made. Report: $reportPath"
}
finally {
    foreach ($server in $servers) { Stop-MockServer $server }
    if ($null -eq $oldNoBytecode) { Remove-Item Env:PYTHONDONTWRITEBYTECODE -ErrorAction SilentlyContinue }
    else { $env:PYTHONDONTWRITEBYTECODE = $oldNoBytecode }
}
