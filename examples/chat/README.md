# Blue-Bird Chat Example

A minimal, persistent, real-time messaging app built on Blue-Bird. Users
register with a username/password, log in, and message each other by
username. Conversations are stored in SQLite; messages are delivered live
over a websocket when both people are online, and are always available on
reload (loaded from SQLite via `GET /api/messages/:peer`).

## Running

From the repository root:

```bash
mkdir build && cd build
cmake ..
make example_chat
./examples/chat/example_chat
```

Then open `http://localhost:8080`. The database file `chat_sqlite.db` is
created next to the binary on first run.

Open the page in two different browsers (or one normal + one private/
incognito window) and register two different users to try live chat.

## Layout

```
examples/chat/
├── main.c                 wiring: db, repos, routes, websocket, server
├── include/
│   ├── app_repo.h          User / Message entities + schemas
│   ├── app_util.h          cookies, sessions, safe JSON helpers
│   ├── app_handlers.h      HTTP route handlers
│   ├── app_ws.h            websocket handler + presence registry
│   └── app_html.h          embedded frontend
└── src/
    ├── app_repo.c
    ├── app_util.c
    ├── app_handlers.c
    ├── app_ws.c
    └── app_html.c           <!-- generated from a plain .html file -->
```

## HTTP API

| Method | Path                 | Auth | Description                          |
|--------|----------------------|------|---------------------------------------|
| GET    | `/`                  | no   | Serves the single-page frontend       |
| POST   | `/api/register`      | no   | `{username, password}` → creates user, sets session cookie |
| POST   | `/api/login`          | no   | `{username, password}` → sets session cookie |
| POST   | `/api/logout`         | yes  | Destroys the session                  |
| GET    | `/api/me`              | yes  | `{ok, username}` for the current session |
| GET    | `/api/users`           | yes  | All other users, with `online` flag   |
| GET    | `/api/messages/:peer`  | yes  | Full conversation history with `peer` |
| WS     | `/ws`                  | see below | Live chat channel                |

Auth for HTTP routes is a `session_id` cookie, issued by `/api/register` and
`/api/login` and validated through the Security module's session store
(`bb_session_get`).

## Websocket protocol

Blue-Bird's public websocket API only calls the app's handler for text/binary
frames — there's no connect/disconnect hook that exposes the originating
HTTP request or its cookies. So a connection authenticates itself explicitly
as its first message:

```jsonc
// client -> server, sent immediately after the socket opens
{"type": "auth", "session_id": "..."}

// client -> server, once authenticated
{"type": "message", "to": "bob", "body": "hey!"}
```

```jsonc
// server -> client
{"type": "auth_ok", "username": "alice"}
{"type": "auth_error", "error": "invalid or expired session"}
{"type": "message", "id": "...", "from": "alice", "to": "bob", "body": "hey!", "created_at": 1234567890}
{"type": "error", "error": "..."}
```

A sent message is both persisted to SQLite and echoed back to the sender
(so their UI gets the canonical id/timestamp) and, if the recipient is
currently connected, forwarded to them immediately. If they're offline the
message simply waits in SQLite until they next call `GET /api/messages/:peer`.

Because the framework doesn't surface disconnects to the app either,
"online" connections are tracked in memory and lazily evicted whenever a
send to them fails — there's a brief window after an ungraceful disconnect
where a user may still show as online.

### Why the session cookie isn't `HttpOnly`

The frontend needs the raw session id to include in the websocket's `auth`
message, and there's no way for it to ask the server for that without
either reading the cookie itself or the server handing it back out over a
plain HTTP response (which is no more private). For a real deployment you'd
want a short-lived, single-purpose websocket ticket instead of reusing the
HTTP session cookie this way — this example keeps it simple since it's a
demo of the framework, not a security-hardened product.

## Persistence

Two schema-driven repos on the SQLite backend (`bb_repo_t`), same pattern as
the `todo` example:

- `users (username TEXT PRIMARY KEY, password_hash TEXT)`
- `messages (id TEXT PRIMARY KEY, from_user TEXT, to_user TEXT, body TEXT, created_at INTEGER)`

Passwords are never stored in plaintext — `bb_password_hash` /
`bb_password_verify` from the Security module handle that.

Conversation history is fetched with `bb_repo_filter` (loads all messages,
filters in-memory to the two participants, sorts by `created_at`). That's
fine at demo scale; a production app would push that filtering down to SQL.
