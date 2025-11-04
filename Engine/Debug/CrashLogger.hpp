/*********************************************************************************************
 \file      CrashLogger.hpp
 \par       SofaSpuds
 \author    Erika Ishii (erika.ishii@digipen.edu) - Main Author, 100%
 \brief     Public crash-logging API: UTC-stamped file logging, optional Android log mirroring,
            and installable terminate/signal handlers; includes TryGuard and SafePtr helpers.
 \details   Create a CrashLogger, assign it to g_crashLogger, then call InstallTerminateHandler()
            and InstallSignalHandlers() during startup. Use Write(reason, extra) for manual logs
            and WriteWithStack(reason, extra) when you want an automatic stack trace appended.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/


#pragma once
#include <string>
#include <functional>
#include <exception>
#include <utility>

class CrashLogger {
public:
    CrashLogger(std::string dir, std::string file, std::string tag);
    void SetDir(std::string dir);
    void SetFile(std::string file);
    void SetTag(std::string tag);
    std::string Write(std::string reason, std::string extra);
    std::string WriteWithStack(std::string reason, std::string extra);
    void Mirror(std::string line);
    std::string LogPath() const;
    static std::string Now();
#if defined(__ANDROID__)
    void InitAndroid(void* env, void* context);
#endif
private:
    std::string dir_;
    std::string file_;
    std::string tag_;
};

extern CrashLogger* g_crashLogger;

void InstallTerminateHandler();
void InstallSignalHandlers();

struct TryGuard {
    template <typename F>
    static void Run(F&& f, std::string where) {
        try { f(); }
        catch (const std::exception& e) {
            if (g_crashLogger) {
                auto line = g_crashLogger->WriteWithStack("std::exception", where + "|" + e.what());
                g_crashLogger->Mirror(line);
            }
            throw;
        }
        catch (...) {
            if (g_crashLogger) {
                auto line = g_crashLogger->WriteWithStack("unknown_exception", where);
                g_crashLogger->Mirror(line);
            }
            throw;
        }
    }
};

template <typename T, typename D = std::function<void(T*)>>
class SafePtr {
public:
    explicit SafePtr(T* p, D d) : p_(p), d_(d) {}
    SafePtr(const SafePtr&) = delete;
    SafePtr& operator=(const SafePtr&) = delete;
    SafePtr(SafePtr&& o) noexcept : p_(o.p_), d_(std::move(o.d_)) { o.p_ = nullptr; }
    SafePtr& operator=(SafePtr&& o) noexcept { if (this != &o) { reset(); p_ = o.p_; d_ = std::move(o.d_); o.p_ = nullptr; } return *this; }
    ~SafePtr() { reset(); }
    T* get() const { return p_; }
    T& operator*() const { return *p_; }
    T* operator->() const { return p_; }
    explicit operator bool() const { return p_ != nullptr; }
    void reset(T* p = nullptr) { if (p_) d_(p_); p_ = p; }
private:
    T* p_;
    D d_;
};
