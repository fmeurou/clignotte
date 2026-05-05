# clignotte

A small command-line note keeper backed by a local SQLite database. Notes are
grouped into notebooks, support due dates, and can be styled with single-char
prefixes that the renderer turns into ANSI colour codes.

The binary is named `note`; the project (and database) are named `clignotte`.

## Features

- Multiple notebooks, with one always flagged as the current one
- Optional due dates, with overdue notes highlighted in red
- Mark notes as done (closed), important, bold, or blinking yellow
- Full-text search across all notes (SQLite FTS5)
- Bulk import from a text file (one note per line), an audio file
  (one note per Whisper segment, with timestamps), or an image/PDF
  (one note per non-empty OCR line via Tesseract)
- Attach local files or URLs to notes; optionally copy files into the
  clignotte storage folder
- Each note carries a UUID for sync with external services
- Compact, terminal-width-aware listing
- Stored as plain SQLite at `$XDG_DATA_HOME/clignotte/notes.db`
  (typically `~/.local/share/clignotte/notes.db`)

## Build

Requires Qt 6 with the `core` and `sql` modules, plus the SQLite driver.
On Arch Linux: `pacman -S qt6-base`.

Optional tools (detected at runtime; `note --help` shows which are available):

| Tool | Package | Used for |
|---|---|---|
| `whisper` | `python-openai-whisper` | Audio transcription |
| `tesseract` | `tesseract tesseract-data-eng` | Image/PDF OCR |
| `pdftoppm` | `poppler` | PDF-to-image conversion (required for PDF OCR) |

```sh
qmake6
make
```

The build aborts with a clear error if you run the Qt 5 `qmake` by mistake.
The resulting binary is `./note`.

An Arch `PKGBUILD` is provided under [`pkg/archlinux/`](pkg/archlinux/PKGBUILD).

## Usage

Running `note` with no command prints all notes grouped by notebook:

```sh
note
```

`note --help` lists every command. The full reference:

| Command | Description |
|---|---|
| *(no command)* | display notes grouped by notebook |
| `list` | display notes in a single-line table |
| `add <text>` | add a note to the current notebook |
| `import <path>` | import notes from a text file (one per line), audio file (one per Whisper segment), or image/PDF (one per OCR line) |
| `close <id>` | mark note `<id>` as done |
| `due <id> <date>` | set due date `yyyy-MM-dd` for note `<id>` |
| `important <id>` | mark note `<id>` as important (red) |
| `bold <id>` | mark note `<id>` as bold (inverted) |
| `blink <id>` | mark note `<id>` as blinking yellow |
| `normal <id>` | strip any styling prefix from note `<id>` |
| `delete <id>` | delete note `<id>` |
| `notebooks` | list all notebooks |
| `notebook <title>` | switch to or create notebook `<title>` |
| `search <query>` | full-text search across all notes (FTS5 syntax) |
| `attach <id> <path>` | attach a local file or URL to note `<id>` |
| `attach <id> <path> --copy` | copy the file to clignotte storage then attach |
| `attachments <id>` | list attachments for note `<id>` |
| `detach <attach_id>` | remove an attachment by its id |

### Examples

```sh
note notebook work          # switch to (or create) the "work" notebook
note add Call the bank      # add a note to the current notebook
note due 3 2026-06-01       # give note 3 a due date
note important 3            # highlight note 3 in red
note close 3                # mark note 3 as done
note search "bank OR loan"  # FTS5 boolean search
note list                   # one-line-per-note overview
note import bullets.txt     # one note per non-empty line
note import meeting.opus    # transcribe audio, one note per segment
note import scan.png        # OCR image, one note per line
note import report.pdf      # OCR PDF (requires poppler for pdftoppm)
note attach 5 spec.pdf      # store path to spec.pdf on note 5
note attach 5 photo.jpg --copy  # copy photo.jpg into clignotte storage
note attachments 5          # list attachments on note 5
note detach 2               # remove attachment id 2
```

