#pragma once

#include "engine/PlaybackSnapshot.h"

#include <atomic>
#include <memory>

namespace trami
{

/// Lock-free handoff of snapshots from the message thread to the audio thread.
///
/// The audio thread never allocates or frees: it takes new snapshots from the
/// `pending` slot and hands old ones back via the `trash` slot, which the
/// message thread empties. Only the audio thread fills `trash`, and only when
/// it is empty, so no snapshot is ever freed while still in use.
class SnapshotExchange
{
public:
    SnapshotExchange() = default;
    ~SnapshotExchange();

    SnapshotExchange(const SnapshotExchange&) = delete;
    SnapshotExchange& operator=(const SnapshotExchange&) = delete;

    // --- Message thread ------------------------------------------------------
    void publish(std::unique_ptr<PlaybackSnapshot> snapshot);
    void collectGarbage();

    // --- Audio thread --------------------------------------------------------
    PlaybackSnapshot* takePending() noexcept;
    bool tryRetire(PlaybackSnapshot* snapshot) noexcept;

private:
    std::atomic<PlaybackSnapshot*> pending_{nullptr};
    std::atomic<PlaybackSnapshot*> trash_{nullptr};
};

inline SnapshotExchange::~SnapshotExchange()
{
    delete pending_.exchange(nullptr);
    delete trash_.exchange(nullptr);
}

inline void SnapshotExchange::publish(std::unique_ptr<PlaybackSnapshot> snapshot)
{
    collectGarbage();
    delete pending_.exchange(snapshot.release()); // An unconsumed older snapshot was never seen by audio.
}

inline void SnapshotExchange::collectGarbage()
{
    delete trash_.exchange(nullptr);
}

inline PlaybackSnapshot* SnapshotExchange::takePending() noexcept
{
    return pending_.exchange(nullptr);
}

inline bool SnapshotExchange::tryRetire(PlaybackSnapshot* snapshot) noexcept
{
    PlaybackSnapshot* expected = nullptr;
    return trash_.compare_exchange_strong(expected, snapshot);
}

static_assert(std::atomic<PlaybackSnapshot*>::is_always_lock_free);

} // namespace trami
