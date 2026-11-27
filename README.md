# Maya Win-Manager

**Version:** 0.0.1

**Maintainer:** DanielMarius

Maya Win-Manager is a native C++17, Win32 x64 desktop application for operating a Maya-XS installation on Windows. It recreates Maya’s management screens as native Windows UI and adds Windows controls. It does not embed Maya’s web app, WebView, Electron, or another browser runtime.

## Run

Keep MayaNative.exe and MayaManagerWorker.exe together in the v0.0.1 folder. Double-click v0.0.1\MayaNative.exe from this repository. Windows requests administrator approval because the app manifest requires elevation.

The Manager searches parent directories for the Maya-XS root markers maya.py and serve\web. It reads installed model profiles from that root. Keep the Worker beside the Manager executable; the Manager starts it as a hidden helper for engine lifecycle and update operations.

The Maya engine remains a separate process. Starting or closing the Manager does not by itself start or stop Maya.

## Build

Requirements:

- Windows x64
- Visual Studio 2022 C++ build tools and Windows SDK
- The Manager folder placed under the Maya-XS root

From PowerShell in this folder, run:

```powershell
.\build.ps1
```

The build compiles the native Manager and Worker with the static MSVC runtime, embeds the Maya and DeepSeek icons, embeds administrator and version metadata, checks that both outputs are x64, and writes the executables to the version folder named by VERSION. Compiler scratch is temporary. If replacing existing executables, the script first makes and verifies timestamped backups.

The one vendored build dependency is nlohmann/json 3.12.0, included under src/third_party/nlohmann with its MIT license. The executable uses Windows system libraries; it does not bundle Python, model weights, CUDA, a browser, or third-party DLLs.

## What it does

- Native Chat, Monitor, Models, Settings, Savings, Connect Your Tools, About, and update pages.
- Maya profile selection, server controls, model switching, status and update preflight.
- Streaming responses, reasoning display, image attachments, and request monitoring.
- Windows startup and compact-controller options.
- A companion Worker for Maya engine operations.

Maya’s own web app and its engine implementation remain in the Maya-XS repository. Manager source and binaries are maintained separately here.

## Local data and network behavior

- Chat history is stored under %LOCALAPPDATA%\MayaXS\WinManager\chats.
- An optional API-key override is saved in Windows Credential Manager under MayaXS.NativeManager.ApiKey; it is sent to the Maya host configured by the selected profile.
- Maya profiles and model files stay in Maya-XS and are not part of this repository.
- The Manager connects to the configured Maya endpoint and to GitHub when checking project updates. Additional destinations are used only when a user selects a feature that needs them.
- Enabling “Add when Windows starts” writes a per-user Windows Run entry. Turn the setting off before uninstalling to remove that entry; then close the Manager and remove this folder.

## Development branches

Work on dev. Do not push to main until the executable-signing path has been resolved and verified.

## Code signing policy

The v0.0.1 development binaries are currently **unsigned**. No signing key, certificate, or signing token belongs in this repository.

We are evaluating SignPath Foundation’s free open-source program. Its published conditions include an OSI-approved license, active maintenance, a published release, documented functionality, source-verifiable builds, MFA for repository and SignPath accounts, defined author/reviewer/approver roles, and manual approval for each signing request. Its GitHub trusted-build integration requires a GitHub-hosted Actions workflow. If accepted, the signing policy and publisher wording will be updated before a signed release. The Foundation’s certificate identifies SignPath Foundation as publisher, not DanielMarius.

References: [SignPath Foundation conditions](https://signpath.org/terms), [SignPath GitHub trusted-build documentation](https://docs.signpath.io/trusted-build-systems/github), and [SignPath signing workflow](https://docs.signpath.io/signing-code).
