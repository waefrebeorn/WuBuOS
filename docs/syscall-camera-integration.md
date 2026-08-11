# Syscall Camera + AGI Interception Layer

## The Gap
Wine/proton runs **ON** the kernel, but the AGI **never sees** what they do.  
`ct_iso_seccomp.c` is a gate (ALLOW/KILL), not a camera.  
`wubu_trace.c` is a ledger with no syscalls written into it.

## The Solution: wubu_secmon

The **syscall camera** (`src/kernel/wubu_secmon.c`) intercepts every syscall
a game process makes and streams it into the KV-FS at `/kv/agent/sys_<pid>/<seq>`.

### Vector Layout (6 floats per syscall):
```
[0] = kind  (0=enter, 1=exit)
[1] = syscall nr
[2] = arg0
[3] = arg1  
[4] = retval (exit) or arg2 (enter)
[5] = pid
```

### Integration Points

1. **Container isolation** → calls `wubu_secmon_attach(pid)` for PID namespace children
2. **KV-FS mount** → `/kv/agent` at blocks 2048..4095 (see `wubu_kvfs_namespace_init`)
3. **9P bridge** → `/n/kv/agent/sys_*` exposed to the Brain over 9P
4. **World state** → `wubu_agi_play_learn()` writes at `/kv/world/tick_N`

## Usage

From the container setup code (in `wubu_ct_child_isolation()`):

```c
wubu_secmon_t *cam = wubu_secmon_create(0);
wubu_secmon_attach(cam, child_pid);
```

Then service the camera from the host thread or use `wubu_secmon_wait()` for
blocking capture.

## Test Strategy

The selftest (`wubu_secmon_selftest.c`) verifies:
1. KV-FS initialization
2. `fork()` + `ptrace()` attach
3. Syscall capture and KV-FS write
4. Reading back the span via `wubu_kvfs_read()`

To run: compile with kernel infrastructure:
```bash
gcc -O0 -g -std=c11 -I./src/kernel \
    src/kernel/wubu_secmon_selftest.c \
    src/kernel/wubu_secmon.c src/kernel/wubu_kvfs.c \
    src/kernel/libc.c src/kernel/klog.c \
    src/kernel/wubu_serial.c src/kernel/memory.c \
    -o test_secmon
# Requires: -lrt, CAP_SYS_PTRACE capability
```

## Future Work

- Wire into `wubu_ct_steamos` for automatic camera attachment on Proton launches
- Add syscall filtering (only show file/network syscalls relevant to behavior)
- Batch spans in KV-FS for efficiency (current: 1 span = 6 floats = 24 bytes)