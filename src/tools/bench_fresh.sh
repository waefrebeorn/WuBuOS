#!/bin/bash
# Fresh full 4-way bench, per-invocation date timing, warm cache, min of 3.
cd /home/wubu/wubuos/src/tools
C=/tmp/c3.txt
W=./wubugrep
time_one() {  # $1=binary $2..=args ; echoes ms
    local t0 t1
    t0=$(date +%s.%N); "$@" $C > /dev/null 2>&1; t1=$(date +%s.%N)
    echo "( $t1 - $t0 ) * 1000" | bc -l
}
best_of3() { local m=999999; for i in 1 2 3; do v=$(time_one "$@"); v=${v%%.*}; [ "$v" -lt "$m" ] && m=$v; done; echo $m; }
UG=/home/wubu/opt/ugrep_study/ugrep/bin/ugrep
printf "%-14s %8s %8s %8s %8s\n" pattern wubugrep grep rg ugrep
for pat in '[a-z]+' 'a+' '[0-9a-f]+' 'a.*b' 'the' 'error' 'the.*dog' '[A-Z]' '[0-9]' 'foo|bar' 'a{2,4}' '(ab)+'; do
    w=$(best_of3 $W -c -E "$pat")
    g=$(best_of3 grep -cE "$pat")
    r=$(best_of3 rg -c -e "$pat")
    u=$(best_of3 $UG -c -e "$pat")
    printf "%-14s %8s %8s %8s %8s\n" "$pat" $w $g $r $u
done
