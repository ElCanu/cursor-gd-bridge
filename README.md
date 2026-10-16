# cursor-gd-bridge

[![Download latest release](https://img.shields.io/badge/download-latest-2ea44f?style=flat-square)](https://github.com/ElCanu/cursor-gd-bridge/releases/latest)

Small Windows executable that speaks the **OpenAI** chat API on `127.0.0.1` and turns each request into a **Cursor Cloud Agent** (no repository: the diff lives in the prompt). GitHub Desktop can use it as a custom provider to generate commit messages.

This is not an official Cursor or GitHub endpoint. Latency comes from the agent (often several seconds, sometimes close to a minute), not from the binary.

The bridge stores **no** API keys and has **no** config file for secrets. Desktop sends the Cursor key, model id, and diff on every request; the exe only forwards them.

## Download (Windows x64)

1. Download **[cursor-gd-bridge.exe](https://github.com/ElCanu/cursor-gd-bridge/releases/latest/download/cursor-gd-bridge.exe)** (or open [Releases](https://github.com/ElCanu/cursor-gd-bridge/releases/latest) for notes).
2. Run it once (or add a shortcut to your Startup folder). No installer.

You may need the [Visual C++ Redistributable (x64)](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) if Windows reports a missing runtime.

## Requirements (build from source)

- Windows x64
- Visual Studio 2022 (MSVC) with the C++ workload
- CMake 3.24+

## Build

```powershell
cmake --preset windows-msvc
cmake --build --preset release
```

Output: `build\Release\cursor-gd-bridge.exe` (no console). Use `cmake --build --preset debug` for a console build and stderr logs.

Release errors are also appended to `%LOCALAPPDATA%\cursor-gd-bridge\bridge.log`.

A second launch exits immediately (mutex). In Release, a dialog says the bridge is already running.

## GitHub Desktop (all credentials here)

1. Start `cursor-gd-bridge.exe` (Startup folder shortcut is fine).
2. **Settings → Copilot → Providers** → add a provider, type **OpenAI**, API format **Chat completions**.

| Field | Value |
|--------|--------|
| Base URL | `http://127.0.0.1:18789/v1` |
| API key | your Cursor API key (`crsr_...` from Cursor Dashboard → API Keys) |
| Request timeout | `90` seconds (or higher) |

3. Under **Models**, add each model you want in the picker. The **id** must be the Cursor model id (`composer-2.5`, `grok-4.7`, `default` for Auto, etc.). The display name is only for Desktop; the bridge forwards the **id** unchanged.

Nothing else to paste into the repo or the executable.

## Run / health check

```powershell
curl.exe http://127.0.0.1:18789/health
```

Port `18789` is fixed in `src/config/defaults.hpp`. One Cursor agent run at a time. Internal deadline: 90 seconds.

## How it maps to Cursor

| From Desktop | Bridge |
|--------------|--------|
| `Authorization: Bearer <key>` | Same header to `api.cursor.com` |
| `model` in JSON body | `model.id` on `POST /v1/agents` |
| `messages` (diff) | Agent prompt (no repo attached) |
| Assistant `content` | Normalized to `{"title":"...","description":"..."}` (required by Desktop) |

## Manual test (optional)

Same headers Desktop uses (counts as one Cursor agent run):

```powershell
curl.exe -sS http://127.0.0.1:18789/v1/chat/completions `
  -H "Authorization: Bearer crsr_YOUR_KEY" `
  -H "Content-Type: application/json" `
  -d "{\"model\":\"composer-2.5\",\"messages\":[{\"role\":\"user\",\"content\":\"diff: add health endpoint\"}]}"
```

## Limits

- One Cloud Agent run per commit message (Cursor quota).
- No local Cursor agent, no tray icon, no installer.
- If Desktop sends `stream: true`, the SSE body is sent in one shot after the agent finishes.

## Publish a release (maintainers)

Pushing a version tag builds `cursor-gd-bridge.exe` on GitHub Actions and attaches it to a new release:

```powershell
git tag v0.1.0
git push origin v0.1.0
```

Use semantic tags (`v0.1.1`, `v1.0.0`, …). Every push to `main` also runs a compile-only CI job.
