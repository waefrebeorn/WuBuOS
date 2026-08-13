#!/usr/bin/env python3
# Brute-force search for the LZX stream for instmsiw.exe inside CAB1.CAB.
# Filter: the FIRST output block should contain "MZ" (EXE header) since
# instmsiw.exe is an x86 EXE. We decode 4KB from each candidate LZX stream
# and check for "MZ" near the start.
#
# To avoid the crash from invalid streams, we use a guarded LZX decoder.
# Rather than reimplement fully here, let's find the stream the SIMPLE way:
# LZX streams in CAB are preceded by a 2-byte or 24-bit "LZX" magic? No.
# 
# Better: the CFHEADER says cbCabinet=136812682 at cab+8. The CFDATA blocks
# are referenced by CFFOLDER.fblock/cblocks. With cFolders=0 the folders
# aren't in the header. But 7z parsed 3 blocks. 
#
# NEW APPROACH: extract the raw CAB1.CAB bytes and parse THEM with a standard
# CAB parser. The cab starts at cab=550324, size 136812682.
import struct
exe = open('/home/wubu/wubunos/vendor/games/halo_pc_trial_setup.exe','rb').read()
cab = 550324
cbCab = struct.unpack('<I', exe[cab+8:cab+12])[0]
print(f"cbCabinet={cbCab}, cab at {cab}, ends at {cab+cbCab}, exe size {len(exe)}")
# Extract raw cab bytes
cab_bytes = exe[cab:cab+cbCab]
open('/tmp/CAB1.CAB','wb').write(cab_bytes)
print("wrote /tmp/CAB1.CAB", len(cab_bytes), "bytes")
# Now parse /tmp/CAB1.CAB with a minimal CAB parser.
# The CFHEADER: cFolders=0, cFiles=259, flags=3, coffFiles=0, setID=83.
# flags=3 -> PREV_CAB(0x1)|NEXT_CAB(0x2) per the spec? OR reserve bits?
# Per MS-CAB spec: flags bit0=PREV (has prev cab), bit1=NEXT. 
# But bit 0x04=cbCFHeaderRes, 0x08=cbCFFolderRes, 0x10=cbCFDataRes.
# flags=0x03 -> PREV+NEXT chain, NO reserve present. 
# coffFiles=0 in a chained cab is legal: means "file section begins right
# after... actually coffFiles=0 points to start of cab which is wrong.
#
# Let me look at it differently. With cFolders=0 and cFiles=259:
# maybe cFiles is REALLY 83 (setID=83 was misread). Let me recompute field
# layout if the header is a DIFFERENT structure. What if Inno uses:
# +0 sig +4 rsv1 +8 cbCabinet +12 coffFiles +16 reserved2 +20 vMaj +21 vMin
# +22 cFolders +24 cFiles +26 flags +28 setID +30 iCabinet
# That's what I have. cFiles=259, setID=83. Both can be true (cFiles counts
# something 7z doesn't, OR cFiles is wrong for Inno).
# 7z clearly reads 83 files though. The discrepancy doesn't matter for my
# LZX test: I just need the compressed stream bytes.
#
# Let me find CFDATA blocks by scanning for the pattern: a 4-byte cbData
# followed by 4-byte cbUnc, where cbUnc matches a known file size.
# instmsiw.exe = 1822848, instmsia.exe = 1709160, halo.exe = 2785280.
targets = {1822848:"instmsiw", 1709160:"instmsia", 2785280:"halo_exe",
           1024:"Strings.dll", 4734976:"SETUPENU.DLL"}
print("\n=== scan cab for CFDATA headers (cbData, cbUnc pairs) ===")
for off in range(44, len(cab_bytes)-8, 1):
    cbd = struct.unpack('<I', cab_bytes[off:off+4])[0]
    cbu = struct.unpack('<I', cab_bytes[off+4:off+8])[0]
    if cbu in targets and 100 < cbd < 7000000 and cbd != cbu:
        # plausibly a CFDATA block: cbData (compressed) + cbUnc (uncompressed)
        name = targets[cbu]
        # the compressed stream starts at off+8
        stream_off = cab + off + 8
        print(f"  cab+{off}: cbData={cbd} cbUnc={cbu} ({name}) -> LZX stream at file+0x{stream_off:x}")
        # show first 16 bytes of the LZX stream
        print(f"    stream bytes: {cab_bytes[off+8:off+24].hex()}")
