# Distribution test: the package on a clean Windows

The decisive check of a release candidate (M70): the ZIP, on a Windows
that has never built AtomEngine. It proves that the package carries what
it needs and nothing assumes the development machine: no Visual C++
Redistributable, no SDL, no repository, no environment variables.

The machine is a **VirtualBox VM**. VirtualBox's virtual GPU has no
Direct3D 12, so the VM tests everything up to the GPU: that the program
starts, its DLLs, its paths, its log and settings, and its failure path,
which must explain itself. **Rendering on a clean machine is not covered
by this test**; that needs a real PC with a D3D12 GPU (the same steps,
then play).

## Prepare the VM (once)

1. Install **VirtualBox** (virtualbox.org), and download the **Windows 11
   ISO** from microsoft.com (*Download Windows 11 → Disk image (ISO)*).
2. New VM: Windows 11 (64-bit), 4 GB memory, 2 CPUs, a 64 GB disk, EFI
   and TPM on (VirtualBox 7 offers them for Windows 11). Install
   Windows. It doesn't need activating, and use an offline/local
   account if offered.
3. Don't install anything else: no Visual Studio, no drivers beyond
   what Windows Update brings, no game runtimes.
4. Check it's clean, in PowerShell:
   ```powershell
   winget list --name "Visual C++"          # expect: no installed package
   Test-Path C:\Windows\System32\vcruntime140.dll   # usually False (some Windows images ship it; note it)
   ```
5. Take a snapshot named **clean**. Before every test, restore it.

## Run the test

1. On the development machine: `pwsh Tools/Dist/package.ps1`.
2. Copy `AtomGame-v<version>-win64.zip` into the VM (drag and drop, or a
   shared folder), and **extract** it to `C:\Games\` (right-click →
   Extract All).
3. **Double-click** `C:\Games\AtomGame\AtomGame.exe`.

## Expected

| Check | Expected |
|---|---|
| No missing-DLL dialog ("VCRUNTIME140.dll was not found", "SDL3.dll …") | ✔ none |
| No console window | ✔ only the game's window, or its message |
| The message box | "AtomGame could not start." with a GPU reason (no D3D12 in VirtualBox) and the log's path |
| The log, `%APPDATA%\AtomEngine\AtomGame\logs\AtomGame.log` | starts with `Executable folder: C:\Games\AtomGame\`, the log path, `Starting AtomEngine <version>`, the SDL versions, then the GPU attempts and why they failed |
| A diagnostics report: in a terminal in `C:\Games\AtomGame`, `.\AtomGame.exe --diagnostics $env:TEMP\report.txt` | if the device can't be created at all, the log says so and no report; note which |
| Nothing written into `C:\Games\AtomGame` | ✔ (`dir C:\Games\AtomGame` shows only the extracted files) |
| A second double-click | the log of the first run is now `AtomGame.previous.log` |

If VirtualBox does present a Direct3D 12 device (for example WARP, the
software rasterizer), note how far the game gets: the title, the first
level, frame rate.

## Record

In the release's CHANGELOG entry: the date, the package version, the VM
(VirtualBox version, Windows build), each row of the table as observed,
the first lines of the log, and anything unexpected, especially any
dependency that appeared.
