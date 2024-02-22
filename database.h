#include <string.h>
#include <iostream>
#include <sqlite3.h>
#include "logger.h"
using namespace std;

class database{
    private:
        sqlite3* db;
    public:
        database(const string& path)
        {
            if(sqlite3_open(path.c_str(), &db) != SQLITE_OK)
                throw runtime_error("Failed to open database");
            
            //create the table of users
            const char * sql = "CREATE TABLE IF NOT EXISTS users(username TEXT PRIMARY KEY, password TEXT)";
            char * errmsg;
            if(sqlite3_exec(db, sql, 0, 0, &errmsg) != SQLITE_OK)
            {
                throw runtime_error("Failed to create table: " + string(errmsg));
            }
            
        }
        ~database()
        {
            sqlite3_close(db);
        }

        //function for users to register
        bool registerUser(const string& username, const string& password)
        {
            //prepare sql
            string sql = "INSERT INTO users (username, password) VALUES (?, ?);";
            sqlite3_stmt * stmt;
            if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            {
                LOG_INFO("Failed to prepare register sql for user: %s", username.c_str());
                return false;
            }
            //bind
            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
            //exec
            if(sqlite3_step(stmt) != SQLITE_DONE)
            {
                LOG_INFO("Failed to excecute sql for user: %s", username.c_str());
                sqlite3_finalize(stmt);
                return false;
            }
            sqlite3_finalize(stmt);
            LOG_INFO("User registered: %s with password: %s", username.c_str(), password.c_str());
            return true;
            //username is the primary key, so it is unique

        }

        //function for users to login
        bool loginUser(const string& username, const string& password)
        {
            string sql = "SELECT password FROM users WHERE username = ?;";
            sqlite3_stmt * stmt;
            if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            {
                LOG_INFO("Failed to prepare login sql for user: %s", username.c_str());
                return false;
            }
            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) != SQLITE_ROW)
            {
                // 如果用户名不存在，记录日志，清理资源并返回false
                LOG_INFO("User not found: %s" , username.c_str());
                sqlite3_finalize(stmt);
                return false;
            }
            //get the password stored
            const char * stored_password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            string password_str = string(stored_password, sqlite3_column_bytes(stmt, 0));
            sqlite3_finalize(stmt);
            if(stored_password == nullptr || password_str != password)
            {
                LOG_INFO("Failed to login for user: %s", username);
                return false;
            }

            LOG_INFO("User login : %s", username);
            return true;
        }
    
};


