#pragma once

#include "model/Song.h"

#include <vector>

namespace trami
{

/// Snapshot-based undo/redo history for the Song document.
class SongHistory
{
public:
    static constexpr std::size_t kMaxHistory = 64;

    void pushState(const Song& song)
    {
        undoStack_.push_back(song);
        if (undoStack_.size() > kMaxHistory)
            undoStack_.erase(undoStack_.begin());
        redoStack_.clear();
    }

    bool canUndo() const noexcept { return !undoStack_.empty(); }
    bool canRedo() const noexcept { return !redoStack_.empty(); }

    bool undo(Song& currentSong)
    {
        if (undoStack_.empty())
            return false;

        redoStack_.push_back(currentSong);
        currentSong = std::move(undoStack_.back());
        undoStack_.pop_back();
        return true;
    }

    bool redo(Song& currentSong)
    {
        if (redoStack_.empty())
            return false;

        undoStack_.push_back(currentSong);
        currentSong = std::move(redoStack_.back());
        redoStack_.pop_back();
        return true;
    }

    void clear()
    {
        undoStack_.clear();
        redoStack_.clear();
    }

private:
    std::vector<Song> undoStack_;
    std::vector<Song> redoStack_;
};

} // namespace trami
