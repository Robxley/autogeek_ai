#include <agk/RecordingEngine/Log.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/async.h>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace agk {

    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;
    static std::shared_ptr<spdlog::sinks::dist_sink_mt> s_DistSink;

    void Log::Init(const std::string& logDirectory) {
        if (s_CoreLogger) return;

        // Ensure directories exist
        try {
            std::filesystem::create_directories(logDirectory);
        } catch (...) {}

        // Generate timestamped filename
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << logDirectory << "/agk_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".log";
        std::string filename = ss.str();

        // Initialize Thread Pool for Async Logging (8192 items, 1 thread)
        spdlog::init_thread_pool(8192, 1);

        // Standard Sinks
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        
        s_DistSink = std::make_shared<spdlog::sinks::dist_sink_mt>();
        s_DistSink->add_sink(consoleSink);

        // Pattern: [Time] [LoggerName] Message
        s_DistSink->set_pattern("%^[%T] %n: %v%$");

        s_CoreLogger = std::make_shared<spdlog::async_logger>("CORE", s_DistSink, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        s_CoreLogger->set_level(spdlog::level::trace);
        spdlog::register_logger(s_CoreLogger);

        s_ClientLogger = std::make_shared<spdlog::async_logger>("CLIENT", s_DistSink, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        s_ClientLogger->set_level(spdlog::level::trace);
        spdlog::register_logger(s_ClientLogger);

        // Automatical flush for safety (important for async)
        spdlog::flush_on(spdlog::level::info);
        spdlog::flush_every(std::chrono::seconds(1));

        // Add the default file sink
        AddFileSink(logDirectory);
    }

    void Log::AddFileSink(const std::string& logDirectory) {
        if (!s_DistSink) return;

        try {
            std::filesystem::create_directories(logDirectory);
        } catch (...) {}

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << logDirectory << "/agk_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".log";
        std::string filename = ss.str();

        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filename, 1024 * 1024 * 5, 3);
        fileSink->set_pattern("%^[%T] %n: %v%$");
        s_DistSink->add_sink(fileSink);
    }

    void Log::AddSink(spdlog::sink_ptr sink) {
        if (s_DistSink) {
            sink->set_pattern("%^[%T] %n: %v%$");
            s_DistSink->add_sink(sink);
        }
    }
} // namespace agk
