#include <memory>
#include <agk/RecordingEngine/IRecordingEngine.hpp>

namespace agk {
namespace Widgets {

    class RecordingConfigUI {
    public:
        static void Render(std::shared_ptr<IRecordingEngine> engine);
    };

} // namespace Widgets
} // namespace agk
