# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

**clignotte** is a SQLite-backed command-line note keeper. The binary is named `note`; there is also a Qt Quick GUI (`note-gui`). Notes live in `~/.local/share/clignotte/notes.db`.

## Build

Requires Qt 6 (`qt6-base` on Arch Linux). There is no test suite.

```sh
# CLI binary (./note)
qmake6
make

# GUI binary (./note-gui)
qmake6 clignotte-gui.pro
make -f Makefile.gui

# Install (default PREFIX=/usr/local)
sudo make install
sudo make uninstall
```

Always use `qmake6`, not `qmake` — the project file aborts with a clear error if Qt 5 is detected.

After linking, the Makefile runs `./note completions <shell>` to regenerate the scripts under `completions/` — those are always derived from the binary.

## Architecture: CLI (`clignotte.pro`)

`main.cpp` is a flat command dispatcher: it parses args with `QCommandLineParser`, then calls into one of the domain modules. There is no abstraction layer between the dispatcher and the SQL calls.

**All SQL strings live in `sql.h` as macros.** This is the single source of truth for the schema and all queries. New queries go here.

Module responsibilities:

| File | Responsibility |
|---|---|
| `db.cpp` | Open the DB, run schema bootstrap and additive migrations (`ALTER TABLE`) |
| `sql.h` | Every SQL statement as a `#define` macro |
| `common.cpp` | Global `QTextStream out`, ANSI escape string constants, `terminalCols()` |
| `render.cpp` | `printNoteTable()`, `printNotesJson()`, `tagFilterClause(n)` / `bindTags()` for dynamic tag filtering |
| `notebook.cpp` | Notebook CRUD, `getCurrentNotebook()`, `setNotebookByTitle()` |
| `notes.cpp` | Note CRUD, `markNote()` (styling prefix), `editNote()` (spawns `$VISUAL`/`$EDITOR`) |
| `tags.cpp` | Tag add/remove/list |
| `attachments.cpp` | Attach/list/detach files or URLs; `--copy` copies into storage dir |
| `import.cpp` | Dispatch by file extension → text/audio (whisper)/image/PDF (tesseract+pdftoppm) |
| `sync.cpp` | `exportNotes()` (DB → `<uuid>.md` files), `syncNotes()` (files → DB, additive or mirror) |
| `completions.cpp` | Emit bash/zsh/fish completion scripts to stdout |

**Styling is encoded as a single leading character in `note.text`:** `!` = important (red), `*` = bold (inverted), `~` = blink yellow. `markNote()` swaps or strips this prefix; the render layer reads it. This means the raw text in the DB includes the prefix character.

**Tag filtering is built dynamically.** `tagFilterClause(n)` returns a SQL `AND` fragment with `n` positional `?` placeholders; `bindTags()` binds them. This is used in `render.cpp` to add `--tag` filtering to any query.

**Schema migration** is additive: `db.cpp` checks for missing columns/tables and runs `ALTER TABLE` or `CREATE TABLE IF NOT EXISTS`. There is no versioned migration system.

The `--all` flag is threaded through from `main.cpp` into every listing function — it switches between queries that filter `done_at IS NULL` and those that don't.

## Architecture: GUI (`clignotte-gui.pro`)

The GUI does **not** share C++ code with the CLI — it has its own DB initialization (`DbController::ensureSchema()`) and its own SQL. They share the same database file.

| File | Responsibility |
|---|---|
| `gui/main.cpp` | `QGuiApplication` setup, Material style, registers models as uncreatable QML types, exposes `db` (a `DbController*`) as a QML context property |
| `gui/dbcontroller.cpp` | `Q_INVOKABLE` methods called from QML; owns `NotebookModel` and `NoteModel`; drives notebook switching and note CRUD |
| `gui/notebookmodel.cpp` | `QAbstractListModel` for notebooks |
| `gui/notemodel.cpp` | `QAbstractListModel` for notes in the current notebook |
| `gui/qml/` | QML UI — `Main.qml` is the root; `NotebookSidebar.qml`, `NoteList.qml`, `NoteEditor.qml` |

QML accesses everything through the `db` context property (`DbController`). Model data flows through `NotebookModel` and `NoteModel` as standard Qt model/view.

## Key constraints

- `--json` output is supported on `list`, `today`, `week`, `search`, and the no-arg view. ANSI styling is auto-suppressed when stdout is not a tty.
- The `note_fts` FTS5 virtual table is kept in sync via three SQL triggers (`note_ai`, `note_ad`, `note_au`); backfill runs once on first creation.
- Attachments are local-only and not exported/synced. The sync format (`.md` files with a key:value header) uses UUID as the durable identity across machines.
- `note_tag` rows are deleted automatically via a trigger when a note is deleted — no manual orphan cleanup.
