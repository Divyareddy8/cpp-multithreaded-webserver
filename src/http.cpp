#include "http.h"
#include <sstream>
#include <unordered_map>

// ─── Serialize response ────────────────────────────────────────────────────
std::string HttpResponse::serialize() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    for (auto& [k, v] : headers)
        oss << k << ": " << v << "\r\n";
    oss << "\r\n" << body;
    return oss.str();
}

// ─── Parse raw HTTP request ────────────────────────────────────────────────
HttpRequest HttpParser::parse(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);

    // Request line
    std::string line;
    if (!std::getline(stream, line)) return req;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream rl(line);
    if (!(rl >> req.method >> req.uri >> req.version)) return req;

    // Headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break; // blank line = end of headers

        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        // Trim leading whitespace from value
        size_t start = value.find_first_not_of(' ');
        if (start != std::string::npos) value = value.substr(start);

        req.headers[key] = value;
    }

    // Body (if any)
    std::string body_line;
    while (std::getline(stream, body_line)) {
        req.body += body_line + "\n";
    }

    req.valid = true;
    return req;
}

// ─── Build a response ─────────────────────────────────────────────────────
HttpResponse HttpParser::make_response(int code, const std::string& body,
                                        const std::string& content_type) {
    HttpResponse res;
    res.status_code = code;
    res.body = body;

    static const std::unordered_map<int, std::string> STATUS_TEXTS = {
        {200, "OK"}, {201, "Created"}, {204, "No Content"},
        {301, "Moved Permanently"}, {302, "Found"},
        {400, "Bad Request"}, {403, "Forbidden"}, {404, "Not Found"},
        {405, "Method Not Allowed"}, {500, "Internal Server Error"},
        {501, "Not Implemented"}
    };

    auto it = STATUS_TEXTS.find(code);
    res.status_text = (it != STATUS_TEXTS.end()) ? it->second : "Unknown";

    res.headers["Content-Type"]   = content_type;
    res.headers["Content-Length"] = std::to_string(body.size());
    res.headers["Connection"]     = "close";
    res.headers["Server"]         = "CppWebServer/1.0";

    return res;
}

// ─── Standard error pages ─────────────────────────────────────────────────
HttpResponse HttpParser::make_error(int code) {
    static const std::unordered_map<int, std::string> MSGS = {
        {400, "Bad Request"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
    };
    auto it = MSGS.find(code);
    std::string msg = (it != MSGS.end()) ? it->second : "Error";

    std::string body =
        "<!DOCTYPE html><html><head><title>" + std::to_string(code) + " " + msg +
        "</title><style>"
        "body{font-family:monospace;background:#0a0a0a;color:#e0e0e0;"
        "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
        "div{text-align:center} h1{font-size:6rem;margin:0;color:#ff4444}"
        "p{color:#888;font-size:1.2rem}"
        "</style></head><body>"
        "<div><h1>" + std::to_string(code) + "</h1><p>" + msg + "</p>"
        "<p><a href='/' style='color:#4af'>← Go home</a></p></div>"
        "</body></html>";

    return make_response(code, body, "text/html; charset=utf-8");
}

// ─── MIME type lookup ─────────────────────────────────────────────────────
std::string HttpParser::get_mime_type(const std::string& path) {
    static const std::unordered_map<std::string, std::string> MIME = {
        {".html", "text/html; charset=utf-8"},
        {".htm",  "text/html; charset=utf-8"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".json", "application/json"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".svg",  "image/svg+xml"},
        {".ico",  "image/x-icon"},
        {".txt",  "text/plain"},
        {".pdf",  "application/pdf"},
        {".woff", "font/woff"},
        {".woff2","font/woff2"},
    };

    auto dot = path.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = path.substr(dot);
        auto it = MIME.find(ext);
        if (it != MIME.end()) return it->second;
    }
    return "application/octet-stream";
}
