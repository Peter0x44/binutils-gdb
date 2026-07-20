# PE/COFF Associative COMDAT Support in GNU Tools

## Status

This note began as an investigation against binutils commit `75e933360c6a` on
2026-07-20 and now records the implementation in the working tree based on
that checkout.  The final rebuilt tools report:

```text
GNU ld (GNU Binutils) 2.47.50.20260720
```

The rebuild uses w64devkit's external binutils build configured against the
live checkout.  Sections titled "Baseline" describe the behavior before this
work; sections titled "Implemented" describe the current working tree.

The implementation now includes:

* a validated COFF-private parent/child graph in BFD, including associative
  chains and BigObj parent indices;
* parent-driven duplicate selection, `kept_section` redirection, and GC
  traversal from each live parent to its direct children;
* valid association regeneration in direct object output, `ld -r`, and
  objcopy, including filtered/reordered objcopy output;
* distinct relocatable output sections for same-named children owned by
  different surviving parents;
* LLVM-compatible GAS syntax for the `associative` selection kind;
* true associations on GAS-generated `.pdata` and `.xdata` sections; and
* compatibility rooting only for legacy unwind sections that have no real
  association.

LLVM's GNU-COFF producer gate has not been changed.  Archive/LTO/plugin
coverage, raw malformed binary fixtures, and complete selector 6/7 conformance
remain follow-up work.

## Executive Summary

PE/COFF defines `IMAGE_COMDAT_SELECT_ASSOCIATIVE` (selection value 5) for a
section whose selection follows another COMDAT section.  This permits a
compiler to describe one definition across code, data, and metadata sections
without relying on section-name suffixes for ownership.

Before this work, GNU BFD decoded but discarded the relationship.  Duplicate
selection, GC, `ld -r`, and objcopy consequently produced duplicate, missing,
or stale children, while blanket `.pdata`/`.xdata` retention could keep dead
functions alive.

The implementation retains the immediate edge, validates the complete graph,
selects ordinary parents first, redirects losing children to corresponding
children under the prevailing parent, and marks direct children recursively.
Every object-producing path writes the final output parent's section number.
Relocatable links additionally keep different association owners in different
section headers even when their child names are identical.

GAS accepts:

```asm
.section child,"dr",associative,parent_symbol
```

and generated SEH side sections use the same BFD setter.  Existing `$` suffixes
remain for compatibility, but ownership is now carried by the standard COFF
auxiliary record.  LLVM/lld remain the interoperability reference; enabling
LLVM's GNU producer path should wait for a released GNU consumer containing
this support.

## Normative PE/COFF Semantics

An associative COMDAT section is identified by a Section Definition auxiliary
symbol record with:

* `Selection == IMAGE_COMDAT_SELECT_ASSOCIATIVE`;
* `Number` equal to the one-based section-table index of its parent.

The parent must be a COMDAT section.  The Microsoft PE/COFF specification says
that the parent may itself be associative.  Such a chain:

* must not contain a loop; and
* must eventually terminate at a non-associative COMDAT.

The intended semantic rule is simple: the child is linked if and only if its
parent is linked.  This supports definitions with components in multiple
sections while making the components an atomic selection unit.

The `$` suffix in a COFF section name is unrelated to this ownership rule.  It
controls grouping and lexical ordering of contributions in the final image.
For example, `.pdata$foo` suggests a convention but does not encode an edge to
the section containing `foo`.

## Baseline GNU Implementation

This section describes commit `75e933360c6a` before the implementation in the
current working tree.

### Input decoding

[bfd/coffcode.h](bfd/coffcode.h) reads the Section Definition auxiliary record.
Its `IMAGE_COMDAT_SELECT_ASSOCIATIVE` case contains an explicit FIXME.  In a
normal build, rather than `STRICT_PE_FORMAT`, it clears `SEC_LINK_ONCE`:

```c
case IMAGE_COMDAT_SELECT_ASSOCIATIVE:
#ifdef STRICT_PE_FORMAT
  /* FIXME: This is not currently implemented.  */
  sec_flags |= SEC_LINK_DUPLICATES_DISCARD;
#else
  sec_flags &= ~SEC_LINK_ONCE;
#endif
  break;
```

The COFF swap code decodes `x_scn.x_associated`, including the high word used
by BigObj.  Decoding the bytes is not enough, however: the relationship is not
stored in the BFD section model.

