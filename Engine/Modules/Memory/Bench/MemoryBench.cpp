// MemoryBench: the Memory module's micro-benches, run in Release. Not built by default
// (EXCLUDE_FROM_ALL): build the MemoryBench target and run it.
//
//     MemoryBench ArenaBump [samples]
//     MemoryBench ObjectPoolChurn <pool|heap> <small|large> [samples]
//     MemoryBench Scratch AB|BA [samples]
//
// ArenaBump: one sample is a frame of 4096 bump allocations of 16, 64 and 256 bytes in a
// fixed shuffled order, each written once, then a Memory::Arena Reset; it reports
// nanoseconds per allocation. A comparison builds this bench at both commits and runs the
// two executables in paired visits.
//
// ObjectPoolChurn: a live set of objects (small: 1,024 of 64 bytes; large: 65,536 of 4 KiB,
// 70 pool blocks) churned by 4096 Delete-then-New pairs per sample, each new object written
// once; it reports nanoseconds per pair. The live-set indices come from one seeded generator
// that continues across samples, so no sample replays another's sequence: an allocator can lay
// out a replayed sequence's objects in visiting order, and the bench would then measure that
// layout instead of the allocator. `pool` uses a Memory::ObjectPool<T>, `heap` the global new
// and delete of the same type, so both arms are this one executable, run alternately in
// paired visits. `pool` also prints the pool's block count from the stats snapshot.
//
// Scratch: one call is a scope opened, allocations of 16, 64 and 256 bytes each written once,
// and the scope closed: (a) as the thread's outermost scope, (b) nested inside an open one.
// Arm A, compiled beside, is the shape ScratchScope replaces: three retained thread_local
// vectors cleared and resized to the same sizes. One sample is 1000 calls; it reports
// nanoseconds per call. Both arms run in one process, in the order given.
//
// Every case takes 100 warm-up samples first, then prints the median and p99 over the
// remaining samples.

#include "Memory/Arena.h"
#include "Memory/ObjectPool.h"
#include "Memory/ScratchScope.h"
#include "Memory/StatsSnapshot.h"

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

constexpr std::size_t kPairsPerSample = 4096;
constexpr std::size_t kSmallLiveObjects = 1024;
constexpr std::size_t kLargeLiveObjects = 65536;
constexpr std::string_view kPoolStatsName = "MemoryBench.ObjectPoolChurn";

struct SmallObject
{
    std::array<std::uint64_t, 8> Words{};
};
static_assert(sizeof(SmallObject) == 64);

struct LargeObject
{
    std::array<std::uint64_t, 512> Words{};
};
static_assert(sizeof(LargeObject) == 4096);

/// Fills a live set of `liveObjects` through `create`, then times samples of Delete-then-New
/// pairs through `destroy` and `create` after the warm-up; returns nanoseconds per pair per
/// sample. The live set is destroyed at the end.
template <class T, class Create, class Destroy>
std::vector<double> MeasureChurn(std::size_t liveObjects, std::size_t samples, Create create, Destroy destroy)
{
    std::vector<T*> live(liveObjects);
    for (T*& object : live)
        object = create();
    std::vector<double> perPair;
    perPair.reserve(samples);
    std::uintptr_t sink = 0;
    std::uint32_t state = 0x2545F491u;
    for (std::size_t sample = 0; sample < kWarmupSamples + samples; ++sample)
    {
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t pair = 0; pair < kPairsPerSample; ++pair)
        {
            state = state * 1664525u + 1013904223u;
            const std::size_t index = (state >> 8) % liveObjects;
            destroy(live[index]);
            T* object = create();
            object->Words[0] = index;
            live[index] = object;
            sink ^= reinterpret_cast<std::uintptr_t>(object);
        }
        const auto end = std::chrono::steady_clock::now();
        if (sample >= kWarmupSamples)
            perPair.push_back(std::chrono::duration<double, std::nano>(end - start).count() /
                              static_cast<double>(kPairsPerSample));
    }
    for (T* object : live)
        destroy(object);
    g_Sink = sink;
    return perPair;
}

std::uint64_t PoolBlockCount()
{
    for (const GameEngine::Memory::StatsEntry& entry : GameEngine::Memory::SnapshotStats())
    {
        if (entry.Name == kPoolStatsName)
            return entry.Blocks;
    }
    return 0;
}

/// Runs one arm of ObjectPoolChurn for `T` and prints its line.
template <class T>
void RunObjectPoolChurn(bool usePool, std::string_view caseName, std::size_t liveObjects, std::size_t samples)
{
    std::vector<double> perPair;
    std::uint64_t blocks = 0;
    if (usePool)
    {
        GameEngine::Memory::ObjectPool<T> pool(kPoolStatsName);
        perPair = MeasureChurn<T>(
            liveObjects, samples, [&pool] { return pool.New(); }, [&pool](T* object) { pool.Delete(object); });
        blocks = PoolBlockCount();
    }
    else
    {
        perPair = MeasureChurn<T>(
            liveObjects, samples, [] { return new T(); }, [](T* object) { delete object; });
    }
    const Summary summary = Summarize(perPair);
    std::printf("ObjectPoolChurn arm=%s case=%.*s live=%zu samples=%zu blocks=%llu median_ns_per_pair=%.4f "
                "p99_ns_per_pair=%.4f\n",
                usePool ? "pool" : "heap", static_cast<int>(caseName.size()), caseName.data(), liveObjects, samples,
                static_cast<unsigned long long>(blocks), summary.MedianNanoseconds, summary.P99Nanoseconds);
}

