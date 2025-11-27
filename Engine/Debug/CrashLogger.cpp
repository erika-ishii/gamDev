/*********************************************************************************************
 \file      CrashLogger.cpp
 \par       SofaSpuds
 \author    Erika Ishii (erika.ishii@digipen.edu) - Main Author, 100%
 \brief     Crash logging utility: UTC-stamped, thread-tagged records with optional Android logcat
            mirroring, stack-trace capture, and terminate/signal handlers for fatal errors.
 \details   Usage:
            1) Create a global instance and point g_crashLogger to it.
            2) Call InstallTerminateHandler() and InstallSignalHandlers() early in app init.
            3) Use CrashLogger::Write/WriteWithStack(reason, extra) for manual error records or drills.
            On Android, call InitAndroid(env, context) to set an app-writable directory.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "CrashLogger.hpp"
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>
#include <utility>
#include <cstdlib>
#if defined(__unix__) || defined(__APPLE__)
#include <execinfo.h>
#endif
#if defined(__ANDROID__)
#include <android/log.h>
#include <jni.h>
#endif
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
CrashLogger* g_crashLogger;

/*************************************************************************************
  \brief  Append a single text line to a file (creates file if missing).
  \param  path  Full path to the log file.
  \param  line  Line to append (no newline needed—function adds '\n').
*************************************************************************************/
static void WriteLine(const std::string& path, const std::string& line) {
    std::ofstream ofs(path, std::ios::out | std::ios::app);
    if (!ofs) {
        std::cerr << "[CrashLog] Failed to open log file: " << path << "\n";
        return;
    }
    ofs << line << '\n';
    ofs.flush();
}

namespace {
    std::string Sanitize(std::string text) {
        for (char& c : text) {
            if (c == '\r' || c == '\n') {
                c = ' ';
            }
            else if (c == '|') {
                c = '/';
            }
        }
        return text;
    }

    std::string ThreadIdString() {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        return oss.str();
    }

    std::string CaptureStackTrace() {
#if defined(__unix__) || defined(__APPLE__)
        constexpr int kMaxFrames = 64;
        void* frames[kMaxFrames];
        int count = ::backtrace(frames, kMaxFrames);
        if (count <= 0) {
            return "stacktrace_unavailable";
        }
        char** symbols = ::backtrace_symbols(frames, count);
        if (!symbols) {
            return "stacktrace_unavailable";
        }
        std::ostringstream oss;
        for (int i = 0; i < count; ++i) {
            if (i > 0) {
                oss << " > ";
            }
            oss << symbols[i];
        }
        std::free(symbols);
        return oss.str();
#elif defined(_WIN32)
        return "stacktrace_unavailable";
#else
        return "stacktrace_unavailable";
#endif
    }
}

/*************************************************************************************
  \brief  Construct a crash logger, normalizing directory and ensuring its existence.
  \param  dir   Target directory for log files.
  \param  file  Log file name (no path).
  \param  tag   Platform logging tag (used by Android logcat).
*************************************************************************************/
CrashLogger::CrashLogger(std::string dir, std::string file, std::string tag)
    : dir_(std::filesystem::absolute(std::filesystem::path(std::move(dir))).string()),
    file_(std::move(file)),
    tag_(std::move(tag)) {
    std::filesystem::create_directories(dir_);
}

/*************************************************************************************
  \brief  Set and normalize the log directory; creates it if necessary.
  \param  dir  New directory path.
*************************************************************************************/
void CrashLogger::SetDir(std::string dir) {
    dir_ = std::filesystem::absolute(std::filesystem::path(std::move(dir))).string();
    std::filesystem::create_directories(dir_);
}

/*************************************************************************************
  \brief  Set the log file name (does not touch the filesystem).
  \param  file  New file name.
*************************************************************************************/
void CrashLogger::SetFile(std::string file) { file_ = std::move(file); }

/*************************************************************************************
  \brief  Set the platform logging tag (used by Mirror on Android).
  \param  tag  New tag string.
*************************************************************************************/
void CrashLogger::SetTag(std::string tag) { tag_ = std::move(tag); }

/*************************************************************************************
  \brief  Get the full path to the current log file.
  \return Absolute path composed from dir_ and file_.
*************************************************************************************/
std::string CrashLogger::LogPath() const {
    return (std::filesystem::path(dir_) / file_).string();
}

