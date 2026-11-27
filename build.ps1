param(
    [string]$VcVars64Path = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$mayaRoot = Split-Path -Parent $root
$source = Join-Path $root 'src'
$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()

if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "Invalid VERSION value: $version" }
if (!(Test-Path -LiteralPath (Join-Path $mayaRoot 'maya.py') -PathType Leaf)) {
    throw 'Place WinManager directly under the Maya-XS folder containing maya.py.'
}
if (!(Test-Path -LiteralPath $VcVars64Path -PathType Leaf)) { throw "Visual Studio x64 setup not found: $VcVars64Path" }

$parts = $version.Split('.')
$fileVersion = "$($parts[0]),$($parts[1]),$($parts[2]),0"
$manifestVersion = "$($parts[0]).$($parts[1]).$($parts[2]).0"
$stamp = (Get-Date).ToString('yyyyMMdd-HHmmss-fff') + '-' + [guid]::NewGuid().ToString('N').Substring(0,6)
$build = Join-Path $root ('.build-' + $stamp)
$release = Join-Path $root ('v' + $version)
New-Item -ItemType Directory -Path $build | Out-Null
New-Item -ItemType Directory -Path (Join-Path $build 'obj-app'),(Join-Path $build 'obj-worker'),(Join-Path $build 'obj-icon') | Out-Null

$manifest = @"
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="$manifestVersion" processorArchitecture="amd64" name="DanielMarius.MayaWinManager" type="win32" />
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0" processorArchitecture="amd64" publicKeyToken="6595b64144ccf1df" language="*" />
    </dependentAssembly>
  </dependency>
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security><requestedPrivileges><requestedExecutionLevel level="requireAdministrator" uiAccess="false" /></requestedPrivileges></security>
  </trustInfo>
</assembly>
"@
$manifestPath = Join-Path $build 'MayaNative.manifest'
[IO.File]::WriteAllText($manifestPath, $manifest, [Text.Encoding]::UTF8)

$versionBlock = @"
1 VERSIONINFO
FILEVERSION $fileVersion
PRODUCTVERSION $fileVersion
FILEFLAGSMASK 0x3fL
FILEFLAGS 0x0L
FILEOS 0x00040004L
FILETYPE 0x00000001L
FILESUBTYPE 0x00000000L
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "CompanyName", "DanielMarius\0"
      VALUE "FileVersion", "$version\0"
      VALUE "ProductName", "Maya Win-Manager\0"
      VALUE "ProductVersion", "$version\0"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
"@
$nativeRc = @"
101 ICON "$($build.Replace('\','/'))/Maya.ico"
102 ICON "$($build.Replace('\','/'))/DeepSeek.ico"
1 24 "$($manifestPath.Replace('\','/'))"
$versionBlock
"@
$workerRc = @"
$versionBlock
"@
$nativeRcPath = Join-Path $build 'MayaNative.rc'
$workerRcPath = Join-Path $build 'MayaManagerWorker.rc'
[IO.File]::WriteAllText($nativeRcPath, $nativeRc, [Text.Encoding]::ASCII)
[IO.File]::WriteAllText($workerRcPath, $workerRc, [Text.Encoding]::ASCII)