### In-memory representation

[bfd/coff-bfd.h](bfd/coff-bfd.h) defines `coff_section_tdata` and
`coff_comdat_info`.  The latter stores a COMDAT symbol name and symbol-table
index, but neither structure records:

* the raw COMDAT selection;
* the input parent section number;
* a resolved parent section pointer; or
* a list of associated children.

Once section flags have been derived, the information needed to implement
selection and liveness has therefore been lost.

### Duplicate selection

[bfd/coffgen.c](bfd/coffgen.c) implements
`_bfd_coff_section_already_linked`.  It selects duplicate link-once sections by
COMDAT key and section name, then uses `_bfd_handle_already_linked` to set a
discarded section's `kept_section` to the prevailing section.

This machinery is suitable for selecting an ordinary COMDAT parent, but an
associative child must not enter independent duplicate selection.  Its result
must follow the result already chosen for its parent.

The source already notes the GNU naming workaround:

```text
.text$<key>, .xdata$<key> and .pdata$<key>
```

Only the first section has a real COMDAT key.  Matching suffixes are a legacy
convention, not general associative COMDAT semantics.

### Garbage collection

The COFF GC mark pass in [bfd/coffgen.c](bfd/coffgen.c) follows relocation
edges.  It has no traversal from a live COMDAT parent to its associative
children.

The sweep pass instead marks every section beginning with `.pdata` or `.xdata`
as live.  The PE linker scripts add another blanket fallback:

* [ld/scripttempl/pe.sc](ld/scripttempl/pe.sc)
* [ld/scripttempl/pep.sc](ld/scripttempl/pep.sc)

Both scripts use `KEEP` for `.pdata`; the PE+ script also uses `KEEP` for
`.xdata`.  This masks missing associations for unwind data, but it cannot help
arbitrary associated metadata.

It also has the reverse consequence: a retained `.pdata` entry relocates to
its function, so ordinary relocation marking can keep an otherwise dead
function alive.

### Relocatable and copied output

The object-output path in [bfd/coffcode.h](bfd/coffcode.h) states that
`x_associated` is unsupported.  The linked-symbol output path in
[bfd/cofflink.c](bfd/cofflink.c) clears `x_associated` and `x_comdat` while
rewriting Section Definition auxiliary records.

An input section number cannot simply be copied to an output object.  Section
ordering changes during `ld -r` and objcopy, so an association must be stored
as an in-memory edge and regenerated from the final parent's output section
number.

### GAS-generated SEH sections

[gas/config/obj-coff-seh-shared.c](gas/config/obj-coff-seh-shared.c) creates
`.pdata` and `.xdata` sections for `.seh_*` directives.  If the code section is
link-once, it copies the code section's link-once flags to those side sections.
It also derives names such as `.pdata$foo` and `.xdata$foo`.

It does not record a parent section.  Consequently these sections are
ordinary, independently selected COMDATs rather than associative children of
the code section.

[gas/config/obj-coff.c](gas/config/obj-coff.c) parses COFF `.section`
attributes but has no syntax for a COMDAT selection plus key or parent symbol.
The generic `.linkonce` directive cannot represent association because it has
no parent operand.

## Baseline Demonstrated Behavior

### Duplicate selection reproducer

Two LLVM MC objects define the same `ANY` parent, `foo`, and give each parent a
different associative child payload.

First object:

```asm
        .section .rdata$assoc,"dr",associative,foo
        .long 0x11111111

        .section .text$foo,"xr",discard,foo
        .globl foo
foo:
        movl $1, %eax
        retq
```

Second object:

```asm
        .section .rdata$assoc,"dr",associative,foo
        .long 0x22222222

        .section .text$foo,"xr",discard,foo
        .globl foo
foo:
        movl $2, %eax
        retq
```

A root object calls `foo`, and all three objects are linked in the same order.

| Link | `.rdata` result |
| --- | --- |
| lld control | one 4-byte child: `0x11111111` |
| GNU ld, without GC | both children: `0x11111111 0x22222222` |
| GNU ld, with `--gc-sections` | neither child |

The first GNU result proves that the child of a discarded parent is not
discarded with it.  The GC result proves that a live parent does not mark its
child.

### Relocatable-link reproducer

Running GNU `ld -r` over the first parent/child object completes without a
diagnostic.  In the output object:

