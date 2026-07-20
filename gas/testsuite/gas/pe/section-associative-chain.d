#objdump: -t -j .middle -j .leaf
#name: PE associative COMDAT chain

.*: +file format .*

SYMBOL TABLE:
\[.*\]\(sec +4\).*\.middle
AUX scnlen 0x4 nreloc 0 nlnno 0 checksum 0x0 assoc 6 comdat 5
\[.*\]\(sec +5\).*\.leaf
AUX scnlen 0x4 nreloc 0 nlnno 0 checksum 0x0 assoc 4 comdat 5
.* middle
