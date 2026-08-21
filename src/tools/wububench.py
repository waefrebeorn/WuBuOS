#!/usr/bin/env python3
"""WuBuBench - our own hyperfine-style benchmark harness (C11 spirit, pure stdlib).
Runs each command N warmup + M timed runs, reports min/median/max in ms with
IQR-based outlier rejection, like hyperfine. No third-party deps.
"""
import subprocess, statistics, sys, time, os

def bench(cmd, n=20, warmup=3):
    # warmup
    for _ in range(warmup):
        subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    times=[]
    for _ in range(n):
        t=time.perf_counter()
        r=subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        times.append((time.perf_counter()-t)*1000.0)
        if r.returncode not in (0,1):  # tolerate "no match" rc=1
            pass
    # IQR outlier rejection (hyperfine-style)
    s=sorted(times); q1=s[len(s)//4]; q3=s[3*len(s)//4]; iqr=q3-q1
    lo=q1-1.5*iqr; hi=q3+1.5*iqr
    filtered=[x for x in s if lo<=x<=hi] or s
    return {
        'min':min(filtered),'med':statistics.median(filtered),
        'max':max(filtered),'mean':statistics.mean(filtered),'n':len(filtered)
    }

def fmt(d):
    return f"min {d['min']:7.1f}ms  med {d['med']:7.1f}ms  max {d['max']:7.1f}ms"

def main():
    corpus=sys.argv[1] if len(sys.argv)>1 else "/tmp/corpus.txt"
    print(f"# WuBuBench: cold-ish timings, 20 runs, IQR-cleaned  ({corpus}, {os.path.getsize(corpus)//1024//1024}MB)\n")
    workloads=[
        ("-F literal 'error'",   lambda b: [b,"-F","error",corpus]),
        ("-E NFA  'code [0-9]+'", lambda b: [b,"-E","code [0-9]+",corpus]),
        ("-E 'fox.*dog'",          lambda b: [b,"-E","fox.*dog",corpus]),
        ("-G BRE 'error.*code'",   lambda b: [b,"-G","error.*code",corpus]),
        ("-c count 'error'",       lambda b: [b,"-c","error",corpus]),
    ]
    engines={
        "wubugrep":"./wubugrep",
        "ripgrep":"/usr/bin/rg",
        "grep":"/usr/bin/grep",
    }
    for label,mk in workloads:
        print(f"## {label}")
        results={}
        for name,b in engines.items():
            try:
                cmd=mk(b)
                results[name]=bench(cmd)
            except Exception as e:
                results[name]=f"ERR {e}"
        # show, and compute speedup vs ripgrep
        for name in ["wubugrep","ripgrep","grep"]:
            if name in results and isinstance(results[name],dict):
                sp=""
                if isinstance(results.get("ripgrep"),dict):
                    sp=f"  ({results['ripgrep']['med']/results[name]['med']:.2f}x vs rg)"
                print(f"  {name:10s}: {fmt(results[name])}{sp}")
        print()

if __name__=="__main__":
    main()