constexpr std::size_t kScratchCallsPerSample = 1000;
constexpr std::array<std::size_t, 3> kScratchSizes = {16, 64, 256};

/// Times `samples` samples of `kScratchCallsPerSample` calls of `call` after the warm-up;
/// returns nanoseconds per call per sample.
template <class Call>
std::vector<double> MeasureCalls(std::size_t samples, Call call)
{
    std::vector<double> perCall;
    perCall.reserve(samples);
    std::uintptr_t sink = 0;
    for (std::size_t sample = 0; sample < kWarmupSamples + samples; ++sample)
    {
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t index = 0; index < kScratchCallsPerSample; ++index)
            sink ^= call();
        const auto end = std::chrono::steady_clock::now();
        if (sample >= kWarmupSamples)
            perCall.push_back(std::chrono::duration<double, std::nano>(end - start).count() /
                              static_cast<double>(kScratchCallsPerSample));
    }
    g_Sink = sink;
    return perCall;
}

/// One scope: the three allocations, each written once.
std::uintptr_t ScratchCall()
{
    GameEngine::Memory::ScratchScope scratch;
    std::uintptr_t sink = 0;
    for (const std::size_t bytes : kScratchSizes)
    {
        auto* memory = static_cast<unsigned char*>(scratch.Allocate(bytes));
        *memory = static_cast<unsigned char>(bytes);
        sink ^= reinterpret_cast<std::uintptr_t>(memory);
    }
    return sink;
}

/// Arm A: the retained thread_local vectors a scratch scope replaces.
std::uintptr_t VectorCall()
{
    thread_local std::array<std::vector<unsigned char>, 3> t_Vectors;
    std::uintptr_t sink = 0;
    for (std::size_t index = 0; index < kScratchSizes.size(); ++index)
    {
        std::vector<unsigned char>& vector = t_Vectors[index];
        vector.clear();
        vector.resize(kScratchSizes[index]);
        vector[0] = static_cast<unsigned char>(kScratchSizes[index]);
        sink ^= reinterpret_cast<std::uintptr_t>(vector.data());
    }
    return sink;
}

void PrintCase(const char* arm, const char* scratchCase, std::size_t samples, std::vector<double> perCall)
{
    const Summary summary = Summarize(perCall);
    std::printf("Scratch arm=%s case=%s samples=%zu median_ns_per_call=%.4f p99_ns_per_call=%.4f\n", arm,
                scratchCase, samples, summary.MedianNanoseconds, summary.P99Nanoseconds);
}

void RunScratchArm(char arm, std::size_t samples)
{
    if (arm == 'A')
    {
        PrintCase("A", "outermost", samples, MeasureCalls(samples, VectorCall));
        PrintCase("A", "nested", samples, MeasureCalls(samples, VectorCall));
        return;
    }
    PrintCase("B", "outermost", samples, MeasureCalls(samples, ScratchCall));
    GameEngine::Memory::ScratchScope outer;
    static_cast<unsigned char*>(outer.Allocate(64))[0] = 1;
    PrintCase("B", "nested", samples, MeasureCalls(samples, ScratchCall));
}

int Usage()
{
    std::fprintf(stderr, "usage: MemoryBench ArenaBump [samples]\n"
                         "       MemoryBench ObjectPoolChurn <pool|heap> <small|large> [samples]\n"
                         "       MemoryBench Scratch AB|BA [samples]\n");
    return 2;
}

/// The positive sample count at `argv[index]`, the default when absent, or 0 when invalid.
std::size_t ParseSamples(int argc, char** argv, int index)
{
    return argc > index ? std::strtoull(argv[index], nullptr, 10) : kDefaultSamples;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return Usage();
    const std::string_view benchName = argv[1];
    if (benchName == "ArenaBump")
    {
        const std::size_t samples = ParseSamples(argc, argv, 2);
        if (samples == 0)
            return Usage();
        Print(RunArena(MakeSizes(), samples), samples);
        return 0;
    }
    if (benchName == "ObjectPoolChurn" && argc >= 4)
    {
        const std::string_view arm = argv[2];
        const std::string_view caseName = argv[3];
        const std::size_t samples = ParseSamples(argc, argv, 4);
        if ((arm != "pool" && arm != "heap") || (caseName != "small" && caseName != "large") || samples == 0)
            return Usage();
        if (caseName == "small")
            RunObjectPoolChurn<SmallObject>(arm == "pool", caseName, kSmallLiveObjects, samples);
        else
            RunObjectPoolChurn<LargeObject>(arm == "pool", caseName, kLargeLiveObjects, samples);
        return 0;
    }
    if (benchName == "Scratch" && argc > 2)
    {
        const std::string_view order = argv[2];
        const std::size_t samples = ParseSamples(argc, argv, 3);
        if ((order != "AB" && order != "BA") || samples == 0)
            return Usage();
        for (const char arm : order)
            RunScratchArm(arm, samples);
        return 0;
    }
    return Usage();
}
