/**
 * @file RecordingEngine.ixx
 * @brief C++20 Module interface for the agk RecordingEngine.
 */

export module RecordingEngine;

// Import the public header to be able to export it
export import "agk/RecordingEngine/IRecordingEngine.hpp";
export import <agk/RecordingEngine/ConfigSystem.hpp>;
export import "SyncSystem.hpp";
import <memory>;

export namespace agk {

    /**
     * @brief Create a new RecordingEngine instance.
     * @return A smart pointer (Reference) to the newly created engine.
     */
    export std::shared_ptr<IRecordingEngine> CreateEngine();

    /**
     * @brief Simple test function for module verification.
     */
    export void TestRecordingEngine();

} // namespace agk
