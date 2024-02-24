#pragma once

#include <string>
#include <unordered_map>
#include <sstream>
using namespace std;
class HttpRequest
{
    /*
    POST /login HTTP/1.1   //line
    Host: localhost:7007
    Content-Type: application/x-www-form-urlencoded
    Content-Length: 30  //head

    username=yuanshen&password=test1  //body
    */
public:
    enum Method
    {
        GET,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH,
        UNKNOWN
    };
    enum ParseState
    {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH
    };
    HttpRequest() : method(UNKNOWN), state(REQUEST_LINE){};

    // parse the entire request
    bool parse(string request)
    {
        istringstream iss(request);
        string line;
        bool result = true;
        // read the request by lines, and handle each line according to current state
        while (getline(iss, line) && line != "\r")
        {
            if (state == REQUEST_LINE)
            {
                result = parseRequestLine(line);
            }
            else if (state == HEADERS)
            {
                result = parseHeader(line);
            }
        }
        if (method == POST)
        {
            body = request.substr(request.find("\r\n\r\n") + 4);
        }

        return result;
    }

    // 解析表单形式的请求体，返回键值对字典
    unordered_map<string, string> parseFormBody() const
    {
        unordered_map<string, string> params;
        if (method != POST)
            return params;

        istringstream stream(body);
        string pair;

        while (getline(stream, pair, '&'))
        {
            size_t pos = pair.find('=');
            if (pos == string::npos)
                continue;
            string key = pair.substr(0, pos);
            string value = pair.substr(pos + 1);
            params[key] = value;
        }
        return params;
    }

    string getMethodString() const
    {
        switch (method)
        {
        case GET:
            return "GET";
        case POST:
            return "POST";

        default:
            return "UNKNOWN";
        }
    }

    const string &getPath() const
    {
        return path;
    }

private:
    Method method;
    string path;
    string version;
    unordered_map<string, string> headers;
    ParseState state;
    string body;

    bool parseRequestLine(const string &line)
    {
        istringstream iss(line);
        string method_str;
        iss >> method_str;
        if (method_str == "GET")
            method = GET;
        else if (method_str == "POST")
            method = POST;
        else
            method = UNKNOWN;

        iss >> path;
        iss >> version;
        state = HEADERS;
        return true;
    }

    bool parseHeader(const string &line)
    {
        size_t pos = line.find(": ");
        if (pos == string::npos)
        {
            return false;
        }
        string key = line.substr(0, pos);
        string value = line.substr(pos + 2);
        headers[key] = value;
        return true;
    }
};