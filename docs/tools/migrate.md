# bb-migrate

`bb-migrate` upgrades a database's schema from one application version to the
next by applying versioned SQL files in order, and remembering what it has
already applied.

Persist creates a table the first time it's used (`CREATE TABLE IF NOT EXISTS`)
but never alters one. That's fine for version 1 of an app and not enough for
version 2. `bb-migrate` covers that gap: it owns **schema evolution**, while
Persist owns **runtime data access**. They only meet at the database file.

> **Status: step 1.** Forward-only `status` and `up` against SQLite. See
> [Not Yet](#not-yet) for what's deliberately left out.

---

# Writing Migrations

One SQL file per change, named `<version>_<name>.sql`:

```txt
migrations/
├── 0001_create_tasks.sql
├── 0002_add_task_status.sql
└── 0003_index_task_status.sql
```

```sql
-- 0002_add_task_status.sql
ALTER TABLE tasks ADD COLUMN status TEXT NOT NULL DEFAULT 'open';
```

- `<version>` is a non-negative integer and defines apply order. It is compared
  numerically, so `0002` comes before `10`. Zero-padded counters and
  timestamps (`20261009120000_...`) both work.
- `<name>` may contain letters, digits, `_` and `-`.
- A file may hold several statements.
- Don't put `BEGIN`/`COMMIT` in a migration — the tool wraps each one in its own
  transaction.

---

# Using It

```bash
bb-migrate status --db app.db migrations/*.sql
bb-migrate up     --db app.db migrations/*.sql
```

As with `bb-codegen`, the shell expands the glob; the tool does no directory
scanning of its own. Always pass the **complete** set of files.

`status` is read-only and works on any database, including a brand-new one:

```txt
VERSION    STATE         NAME
1          applied       create_tasks
2          pending       add_task_status

1 pending, 0 problem(s)
```

`up` applies pending migrations in version order and prints each as it commits.
Run it again and it reports `up to date`.

Exit codes: `0` success, `1` failure or a problem found, `2` bad usage.
`status` exits `1` when it finds a problem, so it works as a CI check.

---

# How a Database Knows Its Version

`up` maintains a ledger table, `bb_schema_migrations`, with one row per applied
migration: `version`, `name`, `checksum` (SHA-256 of the file, with CRLF and LF
treated the same) and `applied_at`. The database's schema version is simply
"which rows are in this table". Nothing is stored in the application.

Each migration runs in **one transaction together with its ledger row**, so
"recorded" always means "fully applied". If a migration fails, its changes are
rolled back, the run stops, and earlier migrations stay applied. Fix the file
and run `up` again.

---

# Safety Rules

History is append-only. `status` classifies every version, and `up` refuses to
run while any of these exist:

| State          | Meaning                                                          |
|----------------|------------------------------------------------------------------|
| `applied`      | In the ledger and the file is unchanged.                         |
| `pending`      | New file, newer than anything applied. `up` will apply it.       |
| `MODIFIED`     | File was edited after it was applied (checksum differs).        |
| `MISSING`      | Applied, but its file wasn't passed in.                          |
| `OUT-OF-ORDER` | New file *older* than the latest applied one (e.g. a late merge). |

The fix for `MODIFIED` is to leave the old file alone and add a new migration.
The tool never guesses.

---

# Architecture

```txt
CLI (main.c)
   │
   ▼
Runner      orchestrates: ledger → plan → apply each pending migration
   │   │
   │   └──▶ Planner      pure: (files) × (ledger) → state per version
   │
   ├──▶ Source loader     pure: file paths → sorted, checksummed migrations
   │
   ▼
Driver      function table; the only layer that knows a database
   │
   ▼
SQLite driver
```

| Layer   | Knows about                | Doesn't know about |
|---------|----------------------------|--------------------|
| Source  | file names, contents, hash | databases, ledger  |
| Planner | versions and checksums     | files, databases   |
| Runner  | the order of operations    | SQL, any database  |
| Driver  | one database engine        | what a plan is     |

Because the planner and source loader are pure, every safety rule above is
covered by fast unit tests with no database. Only the driver and the runner's
integration tests touch SQLite.

The driver is a function table (`bb_migrate_driver_api_t`), the same pattern as
Persist's `bb_model_api_t`. Supporting another database means writing one
driver; nothing above it changes.

**Why not use Persist's SQLite backend?** It's a connection pool built for
concurrent CRUD and exposes no transactions. A migration run needs a single
connection and explicit `BEGIN`/`COMMIT` per step. `bb_migrate_core` therefore
links SQLite directly and has no dependency on `bluebird_persist`.

**Concurrency.** Each migration starts with `BEGIN IMMEDIATE`, so two runners
queue up on SQLite's write lock (5s busy timeout) instead of interleaving. The
ledger row is inserted *before* the migration's SQL runs, so if another process
already applied that version, the primary key rejects it before any DDL
executes.

---

# Not Yet

Left out on purpose so step 1 stays small. The seams for each already exist:

- **Rollback / `down`.** Forward-only is the safer default; the ledger already
  has what a `down` would need.
- **Other databases.** Add a driver. Note that databases without transactional
  DDL (e.g. MySQL) can't give the same per-migration atomicity — the driver
  should document what it can guarantee.
- **Directory scanning (`--dir`).** Needs a portable directory-listing helper
  (the project builds on Windows too).
- **Out-of-order opt-in** (`--allow-out-of-order`) for teams merging branches.
- **Generating migrations** from a change to a `persist.schema` manifest, as a
  new `bb-codegen` generator kind that writes a numbered `.sql` file.
- **Library API / run on app startup.** The core is already a library
  (`bb_migrate_core`); it just isn't a public module yet.
