# Architecture

## Process Model

SmoothEverything consists of two standard-user processes:

1. `SmoothEverything.Engine.exe`: a Win32/C++20 background engine responsible for the input hook, policy cache, motion sampling, input injection, notification-area controls, and per-user startup.
2. `SmoothEverything.ControlPanel.exe`: a native C++20/Win32 control panel that edits settings, displays diagnostics, and starts the colocated engine when needed.

This separation keeps the input hot path independent from the UI lifecycle while preserving a native Windows settings experience. Both processes run as `asInvoker`, and release packages do not require the .NET or Windows App SDK runtimes.

## Input Hot Path

The `WH_MOUSE_LL` callback performs only these bounded operations:

1. Ignore events injected by this engine or marked with `LLMHF_INJECTED` to prevent recursion.
2. Check the global toggle, modifiers, high-resolution pass-through, and horizontal-scrolling options.
3. Find the root window and PID under the pointer.
4. Read an immutable PID policy snapshot. On a cache miss, enqueue a resolution request and pass the current event through.
5. Place eligible events in a fixed-capacity SPSC queue. Pass through when the queue is full.

The callback does not read from disk, open target processes, parse JSON, or wait.

## Motion Worker

The motion worker runs at `THREAD_PRIORITY_ABOVE_NORMAL` and samples with a high-resolution waitable timer every 8 ms:

- Each physical wheel delta creates an independent impulse; same-direction impulses may overlap.
- Quintic smoothstep provides continuous first- and second-order boundaries, while `tail_to_head_ratio` remaps the time axis.
- Repeated input increases acceleration within the configured window up to `acceleration_max`.
- Direction, target window, settings generation, or injection failures cancel outstanding momentum.
- Fractional remainders are accumulated per frame so completed impulses conserve total displacement.

`SendInput` events carry a fixed `dwExtraInfo` marker. The low-level hook checks both this marker and `LLMHF_INJECTED`.

## Application Policy

Target policies are keyed by PID and contain the executable basename, settings generation, and expiration time:

- Path resolution runs on the motion thread and never blocks the hook.
- A target that cannot be opened is cached as pass-through.
- Exclusions take precedence over per-application profiles.
- Compatibility mode always passes input through.
- Settings updates advance the generation and invalidate all policies.
- The cache stores at most 128 PIDs and expires entries after 30 seconds by default.

## IPC and Configuration

The named pipe is `\\.\pipe\SmoothEverything.Engine.v1`:

- Its security descriptor grants access only to the current user SID.
- Remote clients are rejected.
- Each connection accepts one line of UTF-8 JSON up to 1 MiB.
- Supported operations are `get_state`, `set_enabled`, `apply_settings`, `open_settings`, and `shutdown`.
- Pipe I/O uses overlapped operations that can be cancelled by the stop event.

Settings are written to a temporary file in the same directory, flushed with `FlushFileBuffers`, and atomically replaced with `MoveFileEx`. Invalid data or an unknown schema never overwrites the original file; the engine continues with safe defaults and reports the error through diagnostics.

## Failure Policy

The guiding rule is: scrolling quality may degrade, but the wheel must not stop working.

| Condition | Behavior |
|---|---|
| Unknown PID or unreadable path | Pass through the event; resolve asynchronously or cache pass-through |
| Full SPSC queue | Pass through and increment `queue_overflows` |
| Pointer moves to another root window | Cancel remaining momentum |
| Partial or complete `SendInput` failure | Cancel the gesture and temporarily mark the target PID as pass-through |
| Corrupted configuration | Preserve the file, use defaults, and report the error |
| Control panel is closed | Continue running with current settings |

## Test Boundaries

Automated tests cover motion curves, displacement conservation, reverse-direction cancellation, JSON/schema handling, policy resolution, atomic settings writes, the SPSC queue, the named pipe, and worker lifecycle. Process smoke tests cover engine startup, state requests, and clean shutdown. UI runtime checks cover all four native pages and engine interaction.

These checks do not constitute production certification for code signing, installer behavior, every mouse driver, elevated targets, Remote Desktop, or the secure desktop.
