# Contributing to Zircon

Thanks for helping with Zircon. This project is an experimental, AI-assisted C
systems-programming project, so contributions should prioritize correctness,
reproducibility, and precise claims over feature breadth.

## Before submitting changes

Please run, at minimum:

```bash
make
make test
```

If you touch HTTP parsing, path handling, file serving, defensive headers,
rate limiting, or connection state transitions, include tests or document a
clear manual verification path.

## Contribution guidelines

- Keep changes small and reviewable.
- Preserve existing behavior unless you are fixing a documented bug.
- Keep documentation claims precise and modest.
- Do not describe Zircon as production-ready or as a replacement for nginx,
  Caddy, or another maintained production server.
- Avoid unsupported security claims. For example, do not mention SQL injection
  unless SQL-related functionality actually exists.
- Add or update tests for parser, path, and security-sensitive changes.
- Avoid adding heavy dependencies unless the tradeoff is documented.
- Keep Linux and macOS portability in mind where practical.

## Code style

- Use 4-space indentation.
- Keep C code compatible with the project Makefile and existing compiler modes.
- Keep functions focused and name ownership/lifetime expectations clearly.
- Prefer bounded string and buffer handling.
- Comment non-obvious behavior and security-sensitive decisions.

## Reporting issues

For non-sensitive issues, open a GitHub issue with:

- commit hash
- OS and compiler
- command used
- proof-of-concept request or input, if relevant
- expected behavior
- actual behavior

A dedicated `SECURITY.md` with vulnerability reporting guidance is planned in a
follow-up hardening step.
