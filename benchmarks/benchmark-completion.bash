# Bash completion for "make benchmark <name>".
#
# The benchmark case names are not real make targets (they are swallowed goals,
# see the 'benchmark' target in the Makefile), so the stock make completion
# cannot offer them. This wrapper completes them from benchmarks/*.benchmark.cc
# and otherwise defers to the normal make completion.
#
# Repo-scoped by design: source it in the shell where you work on this repo:
#
#     source benchmarks/benchmark-completion.bash
#
# It only takes effect in that shell session - nothing is installed globally, no
# ~/.bashrc or user completions dir changes - and benchmark names are only
# offered while the current directory has benchmarks/*.benchmark.cc.

# Make sure the stock make completion (_make) is loaded first, so we can defer
# to it for everything that is not a benchmark name. Source it straight from the
# system data dirs rather than via _completion_loader, so loading is unaffected
# by any user-level "make" completion.
if ! declare -F _make >/dev/null 2>&1; then
    _wmb_ifs=$IFS
    IFS=:
    for _wmb_dir in ${XDG_DATA_DIRS:-/usr/local/share:/usr/share}; do
        if [[ -r "$_wmb_dir/bash-completion/completions/make" ]]; then
            . "$_wmb_dir/bash-completion/completions/make"
            break
        fi
    done
    IFS=$_wmb_ifs
    unset _wmb_dir _wmb_ifs
fi

_wmbusmeters_make_benchmark()
{
    local cur="${COMP_WORDS[COMP_CWORD]}"

    # "make benchmark <TAB>" / "make benchmark cha<TAB>": complete case names.
    if [[ "${COMP_WORDS[1]}" == "benchmark" && $COMP_CWORD -eq 2 ]]; then
        local names
        names=$(ls benchmarks/*.benchmark.cc 2>/dev/null \
                | sed 's#.*/##; s#\.benchmark\.cc$##')
        COMPREPLY=( $(compgen -W "$names" -- "$cur") )
        return 0
    fi

    # Anything else: fall back to the standard make completion if present.
    if declare -F _make >/dev/null 2>&1; then
        _make
    fi
}

complete -F _wmbusmeters_make_benchmark make gmake gnumake colormake
