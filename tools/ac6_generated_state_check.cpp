#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
struct PendingState
{
    std::array<bool, 32> gpr{};
    std::array<bool, 32> fpr{};
    std::array<bool, 128> vector{};
    std::array<bool, 8> condition{};
    bool lr{};
    bool ctr{};
    bool xer{};
    bool fpscr{};
    bool vscr{};
    bool msr{};

    bool empty() const noexcept
    {
        const auto any = [](const auto& values)
        {
            for (const bool value : values)
            {
                if (value)
                {
                    return true;
                }
            }
            return false;
        };
        return !any(gpr) &&
            !any(fpr) &&
            !any(vector) &&
            !any(condition) &&
            !lr &&
            !ctr &&
            !xer &&
            !fpscr &&
            !vscr &&
            !msr;
    }

    std::string describe() const
    {
        std::ostringstream output;
        const auto appendIndexed =
            [&output](const std::string_view name, const auto& values)
            {
                for (std::size_t index = 0; index < values.size(); ++index)
                {
                    if (values[index])
                    {
                        output << name << index << ' ';
                    }
                }
            };
        appendIndexed("gpr", gpr);
        appendIndexed("fpr", fpr);
        appendIndexed("v", vector);
        appendIndexed("cr", condition);
        if (lr) output << "lr ";
        if (ctr) output << "ctr ";
        if (xer) output << "xer ";
        if (fpscr) output << "fpscr ";
        if (vscr) output << "vscr ";
        if (msr) output << "msr ";
        return output.str();
    }
};

struct PublicationCounts
{
    std::uint64_t iar{};
    std::uint64_t gpr{};
    std::uint64_t fpr{};
    std::uint64_t vector{};
    std::uint64_t condition{};
    std::uint64_t lr{};
    std::uint64_t ctr{};
    std::uint64_t xer{};
    std::uint64_t fpscr{};
    std::uint64_t vscr{};
    std::uint64_t msr{};
};

std::string_view TrimLeft(std::string_view value)
{
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t'))
    {
        value.remove_prefix(1);
    }
    return value;
}

bool ParseIndexAt(
    const std::string_view line,
    const std::string_view prefix,
    const std::size_t position,
    const std::size_t count,
    std::size_t& index)
{
    if (position > line.size() ||
        line.substr(position, prefix.size()) != prefix)
    {
        return false;
    }

    auto cursor = position + prefix.size();
    const auto digitStart = cursor;
    std::size_t value = 0;
    while (cursor < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[cursor])) != 0)
    {
        value = value * 10 +
            static_cast<std::size_t>(line[cursor] - '0');
        ++cursor;
    }
    if (cursor == digitStart || value >= count)
    {
        return false;
    }

    index = value;
    return true;
}

bool ParseIndexAtStart(
    const std::string_view line,
    const std::string_view prefix,
    const std::size_t count,
    std::size_t& index)
{
    return ParseIndexAt(line, prefix, 0, count, index);
}

bool FindIndex(
    const std::string_view line,
    const std::string_view prefix,
    const std::size_t count,
    std::size_t& index)
{
    auto position = line.find(prefix);
    while (position != std::string_view::npos)
    {
        if (ParseIndexAt(line, prefix, position, count, index))
        {
            return true;
        }
        position = line.find(prefix, position + prefix.size());
    }
    return false;
}

bool ParsePublishedIndex(
    const std::string_view line,
    const std::string_view prefix,
    const std::size_t count,
    std::size_t& index)
{
    if (!ParseIndexAtStart(line, prefix, count, index))
    {
        return false;
    }
    const auto close = line.find(')', prefix.size());
    return close != std::string_view::npos &&
        line.substr(close) == ");";
}

