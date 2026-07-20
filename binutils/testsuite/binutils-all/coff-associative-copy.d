#PROG: objcopy
#objcopy:
#objdump: -t -j .assoc
#name: objcopy preserves PE associative COMDAT
#source: coff-associative-copy.s
#target: [is_pecoff_format]

.*: +file format .*

SYMBOL TABLE:
\[.*\]\(sec +4\).*\.assoc
AUX scnlen 0x4 nreloc 0 nlnno 0 checksum 0x0 assoc 5 comdat 5
