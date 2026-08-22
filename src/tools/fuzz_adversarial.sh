#!/bin/bash
# Adversarial fuzz: malformed + rare-literal patterns vs GNU grep, 3 corpora
cd /home/wubu/wubuos/src/tools
fails=0; total=0
for corpus in /tmp/craft.txt /tmp/craft2.txt /tmp/craft3.txt /tmp/c3.txt; do
  while read -r pat; do
    total=$((total+1))
    a=$(./wubugrep -nE "$pat" "$corpus" 2>/dev/null; echo rc$?)
    b=$(grep -nE "$pat" "$corpus" 2>/dev/null; echo rc$?)
    if [ "$a" != "$b" ]; then fails=$((fails+1)); echo "FUZZ FAIL [$pat] on $corpus"; fi
  done <<'PATS'
)
a)
a))
qfm|*
*uce|
|thddklxjwr
d)|t
)a
(|a
a|*
*
*a
**
zzzqqq
the.*zzzqqq
zzzqqq|nothing
[t-z]he
^zzzqqq
deep.*here$
[[:digit:]]
[[:alpha:]]zz
a[]b]c
a[^]b]c
a[[.x.]]
[[:upper:]][[:lower:]]
PATS
done
echo "adversarial: total=$total fails=$fails"
