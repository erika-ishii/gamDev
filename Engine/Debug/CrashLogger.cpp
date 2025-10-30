/*********************************************************************************************
 \file      CrashLogger.cpp
 \par       SofaSpuds
 \author    Erika Ishii (erika.ishii@digipen.edu) - Main Author, 100%
 \brief     Minimal crash logging utility: UTC-stamped file logging, optional Android logcat
            mirroring, and terminate/signal handlers for common fatal errors.
 \details   Usage:
            1) Create a global instance and point g_crashLogger to it.
            2) Call InstallTerminateHandler() and InstallSignalHandlers() early in app init.
            3) Use CrashLogger::Write(reason, extra) for manual error records.
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
#if defined(__ANDROID__)
#include <android/log.h>
#include <jni.h>
#endif

CrashLogger* g_crashLogger;

/*************************************************************************************
  \brief  Append a single text line to a file (creates file if missing).
  \param  path  Full path to the log file.
  \param  line  Line to append (no newline needed—function adds '\n').
*************************************************************************************/
static void WriteLine(const std::string& path, const std::string& line) {
    std::ofstream ofs(path, std::ios::out | std::ios::app);
    ofs << line << "\n";
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
  \brief  Write one log record in the format "<UTC>|<reason>|<extra>".
  \param  reason  Short category or error reason.
  \param  extra   Optional details (may be empty).
*************************************************************************************/
void CrashLogger::Write(std::string reason, std::string extra) {
    WriteLine(LogPath(), Now() + "|" + reason + "|" + extra);
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
        g_crashLogger->Write("std_terminate", "");
        g_crashLogger->Mirror("std_terminate");
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
        g_crashLogger->Write(s, "");
        g_crashLogger->Mirror(s);
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