/*************************************************************************************
  \brief  Current UTC timestamp in ISO-8601 basic form (YYYY-MM-DDTHH:MM:SSZ).
  \return Formatted timestamp string.
*************************************************************************************/
std::string CrashLogger::Now() {
    auto t = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::tm tmv;
#if defined(_WIN32)
    gmtime_s(&tmv, &tt);
#else
    gmtime_r(&tt, &tmv);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmv, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

/*************************************************************************************
  \brief  Write one log record in the format "<UTC>|<reason>|thread=<id>|<extra>" (extras optional).
  \param  reason  Short category or error reason.
  \param  extra   Optional details (may be empty before sanitization).
*************************************************************************************/
std::string CrashLogger::Write(std::string reason, std::string extra) {
    std::string sanitizedReason = Sanitize(std::move(reason));
    std::string sanitizedExtra = Sanitize(std::move(extra));
    std::string record = Now() + "|" + sanitizedReason + "|thread=" + ThreadIdString();
    if (!sanitizedExtra.empty()) {
        record += "|" + sanitizedExtra;
    }
    WriteLine(LogPath(), record);
    return record;
}

/*************************************************************************************
  \brief  Write a log record and append a sanitized stack trace to the extras section.
  \param  reason  Short category or error reason.
  \param  extra   Optional pre-existing details (stack trace appended automatically).
*************************************************************************************/
std::string CrashLogger::WriteWithStack(std::string reason, std::string extra) {
    std::string stack = Sanitize(CaptureStackTrace());
    if (!stack.empty()) {
        if (!extra.empty()) {
            extra += "|";
        }
        extra += "stack=" + stack;
    }
    return Write(std::move(reason), std::move(extra));
}

/*************************************************************************************
  \brief  Mirror a line to Android logcat as FATAL; no-op on other platforms.
  \param  line  Message to mirror.
*************************************************************************************/
void CrashLogger::Mirror(std::string line) {
#if defined(__ANDROID__)
    __android_log_write(ANDROID_LOG_FATAL, tag_.empty() ? "ENGINE/CRASH" : tag_.c_str(), line.c_str());
#else
    (void)line; // Prevent unused-parameter warning
#endif
}

#if defined(__ANDROID__)
/*************************************************************************************
  \brief  Initialize logger directory from Android Context.getFilesDir().
  \param  env      JNIEnv* (as void* to avoid public JNI include in header).
  \param  context  jobject Android Context (as void*).
*************************************************************************************/
void CrashLogger::InitAndroid(void* env, void* context) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    jobject ctx = reinterpret_cast<jobject>(context);
    jclass ctxCls = e->GetObjectClass(ctx);
    jmethodID mid = e->GetMethodID(ctxCls, "getFilesDir", "()Ljava/io/File;");
    jobject fileObj = e->CallObjectMethod(ctx, mid);
    jclass fileCls = e->GetObjectClass(fileObj);
    jmethodID p = e->GetMethodID(fileCls, "getAbsolutePath", "()Ljava/lang/String;");
    jstring jpath = (jstring)e->CallObjectMethod(fileObj, p);
    const char* cpath = e->GetStringUTFChars(jpath, nullptr);
    dir_ = std::string(cpath);
    e->ReleaseStringUTFChars(jpath, cpath);
    std::filesystem::create_directories(dir_);
}
#endif

/*************************************************************************************
  \brief  std::terminate handler: logs "std_terminate" and exits(1).
*************************************************************************************/
static void OnTerminate() {
    if (g_crashLogger) {
        auto line = g_crashLogger->WriteWithStack("std_terminate", "");
        g_crashLogger->Mirror(line);
    }
    std::_Exit(1);
}

/*************************************************************************************
  \brief  POSIX signal handler for common fatal signals; logs and exits(1).
  \param  sig  Signal number (e.g., SIGSEGV, SIGABRT).
*************************************************************************************/
static void OnSignal(int sig) {
    if (g_crashLogger) {
        std::string s = "signal_" + std::to_string(sig);
        auto line = g_crashLogger->WriteWithStack(s, "");
        g_crashLogger->Mirror(line);
    }
    std::_Exit(1);
}

/*************************************************************************************
  \brief  Install std::terminate handler (OnTerminate).
*************************************************************************************/
void InstallTerminateHandler() { std::set_terminate(OnTerminate); }

/*************************************************************************************
  \brief  Install handlers for SIGSEGV, SIGABRT, SIGFPE, SIGILL, and SIGTERM.
*************************************************************************************/
void InstallSignalHandlers() {
    std::signal(SIGSEGV, OnSignal);
    std::signal(SIGABRT, OnSignal);
    std::signal(SIGFPE, OnSignal);
    std::signal(SIGILL, OnSignal);
    std::signal(SIGTERM, OnSignal);
}
