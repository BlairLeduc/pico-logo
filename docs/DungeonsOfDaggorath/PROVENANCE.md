# Provenance of the Dungeons of Daggorath source

This directory holds the 6809 assembly source for *Dungeons of Daggorath*
(DynaMicro / Unified Technologies, 1982–83), vendored as the source of truth
for [`daggorath-design.md`](../daggorath-design.md): every rule in that design
cites the file and routine it came from, and where the design and the source
disagree, the source is right.

`missing-macros.asm` is not part of the original release. It is the 2022
reconstruction of the macros LWTools needs in order to assemble the rest.

This file records what is known about the licensing, because the answer is not
a single line and the two documents beside each other say different things.

## The 1983 notice, which is in every file

Each `.ASM` file carries the original header:

> Copyright (c) 1983 - Unified Technologies, Inc.
>
> **WARNING !!!**
>
> No part of this work — documents, schematic diagrams, drawings, printed
> circuit board artwork, program source listings or object code, may be
> reproduced by any mechanical, photographic, or electronic process, or in the
> form of a phonographic recording, nor may it be stored in a retrieval
> system, transmitted, or otherwise copied for public or private use, without
> permission from Unified Technologies, Inc.

## The 2002 grant, which is `grant_of_license.png`

`grant_of_license.png` is a photograph of a handwritten note. Transcribed
verbatim:

> A non-exclusive perpetual license is hereby granted to Michael J. Spencer Jr.
> to copy, replicate or emulate the source code contained herein, for the
> purposes of replicating the Dungeons of Daggorath game.
>
> Mar. 7, 2002
>
> *(signed)* Douglas J. Morgan
> President, DynaMicro Inc.
> Chairman, United Technologies

## What that grant is, stated plainly

It is a real grant, perpetual and non-exclusive, from a signatory holding
apparent authority over both companies named in the 1983 notice — and it is
made **to a named individual, Michael J. Spencer Jr.** It is not a public
licence, and it is not an open-source licence. It names no successors,
assignees or downstream recipients, and it is limited on its face to the
purpose of replicating the game.

So the honest position is: the note establishes that permission was given, to
someone, for this purpose. It does not by itself establish that this
repository holds that permission.

**A correction to the record.** The commit that vendored these files
(`b54bd71`) describes the source as "released by its authors with the grant of
licence kept beside it", and [`daggorath-design.md`](../daggorath-design.md)
§1 repeats "released with the grant of licence". That is a stronger reading
than the note supports, and this file supersedes both.

## Why the files are still here

Raised by an automated reviewer on
[PR #173](https://github.com/BlairLeduc/pico-logo/pull/173), which asked that
the provenance be confirmed and the applicable grant documented, or the source
un-vendored and referenced externally. The decision taken was to keep the
source in the tree and document the grant exactly as it reads, rather than to
paraphrase it into something more comfortable — which is what this file is.

Nothing here is compiled, linked or shipped: no `.ASM` in this directory is
referenced by `CMakeLists.txt`, and no byte of it reaches the firmware. It is
reference material for a design document.

Anyone with better information about the chain of permission — or a reason
this should not be redistributed — should open an issue.
