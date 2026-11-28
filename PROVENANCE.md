
### vendored: miniz (richgel999/miniz, MIT / public domain (dual))
- `third_party/tiny/miniz/miniz.c` — 26024 bytes, sha256 `29a0c08c4610d20ab18381cbf18190f91b4b41d05c242376364756061b631f1d`, fills: archive, office, files
- `third_party/tiny/miniz/miniz.h` — 29216 bytes, sha256 `2b9c48355793988f0d12b3cef357e52b741d744385157b59cb422237ea226501`, fills: archive, office, files

### vendored: PDMP3 Huffman codebooks (technosaurus/PDMP3, public domain / Unlicense)
- `third_party/mp3/pdmp3_huffman.txt` — 15071 bytes, sha256 `6995832858f090544bd5ab2ba1195f5046cc09ede34babb0b7990c537afb7bb2`, fills: audio
- Data only: the Layer III codebook tree extracted from pdmp3.c (sha256 pinned in the file).
  No PDMP3 code is compiled; the walker is `src/runtime/wubu_mp3_huff.c`.
