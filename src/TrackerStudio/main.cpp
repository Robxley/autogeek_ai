#include "StudioApp.hpp"
#include <agk/RecordingEngine/Log.hpp>

int main(int argc, char** argv) {
    agk::StudioApp app;

    if (!app.Initialize()) {
        AGK_CRITICAL("Failed to initialize Studio App.");
        return -1;
    }

    app.Run();
    return 0;
}
