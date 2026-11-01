#pragma once

#include <cstdint>
#include <functional>

namespace trami
{

/// Strongly typed identifier. Prevents mixing up e.g. a SampleId with a PatternId.
template <typename Tag>
struct Id
{
    std::uint32_t value = 0;

    constexpr bool isValid() const noexcept { return value != 0; }
    friend constexpr bool operator==(Id, Id) = default;
    friend constexpr auto operator<=>(Id, Id) = default;
};

using SampleId = Id<struct SampleTag>;
using PatternId = Id<struct PatternTag>;
using ClipId = Id<struct ClipTag>;
using BlockId = Id<struct BlockTag>;

/// Hands out unique, increasing ids. Zero is reserved for "invalid".
template <typename IdType>
class IdGenerator
{
public:
    IdType next() noexcept { return IdType{++last_}; }

    /// Ensures future ids are larger than an id loaded from disk.
    void observe(IdType id) noexcept
    {
        if (id.value > last_)
            last_ = id.value;
    }

private:
    std::uint32_t last_ = 0;
};

} // namespace trami

template <typename Tag>
struct std::hash<trami::Id<Tag>>
{
    std::size_t operator()(trami::Id<Tag> id) const noexcept { return std::hash<std::uint32_t>{}(id.value); }
};