* the parent has moved to section 5;
* the child's auxiliary `Number` still names section 4; and
* the child no longer has `IMAGE_SCN_LNK_COMDAT`.

The resulting intermediate object has no usable association, even though
selection value 5 remains visible in stale auxiliary data.

### GCC/GAS SEH garbage-collection reproducer

The following object has a live entry point and an otherwise unreferenced
link-once function with GAS-generated SEH data:

```asm
        .text
        .globl main
main:
        xorl %eax, %eax
        retq

        .section .text$dead,"xr"
        .linkonce discard
        .globl dead
dead:
        .seh_proc dead
        subq $40, %rsp
        .seh_stackalloc 40
        .seh_endprologue
        addq $40, %rsp
        retq
        .seh_endproc
```

After linking with rebuilt GNU `ld --gc-sections`, the output still contains:

* symbol `dead` and its `.text` bytes;
* one `.pdata` entry; and
* its `.xdata` record.

The unconditional unwind retention therefore prevents section GC from
removing this ordinary x86-64 MinGW function.

## Baseline Consequences

### Confirmed

* GNU ld does not provide atomic selection of a PE COMDAT and its associated
  components.
* Children belonging to discarded duplicate definitions can remain in a final
  image.
* Live parents do not keep arbitrary associated metadata under section GC.
* Relocatable links do not preserve a valid association.
* Blanket unwind retention can keep otherwise dead GCC-generated functions.
* GNU-produced suffix sections require linker-specific inference rather than
  expressing the standard ownership relationship.

### Likely but not demonstrated at runtime

For unwind metadata, a child left behind by a discarded function can relocate
against the prevailing definition of the same symbol.  This can produce
duplicate or mismatched `.pdata` and `.xdata` entries.  Because the Windows
exception directory is consumed at runtime, malformed ownership can affect
unwinding or exception dispatch rather than merely image size.

No Windows runtime failure was executed as part of this investigation, so
that remains a risk inferred from malformed table membership, not a measured
runtime result.

## LLVM, Clang, and lld

### LLVM IR and code generation

LLVM IR defines a COMDAT as an interrelated group that must be included or
omitted as a unit.  On COFF, LLVM lowers a multi-member IR COMDAT to:

* one key section using the group's requested selection kind; and
* additional sections using `IMAGE_COMDAT_SELECT_ASSOCIATIVE`, pointing to
  the key section.

This is used for code plus related data and metadata rather than relying on
matching section names.

LLVM's `MCAsmInfoGNUCOFF` deliberately sets
`HasCOFFAssociativeComdats = false`.  The accompanying comment says that the
deployed GNU COFF linker does not handle associative COMDATs as LLVM needs.
For GNU environments, LLVM consequently follows the GCC convention and emits
select-any sections named like `.pdata$function` and `.xdata$function`.  This
working tree addresses the consumer gap, but the upstream LLVM capability gate
remains unchanged.

For non-GNU COFF environments, LLVM emits true associations.  Enabling the
GNU path should happen only after a released GNU consumer supports them.

### LLVM assembler and object writer

LLVM's COFF assembler accepts syntax of the form:

```asm
.section name,"flags",associative,parent_symbol
```

The accepted COMDAT keywords include:

* `one_only`;
* `discard`;
* `same_size`;
* `same_contents`;
* `associative`;
* `largest`; and
* `newest` as an LLVM compatibility extension.

The parent operand is a symbol, not a section name.  During object writing,
LLVM verifies that the symbol is defined in a section and writes that
section's final number into the child's auxiliary record.  It numbers ordinary
COMDAT sections before associative sections to avoid the forward references
that older Microsoft linkers have trouble consuming.

### lld

lld first determines whether an ordinary COMDAT parent is prevailing.  It
then reads or discards associated children according to that result and
attaches retained children to the parent.

The resulting child list is consumed by:

* GC, where marking a parent enqueues its children; and
* ICF, where relevant associated children participate in parent equivalence.

In MinGW mode, lld also has a narrow compatibility fallback.  It treats
`.pdata$foo`, `.xdata$foo`, and `.eh_frame$foo` as implicitly associated with a
prevailing section for `foo` when it can identify one.