bool IsBoundary(const std::string_view line)
{
    return line.starts_with("if") ||
        line.starts_with("goto") ||
        line.starts_with("return") ||
        line.find("(ctx, base)") != std::string_view::npos ||
        line.find("PPC_CALL_") != std::string_view::npos ||
        line.find("PPC_LOAD_") != std::string_view::npos ||
        line.find("PPC_STORE_") != std::string_view::npos ||
        line.find("PPC_MM_") != std::string_view::npos ||
        line.find("PPC_READ_TIME_BASE") != std::string_view::npos;
}

bool Fail(
    const std::filesystem::path& path,
    const std::size_t line,
    const std::string& message)
{
    std::cerr << path.string() << ':' << line << ": " << message << '\n';
    return false;
}

template <std::size_t Size>
bool RemovePublished(
    std::array<bool, Size>& pending,
    const std::size_t index,
    std::uint64_t& count,
    const std::filesystem::path& path,
    const std::size_t line,
    const std::string_view name)
{
    if (!pending[index])
    {
        return Fail(
            path,
            line,
            "unexpected " + std::string(name) +
                std::to_string(index) + " publication");
    }
    pending[index] = false;
    ++count;
    return true;
}

bool RemovePublished(
    bool& pending,
    std::uint64_t& count,
    const std::filesystem::path& path,
    const std::size_t line,
    const std::string_view name)
{
    if (!pending)
    {
        return Fail(
            path,
            line,
            "unexpected " + std::string(name) + " publication");
    }
    pending = false;
    ++count;
    return true;
}

