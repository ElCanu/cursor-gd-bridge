# cursor-gd-bridge

Small Windows executable that speaks the **OpenAI** chat API on `127.0.0.1` and turns each request into a **Cursor Cloud Agent** (no repository: the diff lives in the prompt). GitHub Desktop can use it as a custom provider to generate commit messages.

This is not an official Cursor or GitHub endpoint. Latency comes from the agent (often several seconds, sometimes close to a minute), not from the binary.

The bridge stores **no** API keys and has **no** config file for secrets. Desktop sends the Cursor key, model id, and diff on every request; the exe only forwards them.

## Requirements

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
