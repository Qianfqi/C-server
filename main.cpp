
#include "Database.h"
#include "HttpServer.h"

int main()
{
    database db("user.db"); // create database
    HttpServer server(8080, 10, db);
    server.setupRoutes();
    server.start();
    return 0;
}