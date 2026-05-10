# Zircon threat model

This is a placeholder for the detailed threat model planned in the hardening
roadmap.

Current expected scope includes static-file path traversal, malformed HTTP
request lines, oversized requests, slow or idle clients, rate limiting behavior,
content-type handling, and symlink/root escape policy. TLS, dynamic application
security, SQL injection, authentication, reverse proxying, and request body
upload handling are out of scope unless implemented and tested later.

Do not treat Zircon as production-ready based on this placeholder.
