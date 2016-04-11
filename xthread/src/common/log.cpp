#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cstdarg>
#include "../base/time.h"
#include "../base/lock.h"
#include "../base/lock_guard.h"
#include "log.h"

namespace xthread
{

    static pthread_once_t g_log_thread_once = PTHREAD_ONCE_INIT;
    static Logger *g_logger = NULL;

    static void init_global_logger() {
        g_logger = new (std::nothrow) Logger();
    }

    Logger* get_or_create_global_logger() {
        pthread_once(&g_log_thread_once, init_global_logger);
        return g_logger;
    }

    int log_open(FILE *fp, int level) {
        Logger* logger = get_or_create_global_logger();
        return logger->open(fp, level);
    }

    int log_open(const char *filename, int level,
            uint64_t rotate_size) {
        Logger* logger = get_or_create_global_logger();
        return logger->open(filename, level, rotate_size);
    }

    int log_level() {
        Logger* logger = get_or_create_global_logger();
        return logger->get_level();
    }

    void set_log_level(int level) {
        Logger* logger = get_or_create_global_logger();
        logger->set_level(level);
    }

    int log_write(int level, const char *fmt, ...) {
        Logger* logger = get_or_create_global_logger();
        va_list ap;
        va_start(ap, fmt);
        int ret = logger->logv(level, fmt, ap);
        va_end(ap);
        return ret;
    }

    Logger::Logger() {
        fp_ = stdout;
        level_ = LEVEL_DEBUG;
        filename_[0] = '\0';
        rotate_size_ = 0;
        stats.w_curr = 0;
        stats.w_total = 0;
    }

    Logger::~Logger() {
        this->close();
    }

    std::string Logger::level_name(){
        switch(level_){
            case Logger::LEVEL_FATAL:
                return "fatal";
            case Logger::LEVEL_ERROR:
                return "error";
            case Logger::LEVEL_WARN:
                return "warn";
            case Logger::LEVEL_INFO:
                return "info";
            case Logger::LEVEL_DEBUG:
                return "debug";
            case Logger::LEVEL_TRACE:
                return "trace";
        }
        return "";
    }

    std::string Logger::output_name(){
        return filename_;
    }

    uint64_t Logger::get_rotate_size(){
        return rotate_size_;
    }

    int Logger::open(FILE *fp, int level) {
        fp_ = fp;
        level_ = level;
        return 0;
    }

    int Logger::open(const char *filename, int level, uint64_t rotate_size){
        if(strlen(filename) > PATH_MAX - 20){
            fprintf(stderr, "log filename too long!");
            return -1;
        }
        level_ = level;
        rotate_size_ = rotate_size;
        strcpy(this->filename_, filename);

        FILE *fp;
        if(strcmp(filename, "stdout") == 0){
            fp = stdout;
        }else if(strcmp(filename, "stderr") == 0){
            fp = stderr;
        }else{
            fp = fopen(filename, "a");
            if(fp == NULL){
                return -1;
            }

            struct stat st;
            int ret = fstat(fileno(fp), &st);
            if(ret == -1){
                fprintf(stderr, "fstat log file %s error!", filename);
                return -1;
            }else{
                stats.w_curr = st.st_size;
            }
        }
        return this->open(fp, level);
    }

    void Logger::close(){
        if(fp_ != stdin && fp_ != stdout){
            fclose(fp_);
        }
    }

    void Logger::rotate(){
        fclose(fp_);
        char newpath[PATH_MAX];
        time_t time;
        struct timeval tv;
        struct tm *tm;
        gettimeofday(&tv, NULL);
        time = tv.tv_sec;
        tm = localtime(&time);
        sprintf(newpath, "%s.%04d%02d%02d-%02d%02d%02d",
                this->filename_,
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec);

        int ret = rename(this->filename_, newpath);
        if(ret == -1){
            return;
        }
        fp_ = fopen(this->filename_, "a");
        if(fp_ == NULL){
            return;
        }
        stats.w_curr = 0;
    }

