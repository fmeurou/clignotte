#include "completions.h"
#include "common.h"

namespace {
void writeBashCompletion()    {
    out << R"BASH(_note_complete() {
    local cur cmds
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    cmds="list today week bytag add edit move import close close-notebook due important bold blink normal delete notebook notebooks search attach attachments detach tag untag tags export sync completions"

    if [ "$COMP_CWORD" -eq 1 ]; then
        COMPREPLY=( $(compgen -W "$cmds --json --tag --notebook --all --help --version" -- "$cur") )
        return 0
    fi

    case "${COMP_WORDS[1]}" in
        import|attach|export|sync)
            COMPREPLY=( $(compgen -f -- "$cur") )
            ;;
        completions)
            COMPREPLY=( $(compgen -W "bash zsh fish" -- "$cur") )
            ;;
    esac
}
complete -F _note_complete note
)BASH";
    out.flush();
}

void writeZshCompletion()    {
    out << R"ZSH(#compdef note

_note() {
    local -a cmds
    cmds=(
        'list:display notes in a table'
        'today:open notes due today or overdue'
        'week:open notes due in the next 7 days'
        'bytag:display notes grouped by tag'
        'add:add a note (use "-" to read from stdin)'
        'edit:open $EDITOR on a note'
        'move:move a note to another notebook'
        'import:import notes from a file (or "-" for stdin)'
        'close:mark a note as done'
        'due:set a due date for a note'
        'important:mark a note as important (red)'
        'bold:mark a note as bold (inverted)'
        'blink:mark a note as blinking yellow'
        'normal:strip styling prefix from a note'
        'delete:delete a note'
        'notebook:switch to or create a notebook'
        'notebooks:list all notebooks'
        'close-notebook:mark every open note in a notebook as done'
        'search:full-text search across all notes'
        'attach:attach a file or URL to a note'
        'attachments:list attachments for a note'
        'detach:remove an attachment'
        'tag:add tags to a note'
        'untag:remove tags from a note'
        'tags:list all tags (or tags on a note)'
        'export:write notes to a directory as .md files'
        'sync:reconcile notes from a directory into the database'
        'completions:emit shell completion script'
    )
    if (( CURRENT == 2 )); then
        _describe 'command' cmds
        _arguments '--json[machine-readable JSON output]' '--tag[filter by tag]:tag:' '--notebook[filter by notebook]:notebook:' '--all[include closed notes]' '--help[show help]' '--version[show version]'
    else
        case "${words[2]}" in
            import|attach|export|sync) _files ;;
            completions) _values 'shell' bash zsh fish ;;
            attach) _arguments '--copy[copy file into storage]' && _files ;;
        esac
    fi
}

_note "$@"
)ZSH";
    out.flush();
}

void writeFishCompletion()    {
    out << R"FISH(# fish completions for note (clignotte)

complete -c note -f

complete -c note -n '__fish_use_subcommand' -a list        -d 'display notes in a table'
complete -c note -n '__fish_use_subcommand' -a today       -d 'notes due today or overdue'
complete -c note -n '__fish_use_subcommand' -a week        -d 'notes due in the next 7 days'
complete -c note -n '__fish_use_subcommand' -a bytag       -d 'display notes grouped by tag'
complete -c note -n '__fish_use_subcommand' -a add         -d 'add a note (use - for stdin)'
complete -c note -n '__fish_use_subcommand' -a edit        -d 'open $EDITOR on a note'
complete -c note -n '__fish_use_subcommand' -a move        -d 'move a note to another notebook'
complete -c note -n '__fish_use_subcommand' -a import      -d 'import from file (or - for stdin)'
complete -c note -n '__fish_use_subcommand' -a close       -d 'mark a note as done'
complete -c note -n '__fish_use_subcommand' -a due         -d 'set a due date for a note'
complete -c note -n '__fish_use_subcommand' -a important   -d 'mark a note as important'
complete -c note -n '__fish_use_subcommand' -a bold        -d 'mark a note as bold'
complete -c note -n '__fish_use_subcommand' -a blink       -d 'mark a note as blinking'
complete -c note -n '__fish_use_subcommand' -a normal      -d 'strip styling from a note'
complete -c note -n '__fish_use_subcommand' -a delete      -d 'delete a note'
complete -c note -n '__fish_use_subcommand' -a notebook    -d 'switch to or create a notebook'
complete -c note -n '__fish_use_subcommand' -a notebooks   -d 'list all notebooks'
complete -c note -n '__fish_use_subcommand' -a close-notebook -d 'close every open note in a notebook'
complete -c note -n '__fish_use_subcommand' -a search      -d 'full-text search'
complete -c note -n '__fish_use_subcommand' -a attach      -d 'attach a file or URL to a note'
complete -c note -n '__fish_use_subcommand' -a attachments -d 'list attachments for a note'
complete -c note -n '__fish_use_subcommand' -a detach      -d 'remove an attachment'
complete -c note -n '__fish_use_subcommand' -a tag         -d 'add tags to a note'
complete -c note -n '__fish_use_subcommand' -a untag       -d 'remove tags from a note'
complete -c note -n '__fish_use_subcommand' -a tags        -d 'list all tags (or tags on a note)'
complete -c note -n '__fish_use_subcommand' -a export      -d 'write notes to a directory as .md files'
complete -c note -n '__fish_use_subcommand' -a sync        -d 'reconcile notes from a directory into the database'
complete -c note -n '__fish_use_subcommand' -a completions -d 'emit shell completion script'

complete -c note -n '__fish_seen_subcommand_from import attach export sync' -F
complete -c note -n '__fish_seen_subcommand_from completions' -a 'bash zsh fish'

complete -c note -l json     -d 'machine-readable JSON output'
complete -c note -l tag      -d 'filter listings by tag (repeatable; AND)' -r
complete -c note -l notebook -d 'filter listings by notebook (exact, case-sensitive)' -r
complete -c note -l all      -d 'include closed notes in listings'
complete -c note -l copy     -d 'copy file into clignotte storage (with attach)'
complete -c note -l delete -d 'mirror semantics for sync (delete local notes whose files are absent)'
complete -c note -l help -d 'show help'
complete -c note -l version -d 'show version'
)FISH";
    out.flush();
}
} // namespace

void writeCompletions(const QString &shell)    {
    if(shell == "bash")      writeBashCompletion();
    else if(shell == "zsh")  writeZshCompletion();
    else if(shell == "fish") writeFishCompletion();
    else    {
        out << "unknown shell: " << shell << " (supported: bash, zsh, fish)\n";
        out.flush();
    }
}
