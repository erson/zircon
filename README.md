# Zircon

Zircon is a small static HTTP/1.1 web server written in C. It is an
AI-assisted systems-programming experiment: the goal is to make the project
more testable and easier to review while documenting what is implemented, what
is not implemented, and what still needs hardening.

Zircon is **not production-ready** and is not intended to replace nginx, Caddy,
or another maintained production web server. Treat it as experimental code for
learning, testing, and review.

## Current focus

The project is being hardened incrementally with:

- unit and integration tests
- stricter build modes and sanitizers
- fuzz targets for parsing/path code
- reproducible benchmark scripts
- a documented threat model
- honest documentation for AI-assisted development

Some of these items are still planned rather than complete. See the linked docs
below for current status and limitations.

## Implemented / not implemented

### Implemented today

- Static file serving from a configured web root
- HTTP/1.x request handling for `GET` and `HEAD`
- Optional HTTP keep-alive
- kqueue/epoll event backends where supported by the platform layer
- Platform sendfile support with a userspace fallback
- ETag / `If-None-Match` cache validation
- MIME type selection by extension, with limited magic-byte fallback
- IP-based rate limiting
- Basic defensive response headers such as CSP, `X-Frame-Options`, and
  `X-Content-Type-Options`
- Request size limits and connection timeouts
- Config file support

### Not implemented

- TLS / HTTPS termination
- HTTP/2
- HTTP/3
- Reverse proxying
- CGI or dynamic application execution
- Request body upload handling
- Authentication or authorization
- Full RFC 9112 compliance
- Production-grade sandboxing

## Try it locally

```bash
make
./bin/zircon --root www --keep-alive
curl -i http://127.0.0.1:8000/
```

Other useful commands:

```bash
./bin/zircon --help
./bin/zircon --platform-info
make test
```

## Usage

```text
Usage: ./bin/zircon [OPTIONS]
Options:
  --config FILE      Load config from FILE
  --platform-info    Show platform capabilities
  --workers N        Run with N worker threads (0=auto, default=single-threaded)
  --port PORT        Listen on PORT (default: 8000)
  --bind ADDR        Bind to ADDR (default: 127.0.0.1)
  --root DIR         Serve files from DIR (default: www)
  --timeout SEC      Connection timeout in seconds (default: 30)
  --keep-alive       Enable HTTP Keep-Alive
  --help             Show this help
```

## Configuration file

Example `conf/server.conf`:

```ini
# Network
port = 8000
bind = 127.0.0.1

# Paths
root = www

# Limits
timeout = 30
max_request_size = 8192

# Features
keep_alive = false
```

## Project structure

```text
zircon/
├── src/                 # Server, HTTP, rate limiting, logging, platform code
├── include/             # Public/internal headers
├── test/                # C and shell-based tests
├── docs/                # Security, audit, benchmark, and hardening notes
├── fuzz/                # Planned fuzz targets and seed corpora
├── www/                 # Default web root
├── conf/                # Example configuration
└── Makefile
```

## Building

### Requirements

- GCC or Clang
- POSIX-like OS; Linux and macOS are the primary portability targets
- pthread library

### Build commands

```bash
make              # Release build
make DEBUG=1      # Debug build with symbols
make clean        # Clean build artifacts
make test         # Run the current test suite
```

## Testing

```bash
make test
make test-unit
make test-security
make test-performance
./test/edge_case_test.sh
```

Some tests are older integration scripts and are being cleaned up as the project
is hardened. Optional tools should be skipped gracefully rather than becoming
mandatory for a normal build.

## Architecture overview

The default server path is a single-threaded event loop:

1. Accept connections with a non-blocking socket.
2. Register client file descriptors with kqueue/epoll through the platform layer.
3. Read an HTTP request, parse it, validate the requested path, and serve a file.
4. Use platform sendfile support where available.
5. Reset state for keep-alive connections or close the connection.

An experimental worker-thread mode exists behind `--workers`.

## Security posture

Zircon should be read as experimental C networking code, not as a hardened
security boundary. Current defensive behavior includes method restrictions,
request size limits, rate limiting, basic path checks, file extension filtering,
and defensive response headers. These are useful hardening steps, but they are
not a substitute for a complete security review, sandboxing, TLS termination, or
production operational controls.

See:

- Security policy *(planned in a follow-up hardening step)*
- [Threat model](docs/threat-model.md)
- [AI-assisted audit notes](docs/ai-audit.md)
- [Benchmark methodology](docs/benchmarks.md)
- [Fuzzing workspace](fuzz/)

## Performance

No headline performance claim is made here. Benchmark results should be reported
with the exact command, hardware/OS, compiler, git commit, server command, and
raw benchmark output. Reproducible benchmark tooling is planned under `bench/`
and documented in `docs/benchmarks.md`.

## Limitations

- No built-in TLS support
- No HTTP/2 or HTTP/3 support
- No reverse proxy or dynamic application support
- Path handling and HTTP parsing need additional tests and fuzzing
- Sandbox support is not production-grade and is not currently documented as a
  security guarantee

## Contributing

Please see [CONTRIBUTING.md](CONTRIBUTING.md). In short: keep claims precise,
add tests for parser/path/security-sensitive changes, and run `make` and
`make test` before submitting changes.

## License

MIT License. See [LICENSE](LICENSE).

## Acknowledgments

Developed as an experiment in AI-assisted systems programming.
