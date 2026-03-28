#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <memory>

namespace agk {
    class Log {
    public:
        static void Init();
        
        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
        
    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
} // namespace agk

// Macros for Core Engine
#define AGK_CORE_TRACE(...)    ::agk::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define AGK_CORE_INFO(...)     ::agk::Log::GetCoreLogger()->info(__VA_ARGS__)
#define AGK_CORE_WARN(...)     ::agk::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define AGK_CORE_ERROR(...)    ::agk::Log::GetCoreLogger()->error(__VA_ARGS__)
#define AGK_CORE_CRITICAL(...) ::agk::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Macros for Client App
#define AGK_TRACE(...)         ::agk::Log::GetClientLogger()->trace(__VA_ARGS__)
#define AGK_INFO(...)          ::agk::Log::GetClientLogger()->info(__VA_ARGS__)
#define AGK_WARN(...)          ::agk::Log::GetClientLogger()->warn(__VA_ARGS__)
#define AGK_ERROR(...)         ::agk::Log::GetClientLogger()->error(__VA_ARGS__)
#define AGK_CRITICAL(...)      ::agk::Log::GetClientLogger()->critical(__VA_ARGS__)
