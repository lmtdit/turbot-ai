#pragma once

#include <turbot/storage/migration.hpp>

namespace turbot::storage::migrations {

// ─── v1: Initial schema ──────────────────────────────────────────────────────

/// Migration v1 — create the sessions table.
TURBOT_MIGRATION(
    CreateSessionsTable,
    1,
    // up
    R"SQL(
        CREATE TABLE IF NOT EXISTS sessions (
            id          TEXT    PRIMARY KEY NOT NULL,
            project_id  TEXT    NOT NULL,
            parent_id   TEXT,
            slug        TEXT    NOT NULL,
            directory   TEXT    NOT NULL,
            title       TEXT    NOT NULL DEFAULT '',
            version     TEXT    NOT NULL DEFAULT '1.0.0',
            permission  TEXT,
            state       TEXT    NOT NULL DEFAULT 'created',
            time_created  INTEGER NOT NULL,
            time_updated  INTEGER NOT NULL,
            time_compacting INTEGER,
            time_archived   INTEGER
        );
        CREATE INDEX IF NOT EXISTS idx_sessions_project_id ON sessions(project_id);
    )SQL",
    // down
    "DROP TABLE IF EXISTS sessions;"
);

// ─── v2: Add revert field ─────────────────────────────────────────────────────

/// Migration v2 — add the revert JSON column to the sessions table.
///
/// The column stores a JSON object matching RevertInfo:
///   { "message_id": "...", "part_id"?: "...", "snapshot_id"?: "...", "diff"?: "..." }
/// When NULL the session has no pending revert.
TURBOT_MIGRATION(
    AddSessionRevert,
    2,
    // up
    "ALTER TABLE sessions ADD COLUMN revert TEXT;",
    // down
    // SQLite does not support DROP COLUMN in versions < 3.35.0.
    // Rollback recreates the table without the revert column.
    R"SQL(
        CREATE TABLE sessions_backup AS SELECT
            id, project_id, parent_id, slug, directory, title,
            version, permission, state,
            time_created, time_updated, time_compacting, time_archived
        FROM sessions;
        DROP TABLE sessions;
        ALTER TABLE sessions_backup RENAME TO sessions;
    )SQL"
);

} // namespace turbot::storage::migrations
