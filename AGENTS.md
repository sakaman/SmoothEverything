# Repository Guidelines

## Project Structure & Module Organization

`src/core` contains platform-light C++20 motion and JSON/configuration logic. `src/engine` implements the Win32 hook, worker, policy, IPC, and tray process; `src/control_panel` is the current native Win32 settings UI. The older `src/settings` WinUI/.NET project is not included by the root CMake build and should not be extended unless it is deliberately reactivated. Tests live in `tests/core`, `tests/engine`, and `tests/control_panel`, with targets declared in `tests/CMakeLists.txt`. Keep user-facing documentation in `docs`, brand assets in `assets`, installer sources in `installer`, and build/release automation in `scripts`. Everything under `artifacts/` is generated output.

## Build, Test, and Development Commands

Development requires Windows 10/11 x64, PowerShell 7, Scoop, CMake, Ninja, and the Scoop LLVM/UCRT toolchain.

```powershell
pwsh .\scripts\Setup-Toolchain.ps1
pwsh .\scripts\Build.ps1 -Configuration Debug
ctest --preset debug
pwsh .\scripts\Publish.ps1 -Version 0.1.3
```

`Setup-Toolchain.ps1` installs build prerequisites; add `-IncludeInstaller` for Inno Setup. `Build.ps1` configures, builds, and tests by default (`-SkipTests` is for focused iteration only). `ctest` reruns the configured suite with failures displayed. `Publish.ps1` performs a Release build and creates the installer, portable ZIP, and checksums.

## Coding Style & Naming Conventions

Follow the existing C++ style: four-space indentation, braces on the declaration line, `PascalCase` types/functions, `snake_case` files and local data, and trailing underscores for private members. Keep public headers under `include/smootheverything/...` and implementation files under `src`. Use English for documentation, source UI keys, comments, commits, and pull requests; add user-facing translations only through the control-panel localization table. Prefer RAII, explicit ownership, `const`, and the existing warning-clean C++20 patterns. PowerShell scripts use approved verbs, `$camelCase` variables, strict mode, and terminating errors. No formatter is enforced, so match adjacent code and run `git diff --check` before committing.

## Testing Guidelines

Tests use small self-contained C++ executables registered with CTest rather than an external framework. Name files `*_tests.cpp` and test functions after observable behavior, such as `SingleImpulseConservesDelta`. Add deterministic coverage for motion/config changes in core tests and Win32/IPC changes in engine component tests. There is no numeric coverage threshold; all configured Debug and Release tests must pass.

## Commit & Pull Request Guidelines

Recent history uses short imperative subjects without Conventional Commit prefixes, for example `Replace WinUI panel with native Win32 UI`. Keep each commit focused. Pull requests should explain intent, affected modules, validation commands, and user-visible or security boundaries. Link relevant issues and include control-panel screenshots for UI changes. Do not commit `artifacts/`; for releases, keep semantic versions synchronized and document generated assets.
