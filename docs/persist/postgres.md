# PostgreSQL Backend

Blue-Bird can persist models in PostgreSQL through the same repository and model APIs used by the SQLite and JSON backends. It is an optional build-time feature built on libpq.

Application code does not change when you switch backends: schemas, `bb_repo_insert()`, `bb_repo_find_by_pk()`, `bb_repo_find()` and the rest work as before. Only the registration and connection string differ.

---

# Enabling PostgreSQL

The backend is off by default and requires libpq (the PostgreSQL client library). Enable it at configure time:

```bash
cmake .. -DBB_WITH_POSTGRES=ON
make
```

| Build                          | Behavior                                                                                         |
|--------------------------------|--------------------------------------------------------------------------------------------------|
| `BB_WITH_POSTGRES=OFF` (default) | No libpq dependency. The backend is not compiled, so `bb_model_postgres_api()` is not available. |
| `BB_WITH_POSTGRES=ON`          | libpq is located via `find_package(PostgreSQL REQUIRED)`. The `"postgres"` backend is available. |

When the option is on, `BB_WITH_POSTGRES` is also defined for code that links the persist module, so applications that support several backends can guard the call with `#ifdef BB_WITH_POSTGRES`.

## Installing libpq

Only the client library and headers are needed to build. A server is only needed to run it (and the tests).

| Platform | Command |
|----------|---------|
| Debian / Ubuntu | `sudo apt install libpq-dev` |
| macOS (Homebrew) | `brew install libpq`, then configure with `-DPostgreSQL_ROOT="$(brew --prefix libpq)"` |
| macOS (MacPorts) | `sudo port install postgresql16`, then pass the paths reported by the port's `pg_config` (`/opt/local/lib/postgresql16/bin/pg_config`): `-DPostgreSQL_INCLUDE_DIR="$(pg_config --includedir)" -DPostgreSQL_LIBRARY="$(pg_config --libdir)/libpq.dylib"` |
| Windows (vcpkg) | `vcpkg install libpq:x64-windows`, and configure with the vcpkg toolchain file |

If CMake reports `Could NOT find PostgreSQL`, pass `PostgreSQL_INCLUDE_DIR` and `PostgreSQL_LIBRARY` explicitly, as in the MacPorts row.

---

# Using the Backend

```c
#include <blue-bird/persist/repo.h>
#include <blue-bird/persist/model/model_postgres.h>

bb_model_register(bb_model_postgres_api());
const bb_model_api_t *api = bb_model_get("postgres");

bb_model_handle_t *handle =
    api->open("postgresql://app:secret@localhost:5432/appdb");
if (!handle) {
    /* could not connect; libpq's error message was written to stderr */
}

bb_repo_init(&task_repo.base, api, handle, &task_schema);
```

`open()` takes any libpq connection string, either a URI or key/value form:

```text
postgresql://user:password@host:5432/dbname
host=localhost port=5432 dbname=appdb user=app
```

`open()` connects immediately, so a bad address or credentials are reported there rather than on the first query. On failure it returns `NULL` and prints libpq's error message. The connection string itself is never printed, because it may contain a password.

Avoid hard-coding passwords. Read the connection string from the environment or a config file, or leave the password out and let libpq take it from `PGPASSWORD` or a `.pgpass` file. TLS to the server is controlled with libpq's own parameters, for example `?sslmode=require`, and needs a libpq built with SSL support.

The [todo example](../../examples/todo/README.md) can run on PostgreSQL by setting `BB_TODO_DB`; it ships a `compose.yaml` for a throwaway database.

Call `api->close(handle)` when done. Like the other backends, it must not be called while another thread is still using the handle.

---

# Type Mapping

| Schema field type  | PostgreSQL column |
|--------------------|-------------------|
| `BB_FIELD_INT`     | `INTEGER` (the entity holds a C `int`) |
| `BB_FIELD_STRING`  | `TEXT` |
| `BB_FIELD_UUID`    | `UUID` (native type) |
| `BB_FIELD_BLOB`    | `BYTEA` |

The primary key field becomes `PRIMARY KEY`. As with the SQLite backend, the other field flags (`REQUIRED`, `UNIQUE`, `AUTO_GENERATE`) are not interpreted yet, so callers fill UUIDs themselves.

UUID fields keep the usual `bb_uuid_t` string in your struct. PostgreSQL converts it on write and returns the canonical lowercase form on read. A malformed UUID makes the operation fail instead of storing it.

## Tables

A table is created with `CREATE TABLE IF NOT EXISTS` the first time a schema is used on a handle, and remembered afterwards. Existing tables are never altered, so schema changes still need a migration. If a table is dropped behind the handle's back, operations fail until the handle is reopened.

---

# Concurrency

Each handle owns a small pool of up to 8 connections, opened lazily. Every call borrows a connection for its duration and returns it, so concurrent callers run concurrently and a single connection is never used by two threads at once. If all 8 are in use, a caller waits until one is returned.

The pool size is currently the compile-time constant `BB_PG_POOL_MAX_CONNS` in `model_postgres.c`. Remember that every handle opens its own connections, so a server running several handles or processes can reach PostgreSQL's `max_connections` limit.

A connection that the server dropped while idle is reset the next time it is borrowed.

---

# Queries

`bb_repo_find()` is pushed down to a single parameterised `SELECT ... WHERE ... ORDER BY ... LIMIT ... OFFSET ...`. Identifiers are quoted and every value is a bound parameter.

The results match Blue-Bird's in-memory reference implementation (`bb_query_apply()`), not PostgreSQL's defaults:

- String comparison and `ORDER BY` are bytewise (`COLLATE "C"`), not locale-aware.
- `BB_OP_LIKE` treats only `%` as a wildcard. `_` and `\` are literal, and matching is case-sensitive.
- `LIKE` on a UUID field matches its text form. On an `INT` or `BLOB` field it matches nothing.
- A query with no matching rows returns a `NULL` array and a count of 0, unlike `find_all()`, which always returns a non-`NULL` buffer.
- A condition value that is not a valid UUID makes the query fail instead of matching nothing.

Paging without an `ORDER BY` has no defined order, the same as in memory.

---

# Running the Tests

The PostgreSQL tests skip themselves unless `BB_TEST_POSTGRES_URI` points at a database. Use a disposable one, for example a throwaway container:

```bash
docker run -d --rm --name bb-pg -e POSTGRES_PASSWORD=postgres -p 5432:5432 postgres:17

cmake -B build -DBB_WITH_POSTGRES=ON
cmake --build build

BB_TEST_POSTGRES_URI="postgresql://postgres:postgres@localhost:5432/postgres" \
  ctest --test-dir build -R model_postgres -V
```

A successful run prints `All postgres model tests passed`. If it prints `Skipping`, the variable is not set in that shell or the build was configured without `-DBB_WITH_POSTGRES=ON`.

The tests remove every row they insert and leave the empty table `bb_test_people_pg` behind. CI runs the same tests against PostgreSQL service containers on Linux and against a local server on Windows (MSVC).

---

# Current Limitations

- No `ALTER TABLE` or migrations for PostgreSQL; `bb-migrate` currently targets SQLite.
- `REQUIRED`, `UNIQUE` and `AUTO_GENERATE` schema flags are ignored.
- No transaction API.
- Pool size is fixed at build time.
