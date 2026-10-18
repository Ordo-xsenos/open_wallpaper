# Core control protocol v1

Core alone owns SQLite, settings, scene uploads and tray state. Manager uses one
request/response per connection; it never opens Core's database or HWNDs. The
Windows endpoint is `\\.\pipe\AetherWall-Core-v1` (byte mode, local clients only).
The protected DACL grants access to the current logon's SID; the first-instance
flag refuses an existing server. Manager checks the server's logon SID and session
and connects with identification-level security QoS. Another local session must
not be accepted. The fixed endpoint currently permits one Core server per machine;
parallel interactive sessions fail safely rather than share state.

## Framing and envelope

Four unsigned little-endian length bytes, followed by that many UTF-8 JSON bytes.
Payload length is 1..262144 bytes. Reject malformed UTF-8, duplicate object keys,
nesting deeper than 24 and unknown fields. Arrays/objects are never executable.

```json
{"version":1,"requestId":"42","command":"applyScene","params":{"monitorId":"*","sceneId":"a-valid-installed-scene-uuid"}}
```

```json
{"version":1,"requestId":"42","ok":true,"result":{"monitorIds":["device-path"],"sceneId":"a-valid-installed-scene-uuid"}}
```

Errors use `ok:false,error:{code,message}`; codes include `invalid_request` and
`operation_failed`. An unparseable request has an empty response requestId.
Request IDs are bounded to 64 bytes, commands to 64, monitor selectors to 512.
The server disconnects malformed frame lengths before enqueueing a command.

## Commands

| Command | Params | Result / behavior |
| --- | --- | --- |
| ping | `{}` | Engine name and protocol version |
| getStatus | `{}` | Runtime mode, output count, data/log paths, settings recovery warning |
| getMonitors | `{}` | Logical monitors, physical bounds/DPI/Hz, pause state, persisted assignments |
| getSettings | `{}` | Complete schemaVersion 2 settings object |
| updateSettings | `{settings:{...}}` | Strict complete replacement; persist, update runtime; compensate on runtime rejection |
| listInstalledScenes | `{}` | Up to 128 managed scenes, metadata, favorite and local preview/folder paths |
| importScene | `{path:"absolute path"}` | Validate directory/.awscene and copy a bounded managed snapshot; return sceneId |
| importDownloadedScene | `{path,sceneId,sha256,size,source}` | Freeze bounded .awscene, recheck SHA-256/size/UUID, import and record receipt atomically; never apply |
| getDownloadHistory | `{}` | Latest 100 successful download receipts |
| applyScene | `{monitorId:"device-path or *",sceneId:"uuid"}` | Stage every selected GPU upload, commit SQLite assignments, then swap prepared states |
| setFavorite | `{sceneId:"uuid",favorite:true}` | Persist favorite |
| deleteScene | `{sceneId:"uuid"}` | Delete managed copy only; reject active/assigned scenes |
| pause / resume | `{monitorId:"device-path or *"}` | Independent manual pause blocker; automatic blockers remain active |
| getMetrics | `{}` | Last observed per-output five-second metrics; empty until measured |
| reloadDisplayTopology | `{}` | Request bounded output rebuild |
| stop | `{}` | Reply, then stop Core and close matching Manager windows on this desktop |

Full settings example:

```json
{"schemaVersion":2,"fps":30,"gpu":"auto","pauseFullscreen":true,"pauseMaximized":false,"pauseBattery":true,"language":"ru","autostart":false,"catalogBaseUrl":"","catalogAssetOrigins":[],"allowLocalCatalog":false}
```

## Ownership, timeouts and failure limits

A single worker accepts/frames I/O, enqueues one job and wakes the Win32 loop.
Graphics and SQLite work execute only on that loop. Connect/read/write use
cancellable overlapped I/O, 2-second transport deadlines and a 10-second queued
command wait. Manager waits at most about 12 seconds after connection. Stop
cancels outstanding I/O and joins the pipe worker before graphics teardown.
A command already executing can outlive a caller timeout; refresh status before
retrying a mutation. Mutations are not automatically retried or deduplicated.

Manager performs exchanges off the GUI thread, polls status after reconnect,
retains visible errors and refreshes after mutations. Closing Manager sends no
stop command. A disconnected Manager invents no library entries or metrics.
The Unix test socket and injected test transport are development tools and do not
establish Windows pipe ACL, scheduling, tray or D3D compatibility.

Imports are synchronous bounded operations on Core's control thread in this first
M3 slice; large imports can briefly delay rendering. The local preview library is
limited to 128 entries and 2048×2048 preview images. Paging, cancellable background
imports and custom data-root migration remain follow-up work.

M3 schema-v1 settings migrate in memory with catalog disabled by default; the next
save persists schema v2. Core stores catalog configuration but performs no network
requests. Manager validates URL origins before updating settings. Library schema
v2 adds successful download receipts without modifying scene/favorite rows.
