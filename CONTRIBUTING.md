# Contributing

Contributions are welcome. Two things to read first, then the practical part.

## Licence and CLA

This project is distributed under the **GNU General Public License v3.0**
(see [`LICENSE`](LICENSE)).

Contributions require agreeing to the **Contributor Licence Agreement** in
[`CLA.md`](CLA.md). You keep the copyright in your work; the agreement lets the
project keep shipping under the GPL while leaving the owner able to relicense
the project as a whole later. Signing is one line in your pull request
description — the CLA explains how.

Note that [`rom/`](rom/PROVENANCE.md) is **not** covered by the project
licence: it holds transcribed 1986 factory documentation. Do not submit
changes that would relicense it or that would bring in material with an
unclear origin.

## What makes a good contribution here

This project has a bias, and it is worth knowing before you spend time:

**Claims are backed by measurement, not by plausibility.** If you change
timing, flag behaviour or display fidelity, say how you checked it. "It looks
right" is not a check; a test that fails before your change and passes after
it is.

**Uncertainty is documented, not smoothed over.** If something about the real
hardware cannot be confirmed from the documentation, it goes in
[`UNKNOWNS.md`](UNKNOWNS.md) with what is known, why that is not enough, and
how it could be settled — rather than being guessed at silently in the code.
The existing entries show the shape.

**Hardware labels stay in Russian.** `АДРЕС`, `ДАННЫЕ`, `СОСТОЯНИЕ`, `ПЗУ`,
`ОЗУ`, `П`, `РГ`, `СТ` and the rest are what the real machine and its
documentation say. Everything else — prose, comments, identifiers, commit
messages — is in English.

## Practical

1. **Build and test before and after.**

   ```bash
   make test            # four acceptance checks + ROM cross-verification
   make test-exm        # 8080EXM, several minutes
   ```

   All of it must pass. `make verify-rom` in particular must keep reporting
   that the reassembled source and the listing's object column are identical.

2. **Keep the core clean.** `core/` is freestanding C11: no libc, no dynamic
   allocation, no I/O, no pointers in `umk_machine_t` (saving state is a
   struct copy). Anything needing `stdio` belongs in `tools/`, `cli/` or
   `frontend/`.

3. **Warnings are errors in spirit.** The core and tools build with
   `-Wall -Wextra -Wshadow -Wconversion -Wpedantic` and are expected to stay
   silent.

4. **Small commits with a real message.** Say what changed and *why*. If you
   measured something, put the number in the message.

5. **Sign off your commits** with `git commit -s`.

## Reporting a problem

Open an issue with: what you did, what you expected, what happened, and the
output of `build/umkcli --rom rom/monitor.bin -c regs -c panel -c display` if
it is a behaviour question. If it is a fidelity question about the real
hardware, cite the page of the documentation you are going by.
