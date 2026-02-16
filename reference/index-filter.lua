-- Pandoc Lua filter to generate an index from H2 headers.
-- Indexes H2 header definitions and explicit internal link references.
-- Appends \printindex at the end of the document.

-- Set of known H2 anchor IDs mapped to their display name.
-- e.g. { ["forward-fd"] = "forward", ["readlist-rl"] = "readlist" }
local anchor_to_name = {}

-- First pass: collect all H2 header IDs and names.
function collect_headers(doc)
  for _, block in ipairs(doc.blocks) do
    if block.t == "Header" and block.level == 2 then
      local id = block.identifier
      local text = pandoc.utils.stringify(block)
      -- Extract main name, e.g. "forward" from "forward (fd)"
      local main = text:match("^(.-)%s+%(.-%)%s*$") or text:match("^(.-)%s*$")
      if main and #main > 0 and #id > 0 then
        anchor_to_name[id] = main
      end
    end
  end
end

-- Add \index{} right after H2 headers.
function index_at_header(el)
  if el.t == "Header" and el.level == 2 then
    local name = anchor_to_name[el.identifier]
    if name then
      el.content:insert(pandoc.RawInline("latex", "\\index{" .. name .. "}"))
    end
    return el
  end
end

-- Add \index{} at explicit internal links that reference H2 sections.
-- Matches links like [`readlist`](#readlist-rl).
function index_at_link(el)
  if el.t == "Link" then
    local target = el.target
    -- Strip leading '#' to get the anchor ID
    local anchor = target:match("^#(.+)$")
    if anchor then
      local name = anchor_to_name[anchor]
      if name then
        return {
          el,
          pandoc.RawInline("latex", "\\index{" .. name .. "}")
        }
      end
    end
  end
end

-- Append \printindex at end of document.
function add_printindex(doc)
  table.insert(doc.blocks, pandoc.RawBlock("latex", "\\printindex"))
  return doc
end

return {
  -- First traverse: collect H2 names and IDs
  { Pandoc = collect_headers },
  -- Second traverse: add index entries at H2 headers
  { Header = index_at_header },
  -- Third traverse: add index entries at internal links
  { Inline = index_at_link },
  -- Fourth traverse: append \printindex
  { Pandoc = add_printindex }
}
