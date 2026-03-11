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
    "000000", "555555", "AAAAAA", "FFFFFF", "18181E", "EA4747", "FF9650", "FFD250",
    "FFF064", "82F082", "50C850", "50D2FF", "E664FF", "EA47C1", "FFDC14", "14B4FF",
]

hue_order = ["Red", "Orange", "Amber", "Yellow", "Lime", "Green", "Emerald", "Teal",
             "Cyan", "Sky", "Blue", "Indigo", "Violet", "Purple", "Fuchsia", "Pink",
             "Rose", "Slate", "Neutral", "Stone"]

tailwind = {
    "Red":     ["7f1d1d", "991b1b", "b91c1c", "dc2626", "ef4444", "f87171", "fca5a5", "fecaca"],
    "Orange":  ["7c2d12", "9a3412", "c2410c", "ea580c", "f97316", "fb923c", "fdba74", "fed7aa"],
    "Amber":   ["78350f", "92400e", "b45309", "d97706", "f59e0b", "fbbf24", "fcd34d", "fde68a"],
    "Yellow":  ["713f12", "854d0e", "a16207", "ca8a04", "eab308", "facc15", "fde047", "fef08a"],
    "Lime":    ["365314", "3f6212", "4d7c0f", "65a30d", "84cc16", "a3e635", "bef264", "d9f99d"],
    "Green":   ["14532d", "166534", "15803d", "16a34a", "22c55e", "4ade80", "86efac", "bbf7d0"],
    "Emerald": ["064e3b", "065f46", "047857", "059669", "10b981", "34d399", "6ee7b7", "a7f3d0"],
    "Teal":    ["134e4a", "115e59", "0f766e", "0d9488", "14b8a6", "2dd4bf", "5eead4", "99f6e4"],
    "Cyan":    ["164e63", "155e75", "0e7490", "0891b2", "06b6d4", "22d3ee", "67e8f9", "a5f3fc"],
    "Sky":     ["0c4a6e", "075985", "0369a1", "0284c7", "0ea5e9", "38bdf8", "7dd3fc", "bae6fd"],
    "Blue":    ["1e3a8a", "1e40af", "1d4ed8", "2563eb", "3b82f6", "60a5fa", "93c5fd", "bfdbfe"],
    "Indigo":  ["312e81", "3730a3", "4338ca", "4f46e5", "6366f1", "818cf8", "a5b4fc", "c7d2fe"],
    "Violet":  ["4c1d95", "5b21b6", "6d28d9", "7c3aed", "8b5cf6", "a78bfa", "c4b5fd", "ddd6fe"],
    "Purple":  ["581c87", "6b21a8", "7e22ce", "9333ea", "a855f7", "c084fc", "d8b4fe", "e9d5ff"],
    "Fuchsia": ["701a75", "86198f", "a21caf", "c026d3", "d946ef", "e879f9", "f0abfc", "f5d0fe"],
    "Pink":    ["831843", "9d174d", "be185d", "db2777", "ec4899", "f472b6", "f9a8d4", "fbcfe8"],
    "Rose":    ["881337", "9f1239", "be123c", "e11d48", "f43f5e", "fb7185", "fda4af", "fecdd3"],
    "Slate":   ["0f172a", "1e293b", "334155", "475569", "64748b", "94a3b8", "cbd5e1", "e2e8f0"],
    "Neutral": ["171717", "262626", "404040", "525252", "737373", "a3a3a3", "d4d4d4", "e5e5e5"],
    "Stone":   ["1c1917", "292524", "44403c", "57534e", "78716c", "a8a29e", "d6d3d1", "e7e5e4"],
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
