#!/usr/bin/env bash




set -e

cd "$(dirname "$0")/.."

ORTHO_SIZES="20 40 60 80 100 200 300 400 500 600 700 800 900 1000 1250 1500 1750 2000 2250 2500"

mkdir -p instances/sample

# orthorand 
for n in $ORTHO_SIZES; do
  f=$(ls instances/agp2009a-orthorand/random-${n}-*.pol 2>/dev/null | shuf -n1)
  [ -n "$f" ] && cp -v "$f" instances/sample/
done

# simplerand
for n in $ORTHO_SIZES; do
  f=$(ls instances/agp2009a-simplerand/randsimple-${n}-*.pol 2>/dev/null | shuf -n1)
  [ -n "$f" ] && cp -v "$f" instances/sample/
done

# rvk
RVK_SIZES=$(ls instances/agp2009a-rvk/randvon-*.pol 2>/dev/null \
  | xargs -n1 basename \
  | sed "s/randvon-\([0-9]*\)-.*/\1/" \
  | sort -nu)

for n in $RVK_SIZES; do
  f=$(ls instances/agp2009a-rvk/randvon-${n}-*.pol 2>/dev/null | shuf -n1)
  [ -n "$f" ] && cp -v "$f" instances/sample/
done

# gB
for spec in $(ls instances/gB-simple-simple/*.pol 2>/dev/null \
  | xargs -n1 basename \
  | sed "s/_[0-9][0-9]*\.pol$//" \
  | sort -u); do
  f=$(ls instances/gB-simple-simple/${spec}_*.pol 2>/dev/null | shuf -n1)
  [ -n "$f" ] && cp -v "$f" instances/sample/
done

# gD
for spec in $(ls instances/gD-ortho-ortho/*.pol 2>/dev/null \
  | xargs -n1 basename \
  | sed "s/_[0-9][0-9]*\.pol$//" \
  | sort -u); do
  f=$(ls instances/gD-ortho-ortho/${spec}_*.pol 2>/dev/null | shuf -n1)
  [ -n "$f" ] && cp -v "$f" instances/sample/
done

echo "--- total files ---"
ls instances/sample | wc -l