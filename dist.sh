#!/bin/sh
set -e

PRESETS="pico pico2 pico2w"

mkdir -p dist

for preset in $PRESETS; do
    echo "=== Building $preset ==="
    cmake --preset="$preset"
    cmake --build --preset="$preset"
    cp "build-$preset/pico-logo.uf2" "dist/logo-$preset.uf2"
    echo "=== Built dist/logo-$preset.uf2 ==="
done

echo "=== Creating logo.zip ==="
(cd logo && zip -r ../dist/logo.zip .)
echo "=== Created dist/logo.zip ==="

echo "=== Generating Pico_Logo_Reference.pdf ==="
rsvg-convert -f pdf -o dist/Colours.pdf reference/Colours.svg
cat > dist/_header.tex <<'EOF'
\usepackage{fancyhdr}
\pagestyle{fancy}
\fancyhf{}
\fancyhead[L]{\leftmark}
\fancyhead[R]{\thepage}
\renewcommand{\headrulewidth}{0.4pt}
EOF
sed -e '/^# Contents$/d' \
    -e 's/^{{TOC}}$/\\tableofcontents/' \
    -e 's/^===$/\\newpage/' \
    -e 's|\./Colours\.svg|../dist/Colours.pdf|' \
    reference/Pico_Logo_Reference.md | \
    pandoc -f markdown -o dist/Pico_Logo_Reference.pdf \
    --resource-path=reference \
    --pdf-engine=xelatex \
    -V fontsize=12pt \
    -V geometry:margin=1in \
    -V mainfont="Helvetica" \
    -V monofont="Menlo" \
    -H dist/_header.tex \
    --syntax-highlighting=tango
rm -f dist/Colours.pdf dist/_header.tex
echo "=== Generated dist/Pico_Logo_Reference.pdf ==="

echo ""
echo "All builds complete:"
ls -lh dist/logo-*.uf2 dist/logo.zip dist/Pico_Logo_Reference.pdf