bool ValidateSource(
    const std::filesystem::path& path,
    PublicationCounts& counts)
{
    std::ifstream input(path);
    if (!input)
    {
        return Fail(path, 0, "unable to open generated source");
    }

    PendingState pending{};
    std::string storage;
    std::size_t lineNumber = 0;
    while (std::getline(input, storage))
    {
        ++lineNumber;
        const auto line = TrimLeft(storage);
        if (line.starts_with("PPC_SET_GUEST_IAR(0x") &&
            line.ends_with(");"))
        {
            if (!pending.empty())
            {
                return Fail(
                    path,
                    lineNumber,
                    "unpublished state before IAR: " +
                        pending.describe());
            }
            ++counts.iar;
            continue;
        }

        std::size_t index = 0;
        if (ParsePublishedIndex(
                line,
                "PPC_PUBLISH_GPR(",
                pending.gpr.size(),
                index))
        {
            if (!RemovePublished(
                    pending.gpr,
                    index,
                    counts.gpr,
                    path,
                    lineNumber,
                    "gpr"))
            {
                return false;
            }
            continue;
        }
        if (ParsePublishedIndex(
                line,
                "PPC_PUBLISH_FPR(",
                pending.fpr.size(),
                index))
        {
            if (!RemovePublished(
                    pending.fpr,
                    index,
                    counts.fpr,
                    path,
                    lineNumber,
                    "fpr"))
            {
                return false;
            }
            continue;
        }
        if (ParsePublishedIndex(
                line,
                "PPC_PUBLISH_VECTOR(",
                pending.vector.size(),
                index))
        {
            if (!RemovePublished(
                    pending.vector,
                    index,
                    counts.vector,
                    path,
                    lineNumber,
                    "vector"))
            {
                return false;
            }
            continue;
        }
        if (ParsePublishedIndex(
                line,
                "PPC_PUBLISH_CR(",
                pending.condition.size(),
                index))
        {
            if (!RemovePublished(
                    pending.condition,
                    index,
                    counts.condition,
                    path,
                    lineNumber,
                    "condition"))
            {
                return false;
            }
            continue;
        }

        struct SpecialPublication
        {
            std::string_view text;
            bool PendingState::*pending;
            std::uint64_t PublicationCounts::*count;
            std::string_view name;
        };
        constexpr std::array specials{
            SpecialPublication{"PPC_PUBLISH_LR();", &PendingState::lr, &PublicationCounts::lr, "lr"},
            SpecialPublication{"PPC_PUBLISH_CTR();", &PendingState::ctr, &PublicationCounts::ctr, "ctr"},
            SpecialPublication{"PPC_PUBLISH_XER();", &PendingState::xer, &PublicationCounts::xer, "xer"},
            SpecialPublication{"PPC_PUBLISH_FPSCR();", &PendingState::fpscr, &PublicationCounts::fpscr, "fpscr"},
            SpecialPublication{"PPC_PUBLISH_VSCR();", &PendingState::vscr, &PublicationCounts::vscr, "vscr"},
            SpecialPublication{"PPC_PUBLISH_MSR();", &PendingState::msr, &PublicationCounts::msr, "msr"},
        };
        bool wasSpecial = false;
        for (const auto& special : specials)
        {
            if (line == special.text)
            {
                if (!RemovePublished(
                        pending.*special.pending,
                        counts.*special.count,
                        path,
                        lineNumber,
                        special.name))
                {
                    return false;
                }
                wasSpecial = true;
                break;
            }
        }
        if (wasSpecial)
        {
            continue;
        }

        if (IsBoundary(line) && !pending.empty())
        {
            return Fail(
                path,
                lineNumber,
                "state crossed a control boundary: " +
                    pending.describe());
        }

        if (ParseIndexAtStart(line, "ctx.r", pending.gpr.size(), index))
        {
            pending.gpr[index] = true;
        }
        if (ParseIndexAtStart(line, "ctx.f", pending.fpr.size(), index))
        {
            pending.fpr[index] = true;
        }
        if (ParseIndexAtStart(
                line,
                "ctx.v",
                pending.vector.size(),
                index))
        {
            pending.vector[index] = true;
        }
        if (ParseIndexAtStart(
                line,
                "ctx.cr",
                pending.condition.size(),
                index))
        {
            pending.condition[index] = true;
        }

        const auto firstArgumentEnd = line.find(',');
        const auto firstArgument = line.substr(0, firstArgumentEnd);
        if ((line.starts_with("simde_mm_store_") ||
             line.starts_with("PPC_V")) &&
            FindIndex(
                firstArgument,
                "ctx.v",
                pending.vector.size(),
                index))
        {
            pending.vector[index] = true;
        }
        if (line.starts_with("PPC_V") &&
            FindIndex(
                line,
                "&ctx.cr",
                pending.condition.size(),
                index))
        {
            pending.condition[index] = true;
        }
        if (line.starts_with("PPC_VPKSW") &&
            line.find("ctx.vscr") != std::string_view::npos)
        {
            pending.vscr = true;
        }

        pending.lr = pending.lr || line.starts_with("ctx.lr");
        pending.ctr = pending.ctr ||
            line.starts_with("ctx.ctr") ||
            line.starts_with("--ctx.ctr");
        pending.xer = pending.xer || line.starts_with("ctx.xer");
        pending.fpscr =
            pending.fpscr || line.starts_with("ctx.fpscr");
        pending.vscr =
            pending.vscr || line.starts_with("ctx.vscr");
        pending.msr = pending.msr || line.starts_with("ctx.msr");
    }

    if (!pending.empty())
    {
        return Fail(
            path,
            lineNumber,
            "unpublished state at end of file: " +
                pending.describe());
    }
    return true;
}

