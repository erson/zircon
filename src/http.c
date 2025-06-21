#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h> // For errno
#include <stddef.h> // For size_t explicitly

#define HEURISTIC_READ_SIZE 16 // Max bytes to read for heuristic MIME detection

// Define the structure for magic byte sequences and MIME types
typedef struct {
    unsigned char *magic;
    size_t magic_len;
    const char *mime_type;
} magic_mime_map_t;

// Define the array of magic byte maps
static magic_mime_map_t magic_mime_maps[] = {
    // PNG: \x89PNG\r\n\x1a\n (8 bytes) -> image/png
    { (unsigned char *)"\x89PNG\r\n\x1a\n", 8, "image/png" },
    // JPEG: \xFF\xD8\xFF (3 bytes) -> image/jpeg (variants like JFIF, Exif start with this)
    { (unsigned char *)"\xFF\xD8\xFF", 3, "image/jpeg" },
    // GIF: GIF87a or GIF89a (6 bytes) -> image/gif
    { (unsigned char *)"GIF87a", 6, "image/gif" },
    { (unsigned char *)"GIF89a", 6, "image/gif" },
    // PDF: %PDF- (5 bytes, e.g., \x25PDF-) -> application/pdf
    { (unsigned char *)"%PDF-", 5, "application/pdf" }, // or { (unsigned char *)"\x25PDF-", 5, "application/pdf" }
    // ZIP: PK\x03\x04 (4 bytes) -> application/zip
    { (unsigned char *)"PK\x03\x04", 4, "application/zip" },
    // Legacy MS Office (CFBF)
    { (unsigned char []){0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1}, 8, "application/msword" },
    // Rich Text Format (RTF)
    { (unsigned char []){0x7B, 0x5C, 0x72, 0x74, 0x66, 0x31}, 6, "application/rtf" }, // "{\\rtf1"
    // RAR v5+
    { (unsigned char []){0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00}, 8, "application/x-rar-compressed" },
    // GZIP
    { (unsigned char []){0x1F, 0x8B}, 2, "application/gzip" },
    // 7-Zip
    { (unsigned char []){0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C}, 6, "application/x-7z-compressed" },
    // WebP (Simplified - checks for "RIFF" prefix, actual WEBP is at offset 8)
    { (unsigned char []){0x52, 0x49, 0x46, 0x46}, 4, "image/webp" },
    // AVI (Simplified - checks for "RIFF" prefix, actual AVI is at offset 8)
    { (unsigned char []){0x52, 0x49, 0x46, 0x46}, 4, "video/x-msvideo" }, // AVI also starts with RIFF, may need more specific check or different order
    // TIFF (Little Endian)
    { (unsigned char []){0x49, 0x49, 0x2A, 0x00}, 4, "image/tiff" },
    // TIFF (Big Endian)
    { (unsigned char []){0x4D, 0x4D, 0x00, 0x2A}, 4, "image/tiff" },
    // BMP
    { (unsigned char []){0x42, 0x4D}, 2, "image/bmp" },
    // SVG (checking for "<svg")
    { (unsigned char []){0x3C, 0x73, 0x76, 0x67}, 4, "image/svg+xml" }, // "<svg"
    // MP3 (ID3 tag)
    { (unsigned char []){0x49, 0x44, 0x33}, 3, "audio/mpeg" }, // "ID3"
    // Ogg
    { (unsigned char []){0x4F, 0x67, 0x67, 0x53}, 4, "application/ogg" }, // "OggS"
    // Matroska/WebM (.mkv, .webm)
    { (unsigned char []){0x1A, 0x45, 0xDF, 0xA3}, 4, "video/x-matroska" }
};

/**
 * Get appropriate MIME type based on file extension
 * 
 * This function determines the Content-Type header value based on the file extension.
 * Supports common web file types including HTML, CSS, JavaScript, images and more.
 *
 * @param path The file path to analyze
 * @return A string containing the MIME type (defaults to application/octet-stream)
 */
