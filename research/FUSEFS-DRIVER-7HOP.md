# FUSEFS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux FUSE filesystem gaps

FUSE (Filesystem in Userspace) runs filesystem code in userspace via `/dev/fuse`.

### Impl routing (wubu_fusefs.c)

| Impl | Routing |
|------|--------|
| ssh | sshfs |
| ntfs | ntfs-3g |
| mp3 | mp3fs |
| iso | fuseiso |
| enc | encfs |
| bind | bindfs |

### Op routing

| Op | Routing |
|----|--------|
| getattr | getattr |
| readdir | readdir |
| open | open |
| read | read |
| write | write |
| unlink | unlink |
| release | release |