### Status icons in `list` / `search`

Each row shows a single status character. Priority (highest wins):

| Icon | Meaning | Style |
|---|---|---|
| `✓` | done (`close`d) | italic |
| `!` | overdue (due date is in the past) | red background |
| `~` | blink-marked | blinking yellow |
| `*` | important-marked | red |
| `>` | bold-marked | inverted |
| `·` | open / no styling | default |

### Styling prefixes

Styling is encoded as a single leading character in the note's text:

- `!foo` → important
- `*foo` → bold
- `~foo` → blink yellow

The mark/normal commands manage these prefixes for you, but you can also type
them directly when adding a note (`note add !urgent fix`). Switching styles
replaces the prefix without losing content; `normal` strips it.

### Search syntax

`search` uses SQLite FTS5 and supports its query language:

- `pdf` — single token
- `"exact phrase"` — phrase
- `bank OR loan` — boolean OR (also `AND`, `NOT`)
- `migr*` — prefix match

Results are ordered by FTS rank (most relevant first).

### Importing notes

`note import <path>` dispatches based on the file extension:

- **Text files** (any non-audio extension): each non-empty line becomes a note
  in the current notebook. Whitespace-only lines are skipped; styling prefixes
  (`!`, `*`, `~`) at the start of a line are honoured. The whole import runs in
  a single SQL transaction, so large files are fast and partial failures roll
  back.
- **Audio files** (`mp3`, `wav`, `ogg`, `oga`, `opus`, `m4a`, `mp4`, `flac`,
  `aac`, `webm`, `wma`, `amr`, `mka`, `3gp`, `mpeg`, `mpga`): `clignotte`
  shells out to [`whisper`](https://github.com/openai/whisper)
  (`pacman -S python-openai-whisper`) with `--output_format json` and creates
  one note per transcribed segment, prefixed with the segment start time as
  `[HH:MM:SS]`. Whisper's stderr (model loading, progress) is forwarded so you
  can follow long transcriptions.

  If `whisper` isn't on `$PATH`, you'll get a clear hint pointing at the
  package name. Auto language detection is left to Whisper; pre-process or
  rename your file if you want to force a specific language for now.

- **Image files** (`png`, `jpg`, `jpeg`, `tiff`, `tif`, `bmp`, `pnm`, `pbm`,
  `pgm`, `ppm`, `webp`): `clignotte` shells out to
  [`tesseract`](https://github.com/tesseract-ocr/tesseract)
  (`pacman -S tesseract tesseract-data-eng`) and creates one note per
  non-empty output line. Tesseract's stderr (model loading) is forwarded.
  Install additional language packs (`tesseract-data-fra`, etc.) for
  non-English documents.

- **PDF files**: same as image OCR, but `clignotte` first converts each page
  to a PNG using `pdftoppm` from
  [`poppler`](https://poppler.freedesktop.org/) (`pacman -S poppler`), then
  OCRs each page with `tesseract`. Both tools must be on `$PATH`; missing
  either produces a clear install hint.

`note --help` shows which optional tools are present on your system.

### Attachments

A note can have any number of attachments — local file paths or URLs — stored
in the database (files are not copied by default).

```sh
note attach 5 /path/to/file.pdf      # store the path
note attach 5 https://example.com    # store the URL
note attach 5 photo.jpg --copy       # copy into ~/.local/share/clignotte/attachments/
note attachments 5                   # list attachment ids, paths, and copy origins
note detach 3                        # remove attachment with id 3 (does not delete copied files)
```

Copied files are stored as `<uuid>_<original_filename>` inside the clignotte
attachments folder so names never collide.

### Terminal width

`list` and `search` adapt the text column to your terminal width via
`ioctl(TIOCGWINSZ)`, with `$COLUMNS` as a fallback (useful for piping). The
text column never shrinks below 20 characters.

## License

MIT — see [LICENSE](LICENSE).
