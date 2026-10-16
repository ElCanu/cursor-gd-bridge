# cursor-gd-bridge

Small Windows executable that speaks the **OpenAI** chat API on `127.0.0.1` and turns each request into a **Cursor Cloud Agent** (no repository: the diff lives in the prompt). GitHub Desktop can use it as a custom provider to generate commit messages.

This is not an official Cursor or GitHub endpoint. Latency comes from the agent (often several seconds, sometimes close to a minute), not from the binary.

## Requirements

- Windows x64
- Visual Studio 2022 (MSVC) with the C++ workload
- CMake 3.24+

## API key

The Cursor key is **not** compiled into the executable. Put it in the GitHub Desktop provider API key field. Desktop sends `Authorization: Bearer <key>` on each request, and the bridge forwards that header to Cursor.

The model id is also taken from the request. Add each model in Desktop with the Cursor id (`composer-2.5`, `grok-4.7`, …). The bridge forwards that id unchanged.

## Build

From `C:\GameServer\code\cursor-gd-bridge`:

```powershell
cmake --preset windows-msvc
cmake --build --preset debug
cmake --build --preset release
```

| Config | Output | Window |
|--------|--------|--------|
| Debug | `build\Debug\cursor-gd-bridge.exe` | console, traces on stderr |
| Release | `build\Release\cursor-gd-bridge.exe` | no console |

Release errors are appended to `%LOCALAPPDATA%\cursor-gd-bridge\bridge.log`.

A second launch exits immediately (mutex). In Release, a dialog says the bridge is already running.

## Run

Start the exe **before** generating a message in GitHub Desktop. A shortcut in the Windows Startup folder is enough if you want it at logon.

Health check:

```powershell
curl.exe http://127.0.0.1:18789/health
curl.exe http://127.0.0.1:18789/v1/models
```

Local trial (this really calls Cursor, so it counts as one agent run):

```powershell
curl.exe -sS http://127.0.0.1:18789/v1/chat/completions `
  -H "Authorization: Bearer crsr_YOUR_KEY" `
  -H "Content-Type: application/json" `
  -d "{\"model\":\"composer-2.5\",\"messages\":[{\"role\":\"user\",\"content\":\"diff: add health endpoint\"}]}"
```

The port is `18789` (`src/config/defaults.hpp`). One Cursor request at a time. Internal deadline: 90 seconds.

## GitHub Desktop

Settings → Copilot → Providers, type **OpenAI**:

| Field | Value |
|--------|--------|
| Base URL | `http://127.0.0.1:18789/v1` |
| API key | your Cursor key (`crsr_...`) |
| Model | `composer-2.5` (or another Cursor model id) |
| Request timeout | `90` seconds |

The id under the display name is what Cursor receives. `default` is not rewritten.

## Limits

- One Cloud Agent run per commit message (Cursor quota).
- No local agent, no tray icon, no installer.
- If Desktop sends `stream: true`, the SSE body is sent in one shot after the agent finishes.
- The assistant text is always a JSON object `{"title","description"}`. Desktop rejects anything else as invalid JSON.
