#pragma once
#include "config.h"
#include "definitions.h"
#include "httpparsing.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <algorithm>

using namespace Config;
using namespace Definitions;

class HTTPResponse {
private:
    std::vector<Header> headers;
    std::string body;

    static inline std::unordered_map<int, std::string> status = {
        {100, "Continue"}, {101, "Switching Protocols"}, {102, "Processing"}, {103, "Early Hints"}, {200, "OK"}, {201, "Created"},
        {202, "Accepted"}, {203, "Non-Authoritative Information"}, {204, "No Content"}, {205, "Reset Content"},
        {206, "Partial Content"}, {207, "Multi-Status"}, {208, "Already Reported"}, {226, "IM Used"}, {300, "Multiple Choices"},
        {301, "Moved Permanently"}, {302, "Found"}, {303, "See Other"}, {304, "Not Modified"}, {307, "Temporary Redirect"},
        {308, "Permanent Redirect"}, {400, "Bad Request"}, {401, "Unauthorized"}, {402, "Payment Required"}, {403, "Forbidden"},
        {404, "Not Found"}, {405, "Method Not Allowed"}, {406, "Not Acceptable"}, {407, "Proxy Authentication Required"},
        {408, "Request Timeout"}, {409, "Conflict"}, {410, "Gone"}, {411, "Length Required"}, {412, "Precondition Failed"},
        {413, "Content Too Large"}, {414, "URI Too long"}, {415, "Unsupported Media Type"}, {416, "Range Not Satisfiable"},
        {417, "Expectation Failed"}, {418, "I'm a teapot"}, {421, "Misdirected Request"}, {422, "Unprocessable Content"},
        {423, "Locked"}, {424, "Failed Dependency"}, {425, "Too Early"}, {426, "Upgrade Required"}, {428, "Precondition Required"},
        {429, "Too Many Requests"}, {431, "Request Header Fields Too Large"}, {451, "Unavailable For Legal Reasons"},
        {500, "Internal Server Error"}, {501, "Not Implemented"}, {502, "Bad Gateway"}, {503, "Service Unavailable"},
        {504, "Gateway Timeout"}, {505, "HTTP Version Not Supported"}, {506, "Variant Also Negotiates"}, {507, "Insufficient Storage"},
        {508, "Loop Detected"}, {510, "Not Extended"}, {511, "Network Authentication Required"}
    };

    std::string FormattedDateAndTime(){
        auto now = std::chrono::system_clock::now();
        std::time_t _now = std::chrono::system_clock::to_time_t(now);
        std::tm * gmt = std::gmtime(&_now);
        char buf[32];
        if(std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt)){
            return buf;
        }
        return "";
    }

public:
    // Manual header methods
    void SetHeader(std::string name, std::string value){
        for(auto& h : headers){
            if(h.name == name){ h.value = value; }
        }
    }

    std::string GetHeader(std::string name){
        for(const auto& h : headers){
            if(h.name == name){ return h.value; }
        }
        return "";
    }

    void AddHeader(std::string name, std::string value){
        Header h = Header(name, value);
        headers.push_back(h);
    };

    void DeleteHeader(std::string name){
        headers.erase(std::remove_if(headers.begin(), headers.end(),
            [&](const Header& h){ return h.name == name; }
        ), headers.end());
    }

    // Manual body methods
    void AppendToBody(std::string extra){
        body += extra;
    }

    void TextPlain(std::string text){
        Header h = Header("Content-Type", "text/plain");
        Header hh = Header("Content-Length", std::to_string(text.length()));
        headers.push_back(h); headers.push_back(hh);
        body = text;
    }

    void TextHTML(std::string text){
        Header h = Header("Content-Type", "text/html");
        Header hh = Header("Content-Length", std::to_string(text.length()));
        headers.push_back(h); headers.push_back(hh);
        body = text;
    }

    // (The JSON needs to be stringified first)
    void ApplicationJSON(std::string rawjson){
        Header h = Header("Content-Type", "application/json");
        Header hh = Header("Content-Length", std::to_string(rawjson.length()));
        headers.push_back(h); headers.push_back(hh);
        body = rawjson;
    }

    int status_code;

    // Default is "no-cache"
    std::string cache_control = "no-cache";

    std::string Assemble(){
        std::string res = "";

        // Assemble status line
        res += ("HTTP/1.1 " + std::to_string(status_code) + " " + status[status_code] + "\r\n");

        // Assemble response headers (controlled by server, not by user)
        res += "Server: Fepttp";
        res += ("\r\nDate: " + FormattedDateAndTime());
        res += ("\r\nCache-Control: " + cache_control);
        
        for(const auto& h : headers){
            res += ("\r\n" + h.name + ": " + h.value);
        }

        res += "\r\nConnection: keep-alive";

        res += "\r\n\r\n";

        res += body;

        // Return the assembled response
        return res;
    }
};

class HTTPReqRes {
public:
    HTTPRequest req;
    HTTPResponse res;

    HTTPReqRes(HTTPRequest rst, HTTPResponse rsp) : req(rst), res(rsp) {}
};