#!/bin/zsh
# Make variant app bundles from the new (cpu-accuracy) and old (pre-accuracy) builds, each
# differing from the new build in exactly one accuracy change, to attribute a divergence.
# usage: mkvariants.sh <new .app> <old .app> <out dir>
NEW=$1; OLD=$2; OUT=$3
mkdir -p $OUT
RES=Contents/Resources

# libm: the six libm host stubs back (old toml: stubs, not entry points)
rm -rf $OUT/libm.app; cp -R $NEW $OUT/libm.app
cp $OLD/$RES/recomp/roadtrip.toml $OUT/libm.app/$RES/recomp/roadtrip.toml
echo "libm-stubs" > $OUT/libm.app/$RES/sdk/build_id

# branches: the old recompiler (bit-31 branch tests, no orphan entry points), new toml
rm -rf $OUT/br.app; cp -R $NEW $OUT/br.app
cp $OLD/$RES/recomp/ps2_recomp $OUT/br.app/$RES/recomp/ps2_recomp
echo "old-branches" > $OUT/br.app/$RES/sdk/build_id

# fma: clang's default fp contraction back in the game's compiled code
rm -rf $OUT/fma.app; cp -R $NEW $OUT/fma.app
python3 - $OUT/fma.app/$RES/sdk/include/0/ps2_runtime_macros.h <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read()
t = s.replace("#pragma clang fp contract(off)", "// (contraction left on for the A/B)")
assert s != t
open(p, "w").write(t)
EOF
echo "fma-on" > $OUT/fma.app/$RES/sdk/build_id

# float mode off is an environment switch (RT_EE_FLOAT_MODE=0) on the new build: same bundle.
for v in libm br fma; do codesign --force --deep --sign - $OUT/$v.app >/dev/null 2>&1; done
ls $OUT