    int Logger::get_level(const char *levelname){
        if(strcmp("trace", levelname) == 0){
            return LEVEL_TRACE;
        }
        if(strcmp("debug", levelname) == 0){
            return LEVEL_DEBUG;
        }
        if(strcmp("info", levelname) == 0){
            return LEVEL_INFO;
        }
        if(strcmp("warn", levelname) == 0){
            return LEVEL_WARN;
        }
        if(strcmp("error", levelname) == 0){
            return LEVEL_ERROR;
        }
        if(strcmp("fatal", levelname) == 0){
            return LEVEL_FATAL;
        }
        if(strcmp("none", levelname) == 0){
            return LEVEL_NONE;
        }
        return LEVEL_DEBUG;
    }

    inline static const char* get_level_name(int level){
        switch(level){
            case Logger::LEVEL_FATAL:
                return "[FATAL] ";
            case Logger::LEVEL_ERROR:
                return "[ERROR] ";
            case Logger::LEVEL_WARN:
                return "[WARN ] ";
            case Logger::LEVEL_INFO:
                return "[INFO ] ";
            case Logger::LEVEL_DEBUG:
                return "[DEBUG] ";
            case Logger::LEVEL_TRACE:
                return "[TRACE] ";
        }
        return "";
    }

#define LEVEL_NAME_LEN  8
#define LOG_BUF_LEN     4096

    int Logger::logv(int level, const char *fmt, va_list ap){
        Logger *logger = get_or_create_global_logger();
        if(logger->level_ < level){
            return 0;
        }

        char buf[LOG_BUF_LEN];
        int len;
        char *ptr = buf;

        time_t time;
        struct timeval tv;
        struct tm *tm;
        gettimeofday(&tv, NULL);
        time = tv.tv_sec;
        tm = localtime(&time);
        len = sprintf(ptr, "%04d-%02d-%02d %02d:%02d:%02d.%03d ",
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec, static_cast<int>(tv.tv_usec/1000));
        if(len < 0){
            return -1;
        }
        ptr += len;

        memcpy(ptr, get_level_name(level), LEVEL_NAME_LEN);
        ptr += LEVEL_NAME_LEN;

        int space = static_cast<int> (sizeof(buf) - (ptr - buf) - 10);
        len = vsnprintf(ptr, space, fmt, ap);
        if(len < 0){
            return -1;
        }
        ptr += len > space? space : len;
        *ptr++ = '\n';
        *ptr = '\0';

        len = static_cast<int>(ptr - buf);
        {
            base::MutexGuard<base::MutexLock> guard(mutex_);
            fwrite(buf, len, 1, fp_);
            fflush(fp_);

            stats.w_curr += len;
            stats.w_total += len;
            if(rotate_size_ > 0 && stats.w_curr > rotate_size_){
                this->rotate();
            }
        }
        return len;
    }

    int Logger::trace(const char *fmt, ...){
        Logger* logger = get_or_create_global_logger();
        va_list ap;
        va_start(ap, fmt);
        int ret = logger->logv(Logger::LEVEL_TRACE, fmt, ap);
        va_end(ap);
        return ret;
    }

    int Logger::debug(const char *fmt, ...){
        Logger* logger = get_or_create_global_logger();
        va_list ap;
        va_start(ap, fmt);
        int ret = logger->logv(Logger::LEVEL_DEBUG, fmt, ap);
        va_end(ap);
        return ret;
    }

    int Logger::info(const char *fmt, ...){
        Logger* logger = get_or_create_global_logger();
        va_list ap;
        va_start(ap, fmt);
        int ret = logger->logv(Logger::LEVEL_INFO, fmt, ap);
        va_end(ap);
        return ret;
    }

    int Logger::warn(const char *fmt, ...){
        Logger* logger = get_or_create_global_logger();
        va_list ap;
        va_start(ap, fmt);
        int ret = logger->logv(Logger::LEVEL_WARN, fmt, ap);
        va_end(ap);
        return ret;
    }

    int Logger::error(const char *fmt, ...){
        Logger* logger = get_or_create_global_logger();
        va_list ap;
        va_start(ap, fmt);
        int ret = logger->logv(Logger::LEVEL_ERROR, fmt, ap);
        va_end(ap);
        return ret;
    }

    int Logger::fatal(const char *fmt, ...){
        Logger* logger = get_or_create_global_logger();
        va_list ap;
        va_start(ap, fmt);
        int ret = logger->logv(Logger::LEVEL_FATAL, fmt, ap);
        va_end(ap);
        return ret;
    }

}