$cmd = @"
@echo off
call "$VcVars64Path" >nul
if errorlevel 1 exit /b 1
set "CL=/DNOMINMAX"
cd /d "$root"
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W3 /DUNICODE /D_UNICODE /DNOMINMAX /Fo"$build\obj-icon\\" src\icon_builder.cpp /Fe"$build\IconBuilder.exe"
if errorlevel 1 exit /b 1
"$build\IconBuilder.exe" "$build\Maya.ico" "$source\assets\maya-icon-192.png"
if errorlevel 1 exit /b 1
"$build\IconBuilder.exe" "$build\DeepSeek.ico" "$source\assets\deepseek-icon.png"
if errorlevel 1 exit /b 1
rc /nologo /fo "$build\MayaNative.res" "$nativeRcPath"
if errorlevel 1 exit /b 1
rc /nologo /fo "$build\MayaManagerWorker.res" "$workerRcPath"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W3 /DUNICODE /D_UNICODE /DNOMINMAX /Fo"$build\obj-app\\" "$source\native\MayaNative.cpp" /Fe"$build\MayaNative.exe" /link "$build\MayaNative.res" /MANIFEST:NO /SUBSYSTEM:WINDOWS /DELAYLOAD:gdiplus.dll delayimp.lib user32.lib gdi32.lib gdiplus.lib comctl32.lib shell32.lib ole32.lib oleaut32.lib advapi32.lib winhttp.lib iphlpapi.lib wbemuuid.lib crypt32.lib comdlg32.lib dwmapi.lib urlmon.lib uxtheme.lib imm32.lib oleacc.lib mfplay.lib mfplat.lib mfuuid.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W3 /DUNICODE /D_UNICODE /DNOMINMAX /Fo"$build\obj-worker\\" "$source\worker.cpp" "$source\runtime.cpp" /Fe"$build\MayaManagerWorker.exe" /link "$build\MayaManagerWorker.res" user32.lib shell32.lib ole32.lib oleaut32.lib wbemuuid.lib iphlpapi.lib winhttp.lib advapi32.lib
if errorlevel 1 exit /b 1
dumpbin /headers "$build\MayaNative.exe" | findstr /c:"8664 machine (x64)" >nul
if errorlevel 1 (echo MayaNative.exe is not x64.& exit /b 1)
dumpbin /headers "$build\MayaManagerWorker.exe" | findstr /c:"8664 machine (x64)" >nul
if errorlevel 1 (echo MayaManagerWorker.exe is not x64.& exit /b 1)
dumpbin /dependents "$build\MayaNative.exe" | findstr /i /c:"WebView2" /c:"msedgewebview2" /c:"chrome" /c:"cef" /c:"mshtml" /c:"ieframe" /c:"shdocvw" /c:"ieproxy" >nul
if not errorlevel 1 (echo MayaNative.exe imports a browser runtime.& exit /b 1)
dumpbin /dependents "$build\MayaManagerWorker.exe" | findstr /i /c:"WebView2" /c:"msedgewebview2" /c:"chrome" /c:"cef" /c:"mshtml" /c:"ieframe" /c:"shdocvw" /c:"ieproxy" >nul
if not errorlevel 1 (echo MayaManagerWorker.exe imports a browser runtime.& exit /b 1)
echo PE checks passed: both outputs are x64 and have no browser-runtime imports.
"@

$cmdPath = Join-Path $build 'compile.cmd'
$stdout = Join-Path $build 'compile.stdout.log'
$stderr = Join-Path $build 'compile.stderr.log'
[IO.File]::WriteAllText($cmdPath, $cmd, [Text.Encoding]::ASCII)
$process = Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c',('"' + $cmdPath + '"')) -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $stdout -RedirectStandardError $stderr

Get-Content -LiteralPath $stdout
Get-Content -LiteralPath $stderr
if ($process.ExitCode -ne 0) { throw "Native build failed. Scratch files retained at $build" }

New-Item -ItemType Directory -Path $release -Force | Out-Null
$outputs = @('MayaNative.exe','MayaManagerWorker.exe')
$existing = @($outputs | ForEach-Object { Join-Path $release $_ } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
if ($existing.Count) {
    $backupRoot = Join-Path $mayaRoot '_codex-backups'
    if (!(Test-Path -LiteralPath $backupRoot -PathType Container)) {
        throw "Refusing to replace existing executables without the existing backup root: $backupRoot"
    }
    $backup = Join-Path $backupRoot ('WinManager-build-' + $stamp)
    if (Test-Path -LiteralPath $backup) { throw "Backup already exists: $backup" }
    New-Item -ItemType Directory -Path $backup | Out-Null
    foreach ($path in $existing) {
        $dest = Join-Path $backup (Split-Path $path -Leaf)
        Copy-Item -LiteralPath $path -Destination $dest
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash) {
            throw "Executable backup verification failed: $path"
        }
    }
    Write-Output "Verified replacement backup: $backup"
}

foreach ($name in $outputs) {
    $candidate = Join-Path $build $name
    $target = Join-Path $release $name
    Copy-Item -LiteralPath $candidate -Destination $target -Force
    if ((Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash) {
        throw "Output verification failed: $target"
    }
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($target)
    if ($info.FileVersion -ne $version -or $info.ProductVersion -ne $version -or $info.ProductName -ne 'Maya Win-Manager') {
        throw "Version resource verification failed: $target"
    }
    Write-Output "$name | $($info.FileVersion) | $((Get-Item -LiteralPath $target).Length) bytes"
}

Remove-Item -LiteralPath $build -Recurse -Force
Write-Output "Build complete: Maya Win-Manager v$version (x64). Tests were not run."