lld's current implementation is useful as a reference but should not be copied
blindly.  Its pending-section handling rejects an association whose parent has
not yet been resolved, and its child-list representation assumes an associated
child does not itself own children.  The current Microsoft specification
allows acyclic associative chains.  BFD can resolve all edges after reading
the complete section table and support both forward and backward references.

## Implemented GNU Design

### COFF-private section graph

The relationship is stored in COFF-private BFD section data rather than in
generic `asection` flags.  `coff_section_tdata` now retains:

* the raw COMDAT selection value;
* the raw associated input section number;
* a resolved immediate parent `asection *`;
* direct-child and sibling links; and
* a per-section duplicate-processing state bit.

`coff_resolve_associative_comdats` runs after all input sections have been
created, so forward and backward parent references work.  The public
`bfd_coff_set_comdat_associative` producer API updates both serializable
metadata and the live child graph; the corresponding getter is used by ld
placement without exposing COFF-private structures.

### Validation

Input resolution rejects:

* a zero or out-of-range parent number;
* a parent that is not COMDAT;
* a self-reference;
* a cycle; and
* a chain that cannot terminate at a non-associative COMDAT.

The producer setter additionally rejects cross-BFD parents, non-link-once
parents, conflicting repeated associations, and cycles before mutating the
graph.  Diagnostics identify the input object, child section, and raw parent
number where available.

The selection value is the authoritative associative-state representation;
there is no separate boolean that can diverge from it.

### Duplicate selection

Ordinary COMDATs are resolved before associative sections.  An associative
section does not enter the independent duplicate-selection hash.

After a parent decision:

* descendants of a discarded parent are discarded;
* descendants of a prevailing parent are retained; and
* redirection needed for symbols or relocations follows the prevailing
  parent through `asection::kept_section`.

Corresponding children are matched by section name and stable occurrence among
the direct children of each parent.  PE/COFF does not encode an independent
child key, so this is an explicit GNU correspondence rule rather than a format
property.  A child that has no counterpart under the prevailing parent is
simply discarded with its losing parent.  Same-named children under different
surviving parents remain independent.

### Garbage collection

The COFF mark pass traverses the association graph in addition to relocations:

* marking a parent marks each direct child;
* recursive marking naturally handles valid chains; and
* children already discarded by duplicate selection are skipped.

The PE linker scripts no longer apply blanket `KEEP` to `.pdata` and `.xdata`.
For compatibility, the BFD sweep roots only unwind sections that do not carry
a true association.  Associated unwind metadata follows its function and can
be collected with dead code.

### Relocatable and copied output

For `ld -r`, objcopy, and direct BFD object output, the implementation:

* preserves selection value 5;
* preserves the parent as an in-memory edge rather than trusting a copied
  number;
* writes `x_associated` from the final parent output section's
  `target_index`; and
* preserves the high word and fully initializes the 20-byte BigObj auxiliary
  record.

`ld -r` excludes associative inputs from ordinary wildcard merging and the PE
orphan handlers create a separate output-section statement for each one.  This
prevents one section header from receiving contradictory parent auxiliaries.

This correctness rule currently also bypasses matching wildcard placement in a
custom `-r` linker script.  Supporting custom placement requires preserving the
rule when all inputs share an effective parent and diagnosing an attempted
merge when they do not; that policy and its script-provenance plumbing remain a
follow-up rather than silently permitting an invalid merged auxiliary record.

Objcopy initially retains only the raw input number.  After all output sections
exist, BFD maps the input parent through `output_section`, rebuilds an entirely
output-local graph, and regenerates the final index.  The COFF writer also
finds copied section symbols through their input-to-output mapping.  Removing
an earlier section and copying the result again is covered by a regression.

### GAS producer interface

GAS accepts the LLVM-compatible associative form:

```asm
.section child,"dr",associative,parent_symbol
```

The parser stores a symbol reference and resolves it after the complete
assembly has been read.  Iterative resolution supports out-of-order chains.
The symbol must resolve to a link-once parent section; undefined, sectionless,
non-link-once, conflicting, and cyclic associations are diagnosed.
Reopened child sections may name different symbols in the same parent section;
the requests are compared by their resolved section relationship rather than
by symbol identity.

This patch implements only the `associative` keyword, not LLVM's complete COFF
selection grammar.  `.linkonce` was not extended because it cannot name a
parent.

Documenting and implementing the complete PE/COFF selection grammar remains a
follow-up.  The implemented associative operand names a symbol in the parent
section.

