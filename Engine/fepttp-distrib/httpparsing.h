#pragma once
#include "config.h"
#include "definitions.h"
#include <vector>
#include <string>
#include <string_view>
#include <charconv>
#include <iostream>
#include <algorithm>

using namespace Config;
using namespace Definitions;

struct Header {
    std::string name;
    std::string value;

    Header(std::string_view set_name, std::string_view set_value) : name(set_name), value(set_value) {}
};

class HTTPRequest {
private:
    std::vector<Header> headers;

public:
    std::string method;
    std::string target;
    std::string protocol;

    std::string body;

    int error = 0;

    void AddHeader(Header h){
        headers.push_back(h);
    }

    void SetHeader(std::string name, std::string value){
        for(auto& h : headers){
            if(h.name == name){ h.value = value; }
        }
    }

    void DeleteHeader(std::string name){
        headers.erase(std::remove_if(headers.begin(), headers.end(),
            [&](const Header& h){ return h.name == name; }
        ), headers.end());
    }

    std::string GetHeader(std::string_view name){
        for(const auto& h : headers){
            if(name == h.name){ return h.value; }
        }
        
        return "";
    }
};

class HTTPParser {
private:
public:
    int HeaderEnd(std::string_view acc){
        if(acc.length() > REQ_HEADER_CAP){ return TOO_LARGE; }
        auto header_end_index = acc.find("\r\n\r\n");
        if(header_end_index == std::string::npos){ return COULD_NOT_FIND; }
        return header_end_index;
    }

    bool ContainsTE(std::string_view acc, int header_end_index){
        auto TE_index = acc.find("Transfer-Encoding", header_end_index);
        if(TE_index != std::string::npos){ return true; }
        return false;
    }

    int BodyLength(std::string_view acc, int header_end_index){
        auto CL_index = acc.rfind("Content-Length", header_end_index);
        if(CL_index == std::string::npos){ return COULD_NOT_FIND; }
        auto CL_end_index = acc.find("\r\n", CL_index);
        if(CL_end_index == std::string::npos){ return COULD_NOT_FIND; }

        int bytes;
        std::string_view num = acc.substr(CL_index + 16, CL_end_index /* - CL_index - 16 */);
        auto result = std::from_chars(num.data(), num.data() + num.size(), bytes);
        if(result.ec != std::errc()){ return THREW_ERROR; }
        return bytes;
    }

    int RequestLength(std::string_view acc){
        int header_end = HeaderEnd(acc);
        if(header_end == COULD_NOT_FIND){ return CONTINUE; }
        if(header_end == TOO_LARGE){ return CLOSE_ERASE_CONTINUE; }
        if(ContainsTE(acc, header_end)){ return CLOSE_ERASE_CONTINUE; }

        int body_length = BodyLength(acc, header_end);
        if(body_length == TOO_LARGE){ return CLOSE_ERASE_CONTINUE; }
        if(body_length == THREW_ERROR){ return CLOSE_ERASE_CONTINUE; }
        if(body_length == COULD_NOT_FIND){ body_length = 0; }

        const int req_length = header_end + body_length + 4;
        if(acc.length() < req_length){ return CONTINUE; }
        if(acc.length() - header_end - 4 > REQ_BODY_CAP){ return CLOSE_ERASE_CONTINUE; }

        return req_length;
    }

    // --------------------------------- Parser validation --------------------------------------

    bool ValidMethod(std::string_view m){
        if(m == "GET" || m == "POST" || m == "PUT" || m == "PATCH" || m == "DELETE" || m == "OPTIONS" || m == "TRACE" ||
           m == "CONNECT" || m == "HEAD"){
            return true;
        }

        return false;
    }

    bool ValidProtocol(std::string_view p){
        if(p == "HTTP/1.1"){
            return true;
        }

        return false;
    }
    
    // ------------------------------------------------------------------------------------------

    HTTPRequest ParseRequest(std::string acc){
        HTTPRequest req;
        size_t pos = 0; size_t rpos = 0;

        // ------------------------------- Start line ----------------------------------
        rpos = acc.find(" ");
        if(rpos == std::string::npos){ req.error = REQUEST_MALFORMED; return req; }
        req.method = acc.substr(pos, rpos - pos);
        if(!ValidMethod(req.method)){ req.error = REQUEST_MALFORMED; return req; }
        pos = rpos + 1;

        rpos = acc.find(" ", pos);
        if(rpos == std::string::npos){ req.error = REQUEST_MALFORMED; return req; }
        req.target = acc.substr(pos, rpos - pos);
        pos = rpos + 1;

        rpos = acc.find("\r\n", pos);
        if(rpos == std::string::npos){ req.error = REQUEST_MALFORMED; return req; }
        req.protocol = acc.substr(pos, rpos - pos);
        if(!ValidProtocol(req.protocol)){ req.error = REQUEST_MALFORMED; return req; }
        pos = rpos + 2;

        // --------------------------------- Headers -----------------------------------
        size_t colon = 0;
        size_t mpos = 0;
        size_t end = acc.find("\r\n\r\n", pos);
        if(end == std::string::npos){ req.error = REQUEST_MALFORMED; return req; }
        while(rpos < end){
            rpos = acc.find("\r\n", pos);
            if(rpos == std::string::npos){ req.error = REQUEST_MALFORMED; return req; }
            colon = acc.substr(pos, rpos - pos).find(": ");
            if(colon == std::string::npos){ req.error = REQUEST_MALFORMED; return req; }
            mpos = colon + pos;
            if(mpos == std::string::npos){ req.error = REQUEST_MALFORMED; return req; }
            Header h = Header(acc.substr(pos, mpos - pos), acc.substr(mpos + 2, rpos - mpos - 2));
            req.AddHeader(h);
            pos = rpos + 2;
        }

        // ----------------------------------- Body ------------------------------------
        req.body = acc.substr(end + 4);

        return req;
    }
};

HTTPParser parser;