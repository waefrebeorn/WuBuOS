
### vendored: miniz (richgel999/miniz, MIT / public domain (dual))
- `third_party/tiny/miniz/miniz.c` — 26024 bytes, sha256 `29a0c08c4610d20ab18381cbf18190f91b4b41d05c242376364756061b631f1d`, fills: archive, office, files
- `third_party/tiny/miniz/miniz.h` — 29216 bytes, sha256 `2b9c48355793988f0d12b3cef357e52b741d744385157b59cb422237ea226501`, fills: archive, office, files

### vendored: PDMP3 Huffman codebooks (technosaurus/PDMP3, public domain / Unlicense)
- `third_party/mp3/pdmp3_huffman.txt` — 15939 bytes, sha256 `1338d99a3b95928f52a5be83a2d9372c09512d5a9bca4f1601ed76c6ea6fb932`, fills: audio
- Data only: the Layer III codebook tree extracted from pdmp3.c (sha256 pinned in the file).
  No PDMP3 code is compiled; the walker is `src/runtime/wubu_mp3_huff.c`.