### GAS SEH generation

When `.seh_*` directives produce unwind data for a link-once code section, GAS
makes both generated sections direct associative children of the code section:

```text
.text$foo
  |-- .pdata$foo
  `-- .xdata$foo
```

They are siblings rather than an `.xdata`-under-`.pdata` chain.

The `$foo` suffixes remain for compatibility with older GNU linkers and lld's
MinGW fallback; new consumers use the real association.

## Implemented Patch Set

The consumer, producer, transformation, and compatibility stages were
implemented together in this working tree so they could be validated end to
end.  Compiler enablement and adjacent selector work remain separate.

### 1. Preserve and validate association metadata in BFD

Implemented in:

* `bfd/coff-bfd.h`;
* `bfd/libcoff-in.h`, followed by regeneration of `bfd/libcoff.h`;
* `bfd/coffcode.h`; and
* `bfd/coffgen.c`.

These changes add the COFF-private graph, retain selector and parent number
during input, resolve after section ingestion, and validate chains and cycles.
GAS tests cover forward references, chains, missing parents, invalid parents,
and cycles.

### 2. Integrate association with COMDAT selection

`_bfd_coff_section_already_linked` now selects ordinary parents first and makes
descendants follow that result.  `kept_section` carries prevailing redirection,
including corresponding-child lookup under the retained parent.

### 3. Add association traversal to COFF GC

`_bfd_coff_gc_mark` now marks direct children recursively.  Selection and GC
tests verify an arbitrary data child survives with its live prevailing parent
and disappears with a losing or dead parent.

### 4. Preserve association in object output

Normal PE/COFF output, BigObj output, `cofflink.c`, and PE private-data copying
regenerate parent indices after output mapping.  Tests cover `ld -r`, two
same-named children with distinct parents, a BigObj parent above 65,535, a
filtered objcopy that changes the parent index, and a second objcopy after the
first output is reopened.

### 5. Add GAS syntax

`obj_coff_section` accepts the LLVM-compatible associative arguments and uses
the public BFD setter after deferred symbol resolution.  `.linkonce` itself is
unchanged.

### 6. Emit true associations for GAS-generated SEH data

`make_pxdata_seg` associates generated `.pdata` and `.xdata` with link-once
code while preserving existing suffix names.  The x86-64 SEH dump test verifies
both auxiliary parent records.

### 7. Transition unwind GC fallbacks

Blanket unwind retention was replaced with this compatibility policy:

* associated unwind sections use the standard graph; and
* unassociated legacy `.pdata*` and `.xdata*` sections remain rooted in BFD.

The linker-script `KEEP` rules were removed.  A regression proves exact-name
associated unwind sections die with dead code while unassociated legacy
suffix sections remain.

### 8. Coordinate compiler enablement (follow-up)

GCC x86-64 SEH benefits automatically from the GAS change because GCC emits
`.seh_*` directives and GAS creates `.pdata` and `.xdata`.

GCC metadata that creates sections directly will need a separate explicit
parent interface.  It should not infer ownership from a section suffix.

Once a released GNU linker contains this support, LLVM can reconsider
`MCAsmInfoGNUCOFF::HasCOFFAssociativeComdats = false` and remove its GNU unwind
fallback in a coordinated release.

AArch64 MinGW SEH enablement is a separate project.  Current GCC configuration
disables both SEH and DWARF unwind there, so associative COMDAT support is
necessary infrastructure but does not by itself enable AArch64 exceptions.

### 9. Complete adjacent COMDAT conformance (follow-up)

The current GNU constants stop at selection value 5, while the current
Microsoft specification also defines value 6, `LARGEST`.  GNU handling of
`NODUPLICATES` and `EXACT_MATCH` also deserves a conformance pass.

These should be separate patches after the associative data model is sound:

* add and implement `IMAGE_COMDAT_SELECT_LARGEST`;
* preserve unknown selectors or reject them explicitly rather than silently
  treating them as `ANY`;
* tighten `NODUPLICATES` diagnostics; and
* verify exact-match comparison semantics, including relocations if required
  by compatible linkers.

LLVM's `NEWEST` selector is not part of the current Microsoft table and should
be treated as an explicit compatibility decision rather than assumed PE
semantics.

## Test Coverage and Remaining Matrix

### Executed suites

After regenerating BFD headers and both PE emulations, the following complete
target-relevant suites passed against `x86_64-w64-mingw32`:

| Suite | Result |
| --- | --- |
| GAS `pe.exp` | 30 expected passes |
| ld `ld-pe/pe.exp` | 40 expected passes, 1 expected failure, 3 unsupported |
| binutils `objcopy.exp` | 30 expected passes, 12 untested, 2 unsupported |

A separate `i386pe` manual reproducer assembled both inputs with `--32` and
verified two same-named child section headers with distinct parent indices.

### Covered scenarios

Permanent tests now cover:

* direct association and a deferred two-edge chain;
* equivalent parent symbols and conflicting resolved parent sections;
* missing syntax, undefined parents, non-link-once parents, and cycles;
* a BigObj association whose parent index is 80,005;
* GAS-generated `.pdata` and `.xdata` as direct code children;
* duplicate-parent selection with different child payloads;
* differently named children emitted in opposite orders by duplicate parents;
* GC of arbitrary associated data;
* `ld -r` selector, flags, and regenerated parent index;
* valid selector-1 (`NODUPLICATES`) parents;
* two same-named children owned by distinct surviving parents;
* associated unwind collection plus legacy unassociated unwind retention;
* objcopy preservation; and
* filtered objcopy renumbering followed by a second copy after reopening the
  first output.

The initial reproducers also compare LLVM MC/lld behavior with GNU ld and
inspect retained payloads rather than treating link success as sufficient.
A manual BigObj objcopy rewrite preserved parent 80,005 and left both trailing
reserved auxiliary bytes zero.

### Remaining coverage

Follow-up tests should cover:

* raw malformed objects with zero and out-of-range parent indices;
* explicit conflicting-parent diagnostics;
* archive extraction and LTO/plugin replacement;
* more than two associative levels and self-cycles in raw input;
* multiple same-named children under one parent;
* custom `ld -r` wildcard placement with compatible and incompatible parents;
* adversarially deep chains for resolver complexity and GC stack use;
* stripped-symbol-table transformations;
* BigObj objcopy renumbering;
* full selector 6/7 behavior; and
* GAS/`ld -r` output through lld and Microsoft `link.exe` where available.

## Compatibility and Rollout

Consumer support must precede producer enablement.  An old GNU linker already
mishandles a standards-conforming associative object, so changing GAS or
Clang/MinGW first would expose more users to duplicate or missing metadata.

Keeping GNU `$` suffixes during the transition has low cost and preserves:

* existing linker-script collection and ordering;
* older GNU behavior, imperfect though it is; and
* lld's MinGW compatibility inference.

The first release with producer support should therefore emit both the
standard auxiliary association and the conventional name.  Suffix inference
can be retired only after the supported object ecosystem no longer depends on
it.

## Non-goals

This implementation does not attempt to:

* enable GCC AArch64 Windows unwinding;
* redesign generic ELF section groups;
* enable LLVM's GNU-COFF producer gate;
* implement the complete LLVM COFF selection grammar or selectors 6/7;
* remove every PE compatibility rule;
* implement lld-style ICF in GNU ld; or
* make section names serve as the canonical ownership model.

Those concerns either have a different owner or can follow once BFD preserves
the actual PE/COFF graph.

## References

* [Microsoft PE/COFF format: Section Definition and COMDAT semantics](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#comdat-sections-object-only)
* [LLVM Language Reference: COMDATs](https://llvm.org/docs/LangRef.html#comdats)
* [LLVM COFF assembler parser](https://github.com/llvm/llvm-project/blob/main/llvm/lib/MC/MCParser/COFFAsmParser.cpp)
* [LLVM WinCOFF object writer](https://github.com/llvm/llvm-project/blob/main/llvm/lib/MC/WinCOFFObjectWriter.cpp)
* [lld COFF input and COMDAT selection](https://github.com/llvm/llvm-project/blob/main/lld/COFF/InputFiles.cpp)
* [lld COFF liveness](https://github.com/llvm/llvm-project/blob/main/lld/COFF/MarkLive.cpp)
* [lld COFF ICF](https://github.com/llvm/llvm-project/blob/main/lld/COFF/ICF.cpp)
* [LLVM GNU COFF capability gate](https://github.com/llvm/llvm-project/blob/main/llvm/lib/MC/MCAsmInfoCOFF.cpp)
