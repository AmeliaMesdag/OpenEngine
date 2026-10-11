// MemoryBench: the Memory module's micro-benches, run in Release. Not built by default
// (EXCLUDE_FROM_ALL): build the MemoryBench target and run it.
//
//     MemoryBench ArenaBump [samples]
//
// ArenaBump: one sample is a frame of 4096 bump allocations of 16, 64 and 256 bytes in a
// fixed shuffled order, each written once, then a Memory::Arena Reset; it reports
// nanoseconds per allocation. A comparison builds this bench at both commits and runs the
// two executables in paired visits. The run takes 100 warm-up samples first, then prints
// the median and p99 over the remaining samples.

#include "Memory/Arena.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace
{
using GameEngine::Memory::Arena;

constexpr std::size_t kAllocationsPerSample = 4096;
constexpr std::size_t kWarmupSamples = 100;
constexpr std::size_t kDefaultSamples = 2000;

/// The request sizes of one sample, 16, 64 and 256 bytes in equal shares, in an order fixed by
/// a seeded generator so every run sees the same sequence.
std::vector<std::size_t> MakeSizes()
{
    constexpr std::array<std::size_t, 3> kSizes = {16, 64, 256};
    std::vector<std::size_t> sizes(kAllocationsPerSample);
    std::uint32_t state = 0x9E3779B9u;
    for (std::size_t index = 0; index < sizes.size(); ++index)
    {
        state = state * 1664525u + 1013904223u;
        sizes[index] = kSizes[(state >> 16) % kSizes.size()];
    }
    return sizes;
}

struct Summary
{
    double MedianNanoseconds;
    double P99Nanoseconds;
};

/// The nearest-rank quantile of sorted samples.
double Quantile(const std::vector<double>& sorted, double quantile)
{
    return sorted[static_cast<std::size_t>(quantile * static_cast<double>(sorted.size() - 1))];
}

Summary Summarize(std::vector<double>& samples)
{
    std::sort(samples.begin(), samples.end());
    return {Quantile(samples, 0.5), Quantile(samples, 0.99)};
}

/// Written with every allocation's address, so the bumps cannot be optimized away.
volatile std::uintptr_t g_Sink = 0;

/// Times `samples` frames of `allocate` over `sizes`, each followed by `reset`, after the
/// warm-up; returns nanoseconds per allocation per frame.
template <class Allocate, class Reset>
std::vector<double> MeasureFrames(const std::vector<std::size_t>& sizes, std::size_t samples, Allocate allocate,
                                  Reset reset)
{
    std::vector<double> perAllocation;
    perAllocation.reserve(samples);
    std::uintptr_t sink = 0;
    for (std::size_t sample = 0; sample < kWarmupSamples + samples; ++sample)
    {
        const auto start = std::chrono::steady_clock::now();
        for (const std::size_t bytes : sizes)
        {
            auto* memory = static_cast<unsigned char*>(allocate(bytes));
            *memory = static_cast<unsigned char>(bytes);
            sink ^= reinterpret_cast<std::uintptr_t>(memory);
        }
        const auto end = std::chrono::steady_clock::now();
        reset();
        if (sample >= kWarmupSamples)
            perAllocation.push_back(std::chrono::duration<double, std::nano>(end - start).count() /
                                    static_cast<double>(sizes.size()));
    }
    g_Sink = sink;
    return perAllocation;
}

Summary RunArena(const std::vector<std::size_t>& sizes, std::size_t samples)
{
    Arena arena("MemoryBench.ArenaBump");
    std::vector<double> perAllocation = MeasureFrames(
        sizes, samples, [&arena](std::size_t bytes) { return arena.Allocate(bytes); }, [&arena] { arena.Reset(); });
    return Summarize(perAllocation);
}

void Print(const Summary& summary, std::size_t samples)
{
    std::printf("ArenaBump samples=%zu median_ns_per_alloc=%.4f p99_ns_per_alloc=%.4f\n", samples,
                summary.MedianNanoseconds, summary.P99Nanoseconds);
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || std::string_view(argv[1]) != "ArenaBump")
    {
        std::fprintf(stderr, "usage: MemoryBench ArenaBump [samples]\n");
        return 2;
    }
    const std::size_t samples = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : kDefaultSamples;
    if (samples == 0)
    {
        std::fprintf(stderr, "samples must be a positive count\n");
        return 2;
    }
    Print(RunArena(MakeSizes(), samples), samples);
    return 0;
}
