#!/bin/bash
# Use stdout if no argument is given, otherwise write to the specified file
# Copyright (C) 2026 Aras Abbasi (gpl-3.0-or-later)

if [ -z "$1" ]; then
    OUT="/dev/stdout"
else
    OUT="$1"
fi

curl -s 'https://www.dlms.com/srv/FlagIdLoader.php' | jq -r '
def f:
  if contains("\t") then gsub("\t"; "    ") | "\""+.+"\"" else . end
  | if contains("\"") then gsub("\""; "\"\"") | "\""+.+"\"" else . end;

(now | strftime("# Downloaded spreadsheet from https://www.dlms.com/flag-id/flag-id-list",
"# and saved as tab separated list and added this header. %Y-%m-%d")),
"# FLAG ID <TAB> Manufacturer <TAB> Country <TAB> World Region",
(
  .Items
  | sort_by(.FlagID)
  | .[]
  | .FlagID as $id
  | (.Company | f) as $c
  | (.Country // "" | f) as $co
  | (.Region // "" | f) as $r
  | [$id, $c]
    + (if $co != "" then [$co] else [] end)
    + (if $r != "" then [$r] else [] end)
  | @tsv
)
' > "$OUT"