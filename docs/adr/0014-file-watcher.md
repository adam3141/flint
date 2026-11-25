# 0014. FileWatcher for hot reload

- Status: Accepted
- Date: 2026-11-25

## Context

The owner wants views and themes to reload while an application runs (agate milestone 4), on by default in debug builds. Watching files is not UI code, so it belongs in flint, behind an interface so that other platforms can be added (owner: cross-platform later).

## Decision

- `flint::platform::FileWatcher` watches individual files. `Poll()` returns the files that changed since the last call and never blocks, so a UI can poll from a timer on its own thread without a watcher thread.
- On Linux, `Create()` uses inotify on each file's directory (close-after-write, rename-to, create and delete), so saving by writing a new file and renaming it over the old one is seen.
- Everywhere else, and on request, `CreatePolling()` compares each file's modification time and size.

## Consequences

- No threads, and no callbacks arriving on another thread.
- Changes are seen at the polling rate the application chooses; agate polls a few times a second and debounces.
- Windows (`ReadDirectoryChangesW`) and macOS (FSEvents) implementations can replace the polling fallback later.