const char *http_get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    ext++; /* Skip the dot */
    
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0)
        return "text/html";
    if (strcasecmp(ext, "css") == 0)
        return "text/css";
    if (strcasecmp(ext, "js") == 0)
        return "application/javascript";
    if (strcasecmp(ext, "png") == 0)
        return "image/png";
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(ext, "gif") == 0)
        return "image/gif";
    if (strcasecmp(ext, "svg") == 0)
        return "image/svg+xml";
    if (strcasecmp(ext, "ico") == 0)
        return "image/x-icon";
    if (strcasecmp(ext, "json") == 0)
        return "application/json";
    if (strcasecmp(ext, "xml") == 0)
        return "application/xml";
    if (strcasecmp(ext, "txt") == 0)
        return "text/plain";
    if (strcasecmp(ext, "pdf") == 0)
        return "application/pdf";
    if (strcasecmp(ext, "zip") == 0)
        return "application/zip";

    // MS Office documents
    if (strcasecmp(ext, "doc") == 0) return "application/msword";
    if (strcasecmp(ext, "docx") == 0) return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (strcasecmp(ext, "xls") == 0) return "application/vnd.ms-excel";
    if (strcasecmp(ext, "xlsx") == 0) return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    if (strcasecmp(ext, "ppt") == 0) return "application/vnd.ms-powerpoint";
    if (strcasecmp(ext, "pptx") == 0) return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    if (strcasecmp(ext, "rtf") == 0) return "application/rtf";

    // Archive extensions
    if (strcasecmp(ext, "rar") == 0) return "application/x-rar-compressed";
    if (strcasecmp(ext, "tar") == 0) return "application/x-tar";
    if (strcasecmp(ext, "gz") == 0) return "application/gzip";
    if (strcasecmp(ext, "7z") == 0) return "application/x-7z-compressed";

    // Additional Image extensions
    if (strcasecmp(ext, "webp") == 0) return "image/webp";
    if (strcasecmp(ext, "tif") == 0 || strcasecmp(ext, "tiff") == 0) return "image/tiff";
    if (strcasecmp(ext, "bmp") == 0) return "image/bmp";

    // Audio/Video extensions
    if (strcasecmp(ext, "mp3") == 0) return "audio/mpeg";
    if (strcasecmp(ext, "oga") == 0) return "audio/ogg";
    if (strcasecmp(ext, "ogv") == 0) return "video/ogg";
    if (strcasecmp(ext, "ogg") == 0) return "application/ogg"; // Generic Ogg
    if (strcasecmp(ext, "mov") == 0) return "video/quicktime";
    if (strcasecmp(ext, "avi") == 0) return "video/x-msvideo";
    if (strcasecmp(ext, "mkv") == 0) return "video/x-matroska";

    // If no extension match, try heuristic detection based on magic numbers
    fprintf(stderr, "INFO: File extension not recognized for '%s'. Attempting heuristic MIME detection.\n", path);
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Could not open file %s for heuristic MIME detection: %s\n", path, strerror(errno));
        return "application/octet-stream"; // Fallback if file can't be opened
    }

    unsigned char buffer[HEURISTIC_READ_SIZE];
    size_t bytes_read = fread(buffer, 1, HEURISTIC_READ_SIZE, fp);
    fclose(fp);

    if (bytes_read == 0) {
        // Empty file or read error, fallback
        return "application/octet-stream";
    }

    for (size_t i = 0; i < sizeof(magic_mime_maps) / sizeof(magic_mime_maps[0]); ++i) {
        if (bytes_read >= magic_mime_maps[i].magic_len) {
            if (memcmp(buffer, magic_mime_maps[i].magic, magic_mime_maps[i].magic_len) == 0) {
                fprintf(stderr, "INFO: Heuristic detection for '%s' identified MIME type as '%s'.\n", path, magic_mime_maps[i].mime_type);
                return magic_mime_maps[i].mime_type; // Found a match
            }
        }
    }
    
    fprintf(stderr, "INFO: Heuristic detection for '%s' found no matching magic number. Defaulting to application/octet-stream.\n", path);
    return "application/octet-stream"; // Default fallback
}

