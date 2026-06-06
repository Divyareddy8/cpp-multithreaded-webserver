#pragma once
#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string uri;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    bool valid = false;
};

struct HttpResponse {
    int status_code;
    std::string status_text;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    std::string serialize() const;
};

class HttpParser {
public:
    static HttpRequest parse(const std::string& raw);
    static HttpResponse make_response(int code, const std::string& body,
                                      const std::string& content_type = "text/html");
    static HttpResponse make_error(int code);
    static std::string get_mime_type(const std::string& path);
};
