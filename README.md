# clignotte

A note keeper backed by a local SQLite database, available as both a
command-line tool (`note`) and a Qt Quick desktop GUI (`note-gui`).
Notes are grouped into notebooks, support due dates and tags, and can be
styled with single-char prefixes that the renderer turns into ANSI colour codes.

The binary is named `note`; the project (and database) are named `clignotte`.

## Install

### Pre-built binaries

Download `note` and `note-gui` from the
[latest GitHub release](https://github.com/fmeurou/clignotte/releases/latest)
(Arch Linux x86\_64 / aarch64).

### Arch Linux (AUR)

```sh
yay -S clignotte
```

### Build from source

**CLI** — requires `qt6-base`:

```sh
qmake6
make
sudo make install        # installs note, shell completions, and the license
```

**GUI** — additionally requires `qt6-declarative`:

```sh
qmake6 -o Makefile.gui clignotte-gui.pro
make -f Makefile.gui
sudo make -f Makefile.gui install
```

Both builds abort with a clear error if the Qt 5 `qmake` is used by mistake.

Optional runtime tools (detected at startup; `note --help` shows which are available):

| Tool | Package | Used for |
|---|---|---|
| `whisper` | `python-openai-whisper` | Audio transcription |
| `tesseract` | `tesseract tesseract-data-eng` | Image/PDF OCR |
| `pdftoppm` | `poppler` | PDF-to-image conversion for OCR |

## GUI

`note-gui` is a Qt Quick / Material desktop app that shares the same
`~/.local/share/clignotte/notes.db` as the CLI — you can use both interchangeably.

**Features:**

- Notebook switcher in the toolbar (⋮ menu)
- Note list with inline search, tag chips, and attachment count
- Unified create/edit form:
  - Rich text area
  - Due date picker with Today / Clear shortcuts
  - Tag management (add by typing, remove with ×)
  - File and URL attachments — click a chip to open it
  - Move note to another notebook
  - Mark done / Reopen / Delete
- **Sync button (⟳):** exports all notes to a chosen directory as
  `<uuid>.md` files, runs `git add / commit / pull --rebase / push` if the
  directory is a git repo, then reconciles the files back into the database.
  The sync directory is remembered between sessions.

## CLI usage

Running `note` with no command prints all notes grouped by notebook:

```sh
note
```

By default, listings only show open notes. Pass `--all` to include closed ones.
`note --help` lists every command.

| Command | Description |
|---|---|
| *(no command)* | display notes grouped by notebook |
| `list` | display notes in a single-line table |
| `today` | open notes due today or overdue, across all notebooks |
| `week` | open notes due in the next 7 days |
| `bytag` | display notes grouped by tag |
| `add <text>` | add a note to the current notebook |
| `add -` | read note text from stdin |
| `edit <id>` | open `$VISUAL` / `$EDITOR` on note `<id>` |
| `move <id> <title>` | move note to another notebook |
| `import <path>` | import from a text file, audio file, or image/PDF |
| `import -` | import from stdin (one note per non-empty line) |
| `close <id>` | mark note as done |
| `due <id> <date>` | set due date `yyyy-MM-dd` |
| `important <id>` | mark as important (red) |
| `bold <id>` | mark as bold (inverted) |
| `blink <id>` | mark as blinking yellow |
| `normal <id>` | strip any styling prefix |
| `delete <id>` | delete a note |
| `notebooks` | list all notebooks |
| `notebook <title>` | switch to or create a notebook |
| `close-notebook <title>` | mark every open note in a notebook as done |
| `search <query>` | full-text search (FTS5 syntax) |
| `attach <id> <path>` | attach a local file or URL |
| `attach <id> <path> --copy` | copy the file into clignotte storage, then attach |
| `attachments <id>` | list attachments |
| `detach <attach_id>` | remove an attachment |
| `tag <id> <name>...` | add one or more tags |
| `untag <id> <name>...` | remove tags |
| `tags` | list all tags with usage counts |
| `tags <id>` | list tags on a specific note |
| `export <dir>` | write all notes to `<dir>` as `<uuid>.md` files |
| `sync <dir>` | reconcile a `<dir>` of `.md` files into the database |
| `completions <shell>` | emit a shell completion script (`bash`, `zsh`, or `fish`) |

### Examples

```sh
note notebook work          # switch to (or create) the "work" notebook
note add Call the bank      # add a note
note due 3 2026-06-01       # set a due date
note important 3            # highlight in red
note close 3                # mark as done
note search "bank OR loan"  # FTS5 boolean search
note today                  # what's due today or overdue
note week                   # what's due in the next 7 days
note bytag                  # notes grouped by tag
note list --tag home        # filter by tag
note import bullets.txt     # one note per line
note import meeting.opus    # transcribe audio
note import scan.png        # OCR image
note attach 5 spec.pdf      # attach a file
note attach 5 https://example.com   # attach a URL
```

### Piping and scripting

```sh
echo "captured from a pipe" | note add -
ID=$(note add "buy milk") && note tag "$ID" shopping
git log --oneline -10 | note import -
note list --json | jq '.[] | select(.due_date)'
note today --json | jq -r '.[].id' | xargs -n1 note close
```

`--json` is supported on `list`, `today`, `week`, `search`, and the no-arg view.
ANSI styling is suppressed automatically when stdout is not a tty; set
`NO_COLOR=1` to disable it on a tty as well.

### Sync via git

```sh
# one-time setup
mkdir ~/clignotte-sync && cd ~/clignotte-sync
git init && git remote add origin <your-remote>

# push local changes
note export ~/clignotte-sync
git -C ~/clignotte-sync add -A && git -C ~/clignotte-sync commit -m "sync"
git -C ~/clignotte-sync push

# pull remote changes
git -C ~/clignotte-sync pull
note sync ~/clignotte-sync           # additive merge
note sync ~/clignotte-sync --delete  # mirror (also deletes local notes whose files are gone)
```

The GUI's ⟳ sync button automates this entire cycle for you.

Each `.md` file has a key:value header (uuid, notebook, created_at, due_date,
done_at, tags) followed by a blank line and the note body. Attachments are
local-only and not exported.

### Shell completions

```sh
# bash
note completions bash > ~/.local/share/bash-completion/completions/note

# zsh (file must be in $fpath, named _note)
note completions zsh > "${fpath[1]}/_note"

# fish
note completions fish > ~/.config/fish/completions/note.fish
```

### Storage

Notes are stored at `$XDG_DATA_HOME/clignotte/notes.db`
(typically `~/.local/share/clignotte/notes.db`).

## License

MIT — see [LICENSE](LICENSE).
