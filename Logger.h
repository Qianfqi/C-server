#include <fstream>
#include <string>
#include <chrono>
#include <ctime>
#include <cstdarg> // 引入处理可变参数的头文件
using namespace std;
enum Loglevel{
    INFO,
    WARNING,
    ERROR,
};

#define LOG_INFO(...) logger::logmessage(INFO, __VA_ARGS__);
#define LOG_WARNING(...) logger::logmessage(WARNING, __VA_ARGS__);
#define LOG_ERROR(...) logger::logmessage(ERROR, __VA_ARGS__);

class logger{
    public:
    static void logmessage(Loglevel level, const char * format, ...)
    {
        ofstream logfile("server.log", ios::app); // out to server.log
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        string levelmess;
        switch(level)
        {
            case INFO: levelmess = "INFO"; break;
            case WARNING: levelmess = "WARNING"; break;
            case ERROR: levelmess = "ERROR"; break;
            default: break;
        };

        va_list args;
        va_start(args, format);
        char buffer[2048];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        logfile << ctime(&now_c) << " [" << levelmess << "] " << buffer << endl;
        logfile.close();
    }
};