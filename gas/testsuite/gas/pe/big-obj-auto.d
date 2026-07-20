#objdump: -t -j .assoc
#name: PE big obj auto-promotion

.*: *file format pe-bigobj-.*

SYMBOL TABLE:
\[.*\]\(sec +80004\).*\.assoc
AUX scnlen 0x4 nreloc 0 nlnno 0 checksum 0x0 assoc 80005 comdat 5