/**
 * Generate an ETag based on file metadata
 * 
 * Creates a weak ETag (prefixed with W/) based on the file's last modification time
 * and size. This allows browsers to cache content and validate freshness without
 * downloading the entire file again.
 *
 * @param mtime The file's last modification time
 * @param size The file size in bytes
 * @return A newly allocated string containing the ETag (caller must free)
 */
char *http_generate_etag(time_t mtime, size_t size) {
    char *etag = malloc(64);
    if (!etag) return NULL;
    
    /* Generate weak ETag based on modification time and size */
    snprintf(etag, 64, "W/\"%lx-%lx\"", (unsigned long)mtime, (unsigned long)size);
    return etag;
}

/**
 * Check if the client's provided ETag matches the server's ETag
 * 
 * This function checks if the If-None-Match header in the client's request
 * matches the ETag generated by the server. Used to implement HTTP 304
 * Not Modified responses for efficient caching.
 *
 * @param request The full HTTP request from the client
 * @param etag The server-generated ETag value
 * @return true if the ETags match, false otherwise
 */
bool http_check_etag_match(const char *request, const char *etag) {
    if (!request || !etag) return false;
    
    const char *if_none_match = strstr(request, "If-None-Match:");
    if (!if_none_match) return false;
    
    return strstr(if_none_match, etag) != NULL;
}

bool http_parse_request(const char *buffer, __attribute__((unused)) size_t length, http_request_t *req) {
    char method[16];
    
    /* Parse request line */
    if (sscanf(buffer, "%15s %255s %15s", method, req->path, req->version) != 3) {
        return false;
    }

    /* Parse method */
    if (strcmp(method, "GET") == 0) {
        req->method = HTTP_GET;
    } else if (strcmp(method, "HEAD") == 0) {
        req->method = HTTP_HEAD;
    } else if (strcmp(method, "POST") == 0) {
        req->method = HTTP_POST;
    } else {
        req->method = HTTP_UNSUPPORTED;
        return false;
    }

    return true;
}

void http_send_response(int client_fd, int status_code, 
                       const char *content_type, 
                       const void *body, size_t body_length,
                       const char *extra_headers) {
    char headers[4096];
    ssize_t bytes_written; // To store return value of write
    int header_len;

    /* Format headers */
    header_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n",
        status_code,
        status_code == 200 ? "OK" : "Error",
        content_type,
        body_length);

    /* Add extra headers if provided */
    if (extra_headers) {
        strncat(headers, extra_headers, sizeof(headers) - header_len - 1);
        header_len = strlen(headers);
    }

    /* Add final CRLF */
    strncat(headers, "\r\n", sizeof(headers) - header_len - 1);
    header_len = strlen(headers);

    /* Send headers */
    bytes_written = write(client_fd, headers, header_len);
    if (bytes_written < 0) {
        fprintf(stderr, "Error writing headers to client: %s\n", strerror(errno));
        return; // Early exit if headers can't be written
    }
    if (bytes_written < header_len) {
        fprintf(stderr, "Error writing headers to client: not all bytes written (%zd/%d)\n", bytes_written, header_len);
        return; // Early exit
    }

    /* Send body */
    if (body && body_length > 0) {
        bytes_written = write(client_fd, body, body_length);
        if (bytes_written < 0) {
            fprintf(stderr, "Error writing body to client: %s\n", strerror(errno));
        } else if (bytes_written < (ssize_t)body_length) {
            fprintf(stderr, "Error writing body to client: not all bytes written (%zd/%zu)\n", bytes_written, body_length);
        }
    }
}

void http_send_error(int client_fd, int status_code, const char *message) {
    /* Add security headers for error responses too */
    const char *security_headers = 
        "X-Frame-Options: DENY\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "X-XSS-Protection: 1; mode=block\r\n"
        "Content-Security-Policy: default-src 'self'\r\n"
        "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n";

    http_send_response(client_fd, status_code, "text/plain", 
                      message, strlen(message), security_headers);
}
