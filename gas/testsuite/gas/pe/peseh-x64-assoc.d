#source: peseh-x64-4.s
#objdump: -t -j .xdata\$_ZN5VBase1fEv -j .pdata\$_ZN5VBase1fEv
#name: PE x64 SEH associative COMDAT sections

.*: +file format pe-x86-64

SYMBOL TABLE:
\[.*\]\(sec +5\).*\.xdata\$_ZN5VBase1fEv
AUX scnlen 0x8 nreloc 0 nlnno 0 checksum 0x0 assoc 4 comdat 5
\[.*\]\(sec +6\).*\.pdata\$_ZN5VBase1fEv
AUX scnlen 0xc nreloc 3 nlnno 0 checksum 0x0 assoc 4 comdat 5
