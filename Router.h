#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Database.h"
#include <functional>

using namespace std;
using HandlerFunc = function<HttpResponse(const HttpRequest &)>;

class Router
{
public:
    void addRoute(const string &method, const string &path, HandlerFunc handler)
    {
        routes[method + "|" + path] = handler;
    }

    HttpResponse routeRequest(const HttpRequest &request)
    {
        string key = request.getMethodString() + "|" + request.getPath();
        if (routes.count(key))
        {
            return routes[key](request);
        }
        return HttpResponse::makeErrorResponse(404, "Not Found");
    }

    void setupDatabaseRoutes(database &db)
    {
        addRoute("POST", "/register", [&db](const HttpRequest &req){
            auto res = req.parseFormBody();
            string username = res["username"];
            string password = res["password"];
            if (db.registerUser(username, password))
            {
                return HttpResponse::makeOkResponse("Register Success");
            }
            return HttpResponse::makeErrorResponse(400, "Register Failed"); });

        addRoute("POST", "/login", [&db](const HttpRequest &req){
            auto res = req.parseFormBody();
            string username = res["username"];
            string password = res["password"];
            if (db.loginUser(username, password))
            {
                return HttpResponse::makeOkResponse("Login Success");
            }
            return HttpResponse::makeErrorResponse(400, "Login Failed"); });
    }

private:
    unordered_map<string, HandlerFunc> routes;
};