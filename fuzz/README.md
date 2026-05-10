# Zircon fuzzing

This directory is reserved for fuzz targets and seed corpora planned in the
hardening roadmap.

Initial targets should cover the HTTP request parser and path normalization once
those APIs are length-bounded and easy to call with arbitrary bytes.
