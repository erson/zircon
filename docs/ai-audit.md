# AI-assisted development audit

This is a placeholder for the detailed audit document planned in the hardening
roadmap.

Zircon is developed as an AI-assisted C systems-programming experiment. Code
produced with AI assistance should be treated as suspicious until it is tested,
reviewed, fuzzed where practical, and documented with precise limitations.

Areas to audit include HTTP parsing, path normalization, keep-alive state
transitions, timeout behavior, sendfile fallback behavior, rate limiting under
concurrency, portability, and documentation claims.
