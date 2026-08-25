#pragma once

#include <Windows.h>

#include <utility>

namespace xeo3
{
template<typename Frame>
class FiberFrameRegistry
{
public:
    bool initialize() noexcept
    {
        if (index_ != FLS_OUT_OF_INDEXES)
        {
            return true;
        }

        index_ = FlsAlloc(nullptr);
        return index_ != FLS_OUT_OF_INDEXES;
    }

    void cleanup() noexcept
    {
        if (index_ == FLS_OUT_OF_INDEXES)
        {
            return;
        }

        FlsSetValue(index_, nullptr);
        FlsFree(index_);
        index_ = FLS_OUT_OF_INDEXES;
    }

    bool push(Frame& frame) noexcept
    {
        if (index_ == FLS_OUT_OF_INDEXES)
        {
            return false;
        }

        frame.next = static_cast<Frame*>(FlsGetValue(index_));
        return FlsSetValue(index_, &frame) != FALSE;
    }

    bool remove(Frame& frame) noexcept
    {
        if (index_ == FLS_OUT_OF_INDEXES)
        {
            return false;
        }

        auto* head = static_cast<Frame*>(FlsGetValue(index_));
        if (head == &frame)
        {
            const auto succeeded =
                FlsSetValue(index_, frame.next) != FALSE;
            if (succeeded)
            {
                frame.next = nullptr;
            }
            return succeeded;
        }

        for (auto* current = head;
             current != nullptr;
             current = current->next)
        {
            if (current->next == &frame)
            {
                current->next = frame.next;
                frame.next = nullptr;
                return true;
            }
        }
        return false;
    }

    template<typename Predicate>
    Frame* find(Predicate&& predicate) const noexcept
    {
        if (index_ == FLS_OUT_OF_INDEXES)
        {
            return nullptr;
        }

        for (auto* frame = static_cast<Frame*>(FlsGetValue(index_));
             frame != nullptr;
             frame = frame->next)
        {
            if (std::forward<Predicate>(predicate)(*frame))
            {
                return frame;
            }
        }
        return nullptr;
    }

private:
    DWORD index_{FLS_OUT_OF_INDEXES};
};
}