bool ValidateMission01Switch(
    const std::vector<std::filesystem::path>& sources)
{
    constexpr std::string_view dispatchMarker =
        "PPC_SET_GUEST_IAR(0x82291764);";
    constexpr std::string_view tableMarker =
        "PPC_SET_GUEST_IAR(0x82291768);";
    constexpr std::array requiredDispatch{
        std::string_view{"switch (ctx.r8.u64)"},
        std::string_view{"goto loc_8229177C;"},
        std::string_view{"goto loc_822917CC;"},
        std::string_view{"goto loc_82291804;"},
        std::string_view{"goto loc_82291928;"},
        std::string_view{"goto loc_82291960;"},
    };

    std::size_t matches = 0;
    for (const auto& path : sources)
    {
        std::ifstream input(path);
        if (!input)
        {
            return Fail(path, 0, "unable to open generated source");
        }
        std::ostringstream storage;
        storage << input.rdbuf();
        const auto source = storage.str();
        const auto dispatch = source.find(dispatchMarker);
        if (dispatch == std::string::npos)
        {
            continue;
        }
        if (++matches != 1 ||
            source.find(dispatchMarker, dispatch + dispatchMarker.size()) !=
                std::string::npos)
        {
            return Fail(
                path,
                0,
                "Mission 01 switch dispatch marker is not unique");
        }

        const auto table = source.find(tableMarker, dispatch);
        if (table == std::string::npos)
        {
            return Fail(
                path,
                0,
                "Mission 01 switch table marker is missing");
        }
        const std::string_view dispatchBody(
            source.data() + dispatch,
            table - dispatch);
        for (const auto fragment : requiredDispatch)
        {
            if (dispatchBody.find(fragment) == std::string_view::npos)
            {
                return Fail(
                    path,
                    0,
                    "Mission 01 switch dispatch is missing " +
                        std::string(fragment));
            }
        }
        if (dispatchBody.find("PPC_CALL_INDIRECT_FUNC") !=
            std::string_view::npos)
        {
            return Fail(
                path,
                0,
                "Mission 01 switch still exits through indirect dispatch");
        }
    }

    if (matches != 1)
    {
        return Fail(
            sources.front().parent_path(),
            0,
            "Mission 01 switch dispatch was not generated");
    }
    return true;
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr
            << "Usage: ac6_generated_state_check <generated-directory>\n";
        return EXIT_FAILURE;
    }

    const std::filesystem::path generatedRoot = argv[1];
    std::vector<std::filesystem::path> sources;
    for (const auto& entry :
         std::filesystem::directory_iterator(generatedRoot))
    {
        const auto name = entry.path().filename().string();
        if (entry.is_regular_file() &&
            name.starts_with("ppc_recomp.") &&
            entry.path().extension() == ".cpp")
        {
            sources.push_back(entry.path());
        }
    }
    std::sort(sources.begin(), sources.end());
    if (sources.empty())
    {
        std::cerr << "No generated PPC sources were found.\n";
        return EXIT_FAILURE;
    }
    if (!ValidateMission01Switch(sources))
    {
        return EXIT_FAILURE;
    }

    PublicationCounts counts{};
    for (const auto& source : sources)
    {
        if (!ValidateSource(source, counts))
        {
            return EXIT_FAILURE;
        }
    }

    constexpr PublicationCounts expected{
        879801,
        441015,
        67515,
        34625,
        60901,
        53031,
        10115,
        3227,
        22379,
        2,
        151,
    };
    const bool countsMatch =
        sources.size() == 73 &&
        counts.iar == expected.iar &&
        counts.gpr == expected.gpr &&
        counts.fpr == expected.fpr &&
        counts.vector == expected.vector &&
        counts.condition == expected.condition &&
        counts.lr == expected.lr &&
        counts.ctr == expected.ctr &&
        counts.xer == expected.xer &&
        counts.fpscr == expected.fpscr &&
        counts.vscr == expected.vscr &&
        counts.msr == expected.msr;
    std::cout
        << "sources=" << sources.size()
        << " iar=" << counts.iar
        << " gpr=" << counts.gpr
        << " fpr=" << counts.fpr
        << " vector=" << counts.vector
        << " condition=" << counts.condition
        << " lr=" << counts.lr
        << " ctr=" << counts.ctr
        << " xer=" << counts.xer
        << " fpscr=" << counts.fpscr
        << " vscr=" << counts.vscr
        << " msr=" << counts.msr
        << '\n';
    if (!countsMatch)
    {
        std::cerr << "Pinned AC6 publication counts changed.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
