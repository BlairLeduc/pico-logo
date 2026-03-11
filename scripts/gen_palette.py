#!/usr/bin/env python3
"""Generate palette_24bit and palette_16bit arrays for new palette layout."""

def rgb565_round(r, g, b):
    r5 = min(r + 4, 255) >> 3
    g6 = min(g + 2, 255) >> 2
    b5 = min(b + 4, 255) >> 3
    return (r5 << 11) | (g6 << 5) | b5

def hex_to_rgb(h):
    h = h.lstrip('#')
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))

text_colors = [
    "000000", "3F3F3F", "7E7E7E", "BEBEBE", "FFFFFF", "EA4747", "FF9650", "FFD250",
    "FFF064", "82F082", "50C850", "50D2FF", "E664FF", "EA47C1", "FFDC14", "14B4FF",
]

hue_order = ["Red", "Orange", "Amber", "Yellow", "Lime", "Green", "Emerald", "Teal",
             "Cyan", "Sky", "Blue", "Indigo", "Violet", "Purple", "Fuchsia", "Pink",
             "Rose", "Slate", "Neutral", "Stone"]

tailwind = {
    "Red":     ["fecaca", "fca5a5", "f87171", "ef4444", "dc2626", "b91c1c", "991b1b", "7f1d1d"],
    "Orange":  ["fed7aa", "fdba74", "fb923c", "f97316", "ea580c", "c2410c", "9a3412", "7c2d12"],
    "Amber":   ["fde68a", "fcd34d", "fbbf24", "f59e0b", "d97706", "b45309", "92400e", "78350f"],
    "Yellow":  ["fef08a", "fde047", "facc15", "eab308", "ca8a04", "a16207", "854d0e", "713f12"],
    "Lime":    ["d9f99d", "bef264", "a3e635", "84cc16", "65a30d", "4d7c0f", "3f6212", "365314"],
    "Green":   ["bbf7d0", "86efac", "4ade80", "22c55e", "16a34a", "15803d", "166534", "14532d"],
    "Emerald": ["a7f3d0", "6ee7b7", "34d399", "10b981", "059669", "047857", "065f46", "064e3b"],
    "Teal":    ["99f6e4", "5eead4", "2dd4bf", "14b8a6", "0d9488", "0f766e", "115e59", "134e4a"],
    "Cyan":    ["a5f3fc", "67e8f9", "22d3ee", "06b6d4", "0891b2", "0e7490", "155e75", "164e63"],
    "Sky":     ["bae6fd", "7dd3fc", "38bdf8", "0ea5e9", "0284c7", "0369a1", "075985", "0c4a6e"],
    "Blue":    ["bfdbfe", "93c5fd", "60a5fa", "3b82f6", "2563eb", "1d4ed8", "1e40af", "1e3a8a"],
    "Indigo":  ["c7d2fe", "a5b4fc", "818cf8", "6366f1", "4f46e5", "4338ca", "3730a3", "312e81"],
    "Violet":  ["ddd6fe", "c4b5fd", "a78bfa", "8b5cf6", "7c3aed", "6d28d9", "5b21b6", "4c1d95"],
    "Purple":  ["e9d5ff", "d8b4fe", "c084fc", "a855f7", "9333ea", "7e22ce", "6b21a8", "581c87"],
    "Fuchsia": ["f5d0fe", "f0abfc", "e879f9", "d946ef", "c026d3", "a21caf", "86198f", "701a75"],
    "Pink":    ["fbcfe8", "f9a8d4", "f472b6", "ec4899", "db2777", "be185d", "9d174d", "831843"],
    "Rose":    ["fecdd3", "fda4af", "fb7185", "f43f5e", "e11d48", "be123c", "9f1239", "881337"],
    "Slate":   ["e2e8f0", "cbd5e1", "94a3b8", "64748b", "475569", "334155", "1e293b", "0f172a"],
    "Neutral": ["e5e5e5", "d4d4d4", "a3a3a3", "737373", "525252", "404040", "262626", "171717"],
    "Stone":   ["e7e5e4", "d6d3d1", "a8a29e", "78716c", "57534e", "44403c", "292524", "1c1917"],
}

primaries = ["FF0000", "00FF00", "0000FF", "FFFF00", "00FFFF", "FF00FF", "FFFFFF"]

palette_24 = []
palette_16 = []

for h in text_colors:
    r, g, b = hex_to_rgb(h)
    palette_24.append((r << 16) | (g << 8) | b)
    palette_16.append(rgb565_round(r, g, b))

for hue in hue_order:
    for h in tailwind[hue]:
        r, g, b = hex_to_rgb(h)
        palette_24.append((r << 16) | (g << 8) | b)
        palette_16.append(rgb565_round(r, g, b))

for _ in range(72):
    palette_24.append(0)
    palette_16.append(0)

for h in primaries:
    r, g, b = hex_to_rgb(h)
    palette_24.append((r << 16) | (g << 8) | b)
    palette_16.append(rgb565_round(r, g, b))

palette_24.append(0)
palette_16.append(0)

assert len(palette_24) == 256
assert len(palette_16) == 256

def comment(i):
    if i < 16:
        return f"  // {i:3d}-{i+7:3d}: Text palette"
    elif i < 176:
        return f"  // {i:3d}-{i+7:3d}: {hue_order[(i-16)//8]}"
    elif i < 248:
        return f"  // {i:3d}-{i+7:3d}: Unallocated"
    else:
        return f"  // {i:3d}-{i+7:3d}: R, G, B, Y, C, M, White(FG), Black(BG)"

print("=== palette_24bit ===")
for i in range(0, 256, 8):
    vals = ", ".join(f"0x{v:06X}" for v in palette_24[i:i+8])
    print(f"    {vals},{comment(i)}")

print()
print("=== palette_16bit ===")
for i in range(0, 256, 8):
    vals = ", ".join(f"0x{v:04X}" for v in palette_16[i:i+8])
    print(f"    {vals},{comment(i)}")
