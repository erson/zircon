# Changelog

All notable changes to the Zircon project will be documented in this file.

## [Unreleased]

### Added
- Added an MIT license file.
- Added placeholder documentation for threat modeling, AI-assisted audit notes, benchmarks, and fuzzing.

### Changed
- Repositioned project documentation around experimental, AI-assisted HTTP/1.1 static serving.
- Clarified implemented and not implemented features.
- Updated contribution guidance to require precise claims and tests for security-sensitive changes.

### Fixed
- Updated integration test expectations so HTTP-only responses do not require HSTS by default.

## [1.1.0] - 2025-03-30

### Added
- MIME type detection for proper Content-Type headers
- ETag generation for improved caching
- Cache-Control headers (86400s for ETag resources, 3600s for others)
- Automatic rate limiter unblocking after a timeout period
- Comprehensive test protocol in test-improved.sh
- TEST.md documentation for testing procedures
- Enhanced documentation with improved code comments

### Changed
- Improved HTTP header formatting
- Better handling of HEAD requests
- Enhanced directory index handling
- Optimized path processing logic
- Rate limiter now bypasses localhost (127.0.0.1) for testing
- Updated README with latest features and improvements
- Project renamed from "Misewe" to "Zircon"

### Fixed
- Fixed HTTP header formatting issues
- Corrected path handling for directory indexes
- Improved error handling for file paths
- Fixed potential memory leaks in ETag handling

## [1.0.0] - Initial Release

### Added
- Basic HTTP server functionality
- File serving capability
- Rate limiting based on client IP
- Simple security features
- Path traversal prevention
- File type restrictions
