#pragma once

#include "../StudioConfig.hpp"
#include <agk/RecordingEngine/IRecordingEngine.hpp>
#include "../SessionReplayer.hpp"
#include <memory>

namespace agk {
namespace Widgets {
    class SessionExplorer {
    public:
        static void Render(StudioConfig& config, std::shared_ptr<IRecordingEngine> engine, SessionReplayer* replayer);
    };
}
}
