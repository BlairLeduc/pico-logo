#!/bin/sh
set -e

PRESETS="pico2 pico2w pico+2w"

# Start each dist from a clean slate so artifacts from a previous run (e.g. a
# uf2 for a preset that was since removed) do not linger in the output.
rm -rf dist
mkdir -p dist

for preset in $PRESETS; do
    echo "=== Building $preset ==="
    cmake --preset="$preset"
    cmake --build --preset="$preset"
    cp "build-$preset/pico-logo.uf2" "dist/logo-$preset.uf2"
    echo "=== Built dist/logo-$preset.uf2 ==="
done

echo "=== Building mklfsimg ==="
# Configure the host build fresh. A build-host cache from a previous Xcode SDK
# can pin a now-deleted CMAKE_OSX_SYSROOT and warn on every subsequent run.
rm -rf build-host
cmake --preset=host
cmake --build --preset=host --target mklfsimg
echo "=== Creating logo.img (LittleFS restore image) ==="
./build-host/mklfsimg logo dist/logo.img
echo "=== Created dist/logo.img ==="

echo "=== Generating Pico_Logo_Reference.pdf ==="
rsvg-convert -f pdf -o dist/Colours.pdf reference/Colours.svg
cat > dist/_header.tex <<'EOF'
\usepackage{fancyhdr}
\pagestyle{fancy}
\fancyhf{}
\fancyhead[L]{\leftmark}
\fancyhead[R]{\thepage}
\renewcommand{\headrulewidth}{0.4pt}
\usepackage{makeidx}
\makeindex
\usepackage[normalem]{ulem}
\AtBeginDocument{%
  \let\oldhref\href
  \renewcommand{\href}[2]{\oldhref{#1}{\uline{#2}}}%
}
EOF
sed -e '/^# Contents$/d' \
    -e 's/^{{TOC}}$/\\tableofcontents/' \
    -e 's/^===$/\\newpage/' \
    -e 's|\./Colours\.svg|../dist/Colours.pdf|' \
    reference/Pico_Logo_Reference.md | \
    pandoc -f markdown -o dist/Pico_Logo_Reference.pdf \
    --resource-path=reference \
    --pdf-engine=latexmk \
    --pdf-engine-opt=-xelatex \
    --lua-filter=reference/index-filter.lua \
    -V fontsize=12pt \
    -V geometry:margin=1in \
    -V mainfont="Charter" \
    -V monofont="Iosevka" \
    -V colorlinks=true \
    -V urlcolor=blue \
    -V linkcolor=blue \
    -H dist/_header.tex \
    --syntax-highlighting=tango
rm -f dist/Colours.pdf dist/_header.tex
echo "=== Generated dist/Pico_Logo_Reference.pdf ==="

echo ""
echo "All builds complete:"
ls -lh dist/logo-*.uf2 dist/logo.img dist/Pico_Logo_Reference.pdf
