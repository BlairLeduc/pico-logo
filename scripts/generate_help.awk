#!/usr/bin/awk -f
#
# generate_help.awk — Parse Pico_Logo_Reference.md and emit help_data.c
#
# Usage: awk -f generate_help.awk reference/Pico_Logo_Reference.md > help_data.c
#
# Compatible with BSD awk (macOS) and gawk.
#
# Extracts primitive entries (## headings that contain a `command` or
# `operation` badge) and emits a sorted C array of { name, text } pairs.
# Aliases share the same string variable to avoid flash duplication.
#
# Markdown-to-Logo transformations applied:
#   _word_        -> :word      (italic parameters -> Logo variable refs)
#   [`text`](#..) -> text       (markdown links with code -> display text)
#   [text](#..)   -> text       (markdown links -> display text)
#   `code`        -> code       (strip backtick wrapping)
#   **text**      -> text       (strip bold markers)
#   > quote       -> quote      (strip blockquote markers)

BEGIN {
    MAX_COL = 40
    in_appendix = 0
    in_entry = 0
    in_code_fence = 0
    entry_count = 0
    saw_badge = 0
    state = ""
}

# Track code fences
/^```/ {
    in_code_fence = !in_code_fence
    if (in_entry && state == "desc") {
        flush_entry()
    }
    next
}

in_code_fence { next }

# Section-level heading
/^# / {
    if (in_entry) flush_entry()
    if (index($0, "# Appendix") > 0) {
        in_appendix = 1
    }
    next
}

# Section divider
/^===/ {
    if (in_entry) flush_entry()
    next
}

# Level-2 heading — potential primitive entry
/^## / {
    if (in_entry) flush_entry()
    if (in_appendix) next

    line = $0
    sub(/^## */, "", line)
    sub(/ *$/, "", line)

    # Extract name and optional alias from parentheses
    primary = ""
    alias = ""
    if (match(line, / *\([^)]+\) *$/)) {
        # Has parenthesized alias
        alias_part = substr(line, RSTART, RLENGTH)
        primary = substr(line, 1, RSTART - 1)
        sub(/ *$/, "", primary)
        # Extract alias from parens
        sub(/^ *\(/, "", alias_part)
        sub(/\) *$/, "", alias_part)
        alias = alias_part
    } else {
        primary = line
    }

    in_entry = 1
    saw_badge = 0
    state = "sig"
    cur_primary = primary
    cur_alias = alias
    cur_badge = ""
    sig_count = 0
    desc_text = ""
    next
}

# Blank line handling
/^[[:space:]]*$/ {
    if (!in_entry) next
    if (state == "sig") next
    if (state == "badge") {
        state = "desc"
        next
    }
    if (state == "desc") {
        if (desc_text != "") desc_text = desc_text "\n\n"
        next
    }
    next
}

# Badge line
in_entry && state == "sig" && /^`(command|operation)`[[:space:]]*$/ {
    cur_badge = $0
    sub(/^`/, "", cur_badge)
    sub(/`[[:space:]]*$/, "", cur_badge)
    saw_badge = 1
    state = "badge"
    next
}

# Example block starts
in_entry && state == "desc" && /^\*\*[Ee]xamples?\*\*/ {
    flush_entry()
    next
}

in_entry && state == "desc" && /^[Ee]xamples?:/ {
    flush_entry()
    next
}

# Signature lines
in_entry && state == "sig" {
    sig_count++
    sigs[sig_count] = $0
    next
}

# Description lines
in_entry && state == "desc" {
    if (desc_text != "" && desc_text !~ /\n\n$/) {
        desc_text = desc_text " " $0
    } else {
        desc_text = desc_text $0
    }
    next
}

END {
    if (in_entry) flush_entry()
    emit_c_file()
}

function flush_entry(    i, s, d) {
    if (in_entry && saw_badge) {
        entry_count++
        entries_primary[entry_count] = cur_primary
        entries_alias[entry_count] = cur_alias
        entries_badge[entry_count] = cur_badge

        sigtext = ""
        for (i = 1; i <= sig_count; i++) {
            s = sigs[i]
            sub(/ *$/, "", s)
            s = md_to_logo(s)
            if (sigtext != "") sigtext = sigtext "\n"
            sigtext = sigtext s
        }
        entries_sig[entry_count] = sigtext

        d = md_to_logo(desc_text)
        gsub(/  +/, " ", d)
        # Trim trailing paragraph breaks
        sub(/(\n\n)+$/, "", d)
        entries_desc[entry_count] = d
    }

    in_entry = 0
    saw_badge = 0
    state = ""
    cur_primary = ""
    cur_alias = ""
    cur_badge = ""
    sig_count = 0
    desc_text = ""
    delete sigs
}

function md_to_logo(s,    tmp, before, after) {
    # Strip blockquote markers at start of line
    gsub(/^> ?/, "", s)

    # [`text`](#anchor) -> text
    while (match(s, /\[`[^`]*`\]\(#[^)]*\)/)) {
        before = substr(s, 1, RSTART - 1)
        tmp = substr(s, RSTART, RLENGTH)
        after = substr(s, RSTART + RLENGTH)
        sub(/^\[`/, "", tmp)
        sub(/`\]\(#[^)]*\)/, "", tmp)
        s = before tmp after
    }

    # [text](#anchor) -> text
    while (match(s, /\[[^\]]*\]\(#[^)]*\)/)) {
        before = substr(s, 1, RSTART - 1)
        tmp = substr(s, RSTART, RLENGTH)
        after = substr(s, RSTART + RLENGTH)
        sub(/^\[/, "", tmp)
        sub(/\]\(#[^)]*\)/, "", tmp)
        s = before tmp after
    }

    # [text](url) (external links) -> text
    while (match(s, /\[[^\]]*\]\([^)]*\)/)) {
        before = substr(s, 1, RSTART - 1)
        tmp = substr(s, RSTART, RLENGTH)
        after = substr(s, RSTART + RLENGTH)
        sub(/^\[/, "", tmp)
        sub(/\]\([^)]*\)/, "", tmp)
        s = before tmp after
    }

    # _word_ -> :word (italic parameters -> Logo variable refs)
    while (match(s, /_[a-zA-Z][a-zA-Z0-9]*_/)) {
        before = substr(s, 1, RSTART - 1)
        tmp = substr(s, RSTART, RLENGTH)
        after = substr(s, RSTART + RLENGTH)
        sub(/^_/, "", tmp)
        sub(/_$/, "", tmp)
        s = before ":" tmp after
    }

    # `code` -> code (strip backticks)
    gsub(/`/, "", s)

    # **text** -> text (strip bold)
    gsub(/\*\*/, "", s)

    return s
}

function emit_c_file(    i, j, n, varname, nnames, key_name, key_var) {
    print "// Auto-generated from Pico_Logo_Reference.md -- do not edit"
    print "#include \"core/help.h\""
    print ""

    # Emit string constants for each entry
    for (i = 1; i <= entry_count; i++) {
        varname = c_varname(entries_primary[i])
        printf "static const char %s[] =\n", varname

        n = split(entries_sig[i], siglines, "\n")
        for (j = 1; j <= n; j++) {
            printf "    \"%s\\n\"\n", c_escape(siglines[j])
        }

        printf "    \"(%s)\\n\"\n", entries_badge[i]
        printf "    \"\\n\"\n"

        wrap_and_emit(entries_desc[i])

        printf ";\n\n"
    }

    # Build sorted entries array
    nnames = 0
    for (i = 1; i <= entry_count; i++) {
        nnames++
        all_names[nnames] = tolower(entries_primary[i])
        all_varnames[nnames] = c_varname(entries_primary[i])
        if (entries_alias[i] != "") {
            nnames++
            all_names[nnames] = tolower(entries_alias[i])
            all_varnames[nnames] = c_varname(entries_primary[i])
        }
    }

    # Insertion sort by name
    for (i = 2; i <= nnames; i++) {
        key_name = all_names[i]
        key_var = all_varnames[i]
        j = i - 1
        while (j >= 1 && all_names[j] > key_name) {
            all_names[j + 1] = all_names[j]
            all_varnames[j + 1] = all_varnames[j]
            j--
        }
        all_names[j + 1] = key_name
        all_varnames[j + 1] = key_var
    }

    # Emit sorted array
    print "const HelpEntry help_entries[] = {"
    for (i = 1; i <= nnames; i++) {
        printf "    { \"%s\", %s },\n", c_escape(all_names[i]), all_varnames[i]
    }
    print "};"
    print ""
    printf "const int help_entry_count = %d;\n", nnames
}

function wrap_and_emit(text,    np, p, nw, i, line, word) {
    np = split(text, paragraphs, "\n\n")

    for (p = 1; p <= np; p++) {
        if (p > 1) {
            printf "    \"\\n\"\n"
        }
        nw = split(paragraphs[p], words, /[ \t]+/)
        line = ""
        for (i = 1; i <= nw; i++) {
            word = words[i]
            if (word == "") continue
            if (line == "") {
                line = word
            } else if (length(line) + 1 + length(word) <= MAX_COL) {
                line = line " " word
            } else {
                printf "    \"%s\\n\"\n", c_escape(line)
                line = word
            }
        }
        if (line != "") {
            printf "    \"%s\\n\"\n", c_escape(line)
        }
    }
}

function c_varname(name,    v) {
    v = "help_" name
    gsub(/[^a-zA-Z0-9_]/, "_", v)
    gsub(/__+/, "_", v)
    return v
}

function c_escape(s) {
    gsub(/\\/, "\\\\", s)
    gsub(/"/, "\\\"", s)
    return s
}
