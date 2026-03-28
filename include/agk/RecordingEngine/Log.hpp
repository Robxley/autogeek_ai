#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <memory>

namespace agk {
    class Log {
    public:
        static void Init(const std::string& logDirectory = "logs");
        static void AddFileSink(const std::string& logDirectory);
        static void AddSink(spdlog::sink_ptr sink);
        
        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
        
    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
} // namespace agk

// Macros for Core Engine
#define AGK_CORE_TRACE(...)    if (::agk::Log::GetCoreLogger()) ::agk::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define AGK_CORE_INFO(...)     if (::agk::Log::GetCoreLogger()) ::agk::Log::GetCoreLogger()->info(__VA_ARGS__)
#define AGK_CORE_WARN(...)     if (::agk::Log::GetCoreLogger()) ::agk::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define AGK_CORE_ERROR(...)    if (::agk::Log::GetCoreLogger()) ::agk::Log::GetCoreLogger()->error(__VA_ARGS__)
#define AGK_CORE_CRITICAL(...) if (::agk::Log::GetCoreLogger()) ::agk::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Macros for Client App
#define AGK_TRACE(...)         if (::agk::Log::GetClientLogger()) ::agk::Log::GetClientLogger()->trace(__VA_ARGS__)
#define AGK_INFO(...)          if (::agk::Log::GetClientLogger()) ::agk::Log::GetClientLogger()->info(__VA_ARGS__)
#define AGK_WARN(...)          if (::agk::Log::GetClientLogger()) ::agk::Log::GetClientLogger()->warn(__VA_ARGS__)
#define AGK_ERROR(...)         if (::agk::Log::GetClientLogger()) ::agk::Log::GetClientLogger()->error(__VA_ARGS__)
#define AGK_CRITICAL(...)      if (::agk::Log::GetClientLogger()) ::agk::Log::GetClientLogger()->critical(__VA_ARGS__)
