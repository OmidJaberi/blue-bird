-- Matches what Persist creates for schemas/task.schema.json.
-- IF NOT EXISTS lets this adopt databases that Persist already created
-- before migrations were introduced.
CREATE TABLE IF NOT EXISTS tasks (
    id     TEXT PRIMARY KEY,
    name   TEXT,
    status TEXT
);
