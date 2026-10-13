#!/bin/zsh
# Variant of the new build that logs (to stderr = the app's log) the first calls of the four
# `cos` callers (0x2443D0, 0x244538, 0x2446A8, 0x244800), of cos itself (0x274810) and of the
# two scenes that call them (0x2455C0, 0x246250), by patching the generated sources after the
# recompiler ran (a wrapper around ps2_recomp).
# usage: mkprobe.sh <new .app> <out dir>
NEW=$1; OUT=$2
RES=Contents/Resources
APP=$OUT/probe.app
rm -rf $APP; cp -R $NEW $APP
mv $APP/$RES/recomp/ps2_recomp $APP/$RES/recomp/ps2_recomp.real
cat > $APP/$RES/recomp/ps2_recomp <<'EOF'
#!/bin/zsh
HERE=${0:A:h}
$HERE/ps2_recomp.real "$@" || exit $?
GEN=${1:A:h}/generated
for a in 2443d0 244538 2446a8 244800 274810 2455c0 246250; do
  f=$(ls $GEN/sub_00${a:u}_0x$a.cpp 2>/dev/null | head -1)
  [[ -z $f ]] && continue
  sed -E -i '' 's|^(void sub_[0-9A-F]+_0x[0-9a-f]+\(.*\) \{)$|\1 { static int n_; if (n_++ < 2) fprintf(stderr, "[cos-probe] 0x'$a' call %d\\n", n_); }|' $f
done
  # what the callers see after `cos` returns: it answers in v0/v1 (soft-float double); $f0 must be
  # what the caller had put there (0.0f from the mtc1 at its entry), not cosf($f12)
  sed -E -i '' 's|^label_244428:$|label_244428: { static int m_; if (m_++ < 3) fprintf(stderr, "[cos-probe] 0x2443d0 after cos: f0=%08x f12=%08x\\n", *(unsigned *)\&ctx->f[0], *(unsigned *)\&ctx->f[12]); }|' $GEN/sub_002443D0_0x2443d0.cpp
exit 0
EOF
chmod +x $APP/$RES/recomp/ps2_recomp
echo "cos-probe2" > $APP/$RES/sdk/build_id
codesign --force --deep --sign - $APP >/dev/null 2>&1
echo made $APP
