# Zircon

A lightweight, event-driven HTTP server written in C.

> **Note**: This is an experimental project developed with AI assistance. Not recommended for production use.

## Features

- **Event-driven architecture** using kqueue (macOS/BSD) or epoll (Linux)
- **Zero-copy file transfer** via platform-native sendfile
- **Security hardened**:
  - Path traversal prevention
  - File type restrictions
  - Security headers (CSP, X-Frame-Options, HSTS, etc.)
  - Rate limiting
- **HTTP caching** with ETag support and Cache-Control headers
- **MIME type detection** by extension and magic bytes
- **Multi-threaded mode** with SO_REUSEPORT support (experimental)

## Quick Start

```bash
# Build
make

# Run (listens on 127.0.0.1:8000 by default)
./bin/zircon

# Run with options
./bin/zircon --port 8080 --bind 0.0.0.0

# Show all options
./bin/zircon --help
```

## Usage

```
Usage: ./bin/zircon [OPTIONS]

Options:
  --port PORT        Listen on PORT (default: 8000)
  --bind ADDR        Bind to ADDR (default: 127.0.0.1)
  --workers N        Run with N worker threads (experimental)
  --platform-info    Show platform capabilities
  --help             Show this help
```

## Project Structure

```
zircon/
├── src/
│   ├── main.c              # Entry point, CLI parsing
│   ├── server_async.c      # Event-driven server core
│   ├── http.c              # HTTP parsing, MIME types, ETag
│   ├── rate_limiter.c      # IP-based rate limiting
│   ├── security_headers.c  # Security header generation
│   ├── logger.c            # Logging system
│   └── platform/           # Platform abstraction layer
│       ├── event_kqueue.c  # macOS/BSD event loop
│       ├── event_epoll.c   # Linux event loop
│       ├── sendfile.c      # Zero-copy file transfer
│       ├── thread_pool.c   # Worker thread pool
│       └── thread.c        # Thread utilities
├── include/                # Header files
├── test/                   # Test suite
├── www/                    # Default web root
├── conf/                   # Configuration files
└── Makefile
```

## Building

### Requirements

- GCC or Clang
- POSIX-compliant OS (Linux, macOS, FreeBSD)
- pthread library

### Build Commands

```bash
make              # Release build
make DEBUG=1      # Debug build with symbols
make clean        # Clean build artifacts
make test         # Run test suite
```

## Testing

```bash
# Run all tests
make test

# Run specific test categories
make test-unit
make test-security
make test-performance

# Run edge case security tests
./test/edge_case_test.sh
```

## Architecture

### Event Loop

The server uses a single-threaded event loop by default:

1. Accept connections via non-blocking socket
2. Register client fd with kqueue/epoll
3. On read event: parse HTTP request, validate, serve file
4. Use sendfile() for zero-copy transfer
5. Close connection after response

### Security Model

All requests pass through multiple validation layers:

- **Path validation**: Blocks `..`, encoded traversal, null bytes
- **Method restriction**: Only GET and HEAD allowed
- **File type check**: Whitelist of allowed extensions
- **Rate limiting**: Per-IP request throttling
- **Response headers**: CSP, HSTS, X-Frame-Options, etc.

## Performance

Single-threaded async mode on macOS (Apple Silicon):
- ~15,000 requests/second with 50 concurrent connections
- Sub-millisecond latency for small files

## Limitations

- No HTTPS (TLS) support
- No HTTP/2
- No dynamic content / CGI
- Request size limited to 4KB
- Single-threaded by default

## License

MIT License

## Acknowledgments

Developed as an experiment in AI-assisted systems programming.
