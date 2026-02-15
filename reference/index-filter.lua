-- Pandoc Lua filter to generate an index from H2 headers
-- Collects all H2 header names, indexes them at their definition,
-- and indexes mentions of those names throughout the body text.
-- Appends \printindex at the end of the document.

-- Table of H2 header names to index.
-- Each entry maps a lowercase search term to the index display name.
local index_terms = {}

-- First pass: collect all H2 header texts
function collect_headers(doc)
  for _, block in ipairs(doc.blocks) do
    if block.t == "Header" and block.level == 2 then
      local text = pandoc.utils.stringify(block)
      -- Extract main name and abbreviation, e.g. "forward (fd)"
      local main, abbr = text:match("^(.-)%s+%((.-)%)%s*$")
      if main then
        -- Index both the full name and abbreviation
        index_terms[main:lower()] = main
        index_terms[abbr:lower()] = abbr
      else
        -- No abbreviation, just use the full name
        local name = text:match("^(.-)%s*$")
        if name and #name > 0 then
          index_terms[name:lower()] = name
        end
      end
    end
  end
end

-- Check if a word boundary exists at position in string
local function is_word_boundary(str, pos)
  if pos < 1 or pos > #str then return true end
  local ch = str:sub(pos, pos)
  return not ch:match("[%w_%.%?]")
end

-- Add \index{} entries for mentions of H2 terms in a Str element
function index_in_str(el)
  local text = el.text
  local lower_text = text:lower()
  local result = {}
  local found = false

  for term, display in pairs(index_terms) do
    local start = 1
    while true do
      local i, j = lower_text:find(term, start, true)
      if not i then break end
      -- Check word boundaries
      if is_word_boundary(lower_text, i - 1) and is_word_boundary(lower_text, j + 1) then
        found = true
        break
      end
      start = j + 1
    end
    if found then break end
  end

  if not found then return nil end

  -- For matched terms, add index entries as RawInline after the element
  local inlines = { el }
  for term, display in pairs(index_terms) do
    local i, j = lower_text:find(term, 1, true)
    if i and is_word_boundary(lower_text, i - 1) and is_word_boundary(lower_text, j + 1) then
      table.insert(inlines, pandoc.RawInline("latex", "\\index{" .. display .. "}"))
    end
  end

  return inlines
end

-- Add \index{} right after H2 headers
function index_at_header(el)
  if el.t == "Header" and el.level == 2 then
    local text = pandoc.utils.stringify(el)
    local main, abbr = text:match("^(.-)%s+%((.-)%)%s*$")
    local index_raw
    if main then
      index_raw = pandoc.RawInline("latex",
        "\\index{" .. main .. "}" ..
        "\\index{" .. abbr .. "}")
    else
      local name = text:match("^(.-)%s*$")
      if name and #name > 0 then
        index_raw = pandoc.RawInline("latex", "\\index{" .. name .. "}")
      end
    end
    if index_raw then
      el.content:insert(index_raw)
    end
    return el
  end
end

-- Append \printindex at end of document
function add_printindex(doc)
  table.insert(doc.blocks, pandoc.RawBlock("latex", "\\printindex"))
  return doc
end

-- Process Str elements in non-header blocks to add index entries
function process_inlines(el)
  if el.t == "Header" then return nil end -- skip headers, handled separately

  local dominated_by_header = false
  return el:walk({
    Str = index_in_str
  })
end

return {
  -- First traverse: collect H2 names
  { Pandoc = collect_headers },
  -- Second traverse: add index entries at headers
  { Header = index_at_header },
  -- Third traverse: add index entries for mentions in body text
  { Block = process_inlines },
  -- Fourth traverse: append \printindex
  { Pandoc = add_printindex }
}
