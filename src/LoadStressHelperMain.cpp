#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace
{
std::atomic<bool> g_stop{false};
void handleSignal(int) { g_stop.store(true); }

// 「メモリへの負荷の掛け方として二つのオプションを選べるようにしてください」:
// how the requested MB are laid out in the process's address space, not just
// how many of them there are -- one large contiguous block maximizes
// resident-memory/OOM pressure while staying cheap on the TLB (and eligible
// for transparent huge pages), whereas many separate smaller mappings
// exercise the kernel's VMA bookkeeping and the TLB/page-fault path harder
// per byte touched, at the cost of being gentler on raw resident-size
// pressure. Each chunk is requested via a direct, separate mmap() call
// (MAP_PRIVATE|MAP_ANONYMOUS) rather than many small heap allocations, so
// fragmentation is genuine (distinct VMAs/mappings) regardless of whatever
// threshold the platform's own malloc happens to use for falling back to
// mmap internally.
constexpr std::size_t kFragmentedChunkBytes = 256 * 1024;

// Size of the unmapped-access (PROT_NONE) guard region placed after every
// fragmented-mode chunk -- see that branch's own comment for why it's
// needed at all (preventing the kernel from merging adjacent chunks back
// into one VMA), and for why it's now carved out of the *same* mmap() call
// as its chunk (不具合報告F-02) rather than requested as its own separate
// mmap(). Also doubles as the stride used to prefault every page of a
// chunk right after mapping it (不具合報告F-01, see that loop's own
// comment) -- one page is enough for either purpose, and this is at least
// as large as any real page size this is likely to run on (4KiB on
// x86_64; touching more often than a larger real page size requires is
// harmless, just slightly redundant).
constexpr std::size_t kGuardPageBytes = 4096;

// 不具合報告F-03(中): upper bound on how many (chunk + guard) mapping pairs
// fragmented mode will ever create, however large the requested total is --
// each pair costs 2 VMAs, and Linux's default vm.max_map_count (65530) is a
// hard per-process ceiling on how many a process can ever hold at once; once
// hit, mmap() starts failing not just for further chunks here but
// potentially for anything else this process might otherwise need to
// allocate too. Rather than silently stop fragmenting partway through an
// oversized request (the previous behavior -- found, for a large enough
// request, to silently cap out around 8GB regardless of how much more was
// asked for), the chunk size itself scales up with the requested total
// (see byteCount/kMaxFragmentCount below) so the mapping count this loop
// can ever produce stays well under that ceiling at any size this helper's
// own command-line MB argument can realistically request. Ordinary
// requests (well under kMaxFragmentCount * kFragmentedChunkBytes, i.e. up
// to about 2GB) are completely unaffected -- kFragmentedChunkBytes itself
// still wins the std::max() below, so chunk size/fragmentation granularity
// stays exactly as before.
constexpr std::size_t kMaxFragmentCount = 8192;
}  // namespace

// Standalone CPU+memory load-generator helper, launched as a separate OS
// process by LoadInjector (never as a thread inside EnduranceTestGUI's own
// process -- see LoadInjector.h's header comment for why). Spins one CPU
// core at ~100% and continually touches a fixed block of memory until
// terminated (SIGTERM/SIGINT), simulating "another piece of software's
// CPU/memory load on the same machine" for reproducing load/timing-
// dependent target-app crashes (SPEC.md 追加実装依頼「負荷注入モード」).
//
// Deliberately has no Qt dependency: it only ever needs to burn CPU and
// memory, so keeping it a minimal, fast-to-build, dependency-free binary
// avoids any risk of it competing for the same libraries/resources the main
// app and the target app are also using.
//
// Takes up to two optional arguments: how many MB to allocate and keep
// touching (default 64; anything unparseable is ignored and the default is
// used), and the memory layout mode -- "single" (default: one contiguous
// block, see memoryBlock below) or "fragmented" (many separate
// kFragmentedChunkBytes-sized mmap() mappings scattered across the address
// space instead of one mapping for the whole amount -- see
// kFragmentedChunkBytes' own comment for why that changes how the load
// actually behaves). Anything other than exactly "fragmented" is treated as
// "single".
int main(int argc, char **argv)
{
    std::signal(SIGTERM, handleSignal);
    std::signal(SIGINT, handleSignal);

    // 不具合報告(無人実行・共有環境で静かに眠るもの) 1.2: this process used to
    // have no mechanism at all for noticing its parent (EnduranceTestGUI)
    // is gone -- a clean shutdown always sent SIGTERM via LoadInjector's own
    // terminate() first, but a `kill -9`/crash/OOM-kill of the parent skips
    // that entirely, leaving this process spinning a CPU core forever with
    // nothing left able to stop it. Two layers: on Linux, ask the kernel to
    // deliver SIGTERM itself the instant the parent dies, for any reason
    // (PR_SET_PDEATHSIG is Linux-only -- no portable equivalent exists);
    // everywhere (including the narrow race where the parent already died
    // between fork/exec and this prctl() call, so no signal is ever coming),
    // fall back to noticing that this process has been reparented -- an
    // orphan is always handed to a new parent (traditionally PID 1) by the
    // OS, so a getppid() that no longer matches the parent recorded at
    // startup means the original parent is gone.
    const pid_t originalParentPid = getppid();
#if defined(__linux__)
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    // If the parent already exited between fork/exec and the prctl() call
    // above, no SIGTERM is ever coming (the kernel only arms the signal for
    // deaths *after* this point) -- but a dead original parent means we've
    // already been reparented, so catch that directly instead of waiting
    // for the poll fallback below to notice on its next pass.
    if (getppid() != originalParentPid)
        g_stop.store(true);
#endif

    long memoryMb = 64;
    if (argc > 1) {
        char *end = nullptr;
        const long parsed = std::strtol(argv[1], &end, 10);
        if (end != argv[1] && parsed > 0)
            memoryMb = parsed;
    }
    const bool fragmented = argc > 2 && std::strcmp(argv[2], "fragmented") == 0;

    const std::size_t byteCount = static_cast<std::size_t>(memoryMb) * 1024 * 1024;

    // One contiguous region to touch (single mode), or several scattered
    // ones (fragmented mode) -- see each branch's own comment. Either way,
    // `spans` below is what the touching loop actually walks, so it doesn't
    // need to know or care which mode produced it.
    struct Span
    {
        volatile unsigned char *ptr;
        std::size_t size;
    };
    std::vector<Span> spans;

    // std::vector<T> requires a non-cv-qualified value_type, so the buffer
    // itself is plain unsigned char; every access below instead goes through
    // a volatile pointer to it, so the compiler can't prove the touching
    // writes are dead and optimize them away entirely. Declared here (not
    // inside the `if` below) so it stays alive for the rest of main(), since
    // `spans` only borrows a pointer into it.
    std::vector<unsigned char> memoryBlock;

    if (!fragmented) {
        memoryBlock.assign(byteCount, 0);
        if (byteCount > 0)
            spans.push_back({memoryBlock.data(), byteCount});
    } else {
        // Never explicitly munmap()'d -- the process exiting at the end of
        // main() reclaims every mapping just as it would memoryBlock's heap
        // storage above; nothing other than this process ever needs any of
        // them back sooner.
        //
        // 不具合報告F-03: scales up with the request so the mapping count
        // below stays bounded -- see kMaxFragmentCount's own comment.
        // Rounded up to a multiple of kGuardPageBytes (this platform's own
        // page size, in practice): mprotect() below requires its address
        // argument to be page-aligned, and silently fails (EINVAL) if it
        // isn't -- found, while verifying this very scaling, that an
        // unrounded chunk size (e.g. 9000MB/kMaxFragmentCount = 1,152,000
        // bytes, not a multiple of 4096) left every guard's mprotect() call
        // failing this way, with the return value going unchecked, so every
        // chunk's own guard silently never took effect -- right back to
        // vma_merge() coalescing adjacent chunks into a handful of huge
        // VMAs instead of thousands of genuinely separate ones.
        const std::size_t rawChunkBytes = byteCount / kMaxFragmentCount;
        const std::size_t chunkBytes =
            rawChunkBytes > kFragmentedChunkBytes
                ? ((rawChunkBytes + kGuardPageBytes - 1) / kGuardPageBytes) * kGuardPageBytes
                : kFragmentedChunkBytes;

        // A PROT_NONE guard region follows each data chunk, carved out of
        // the very same mmap() call as its chunk via mprotect() (不具合報告
        // F-02(中)) rather than requested as its own separate mmap(): the
        // kernel decides where each independent mmap() call's own address
        // range actually lands, with no guarantee that two consecutive
        // calls end up adjacent to each other -- found, while verifying
        // this very guard-page mechanism, to leave a meaningful fraction of
        // guards *not* adjacent to the chunk they were meant to separate
        // (landing in some other address-space gap instead), in which case
        // the *chunks* on either side of that gap ended up adjacent to each
        // other and sharing the same protection/flags instead -- right back
        // to the kernel's own vma_merge() silently coalescing them into one
        // VMA, the exact failure this whole mechanism exists to prevent
        // (see kGuardPageBytes' own comment for vma_merge() itself). A
        // single mmap() call spanning chunk+guard together has no such
        // ambiguity: whatever address the kernel picks, the guard is
        // necessarily the tail end of that same contiguous range.
        std::size_t remaining = byteCount;
        while (remaining > 0) {
            const std::size_t chunkSize = remaining < chunkBytes ? remaining : chunkBytes;
            const std::size_t mappingSize = chunkSize + kGuardPageBytes;
            void *mapped = ::mmap(nullptr, mappingSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapped == MAP_FAILED) {
                // 不具合報告F-03: report, rather than silently stop here --
                // this process's own stderr isn't currently surfaced into
                // EnduranceTestGUI's own log (LoadInjector only watches for
                // this process exiting, not anything it prints), but this
                // is still strictly better than no record at all existing
                // anywhere (e.g. for a user who redirects/checks this
                // process's own output directly, or a future enhancement
                // that does relay it).
                std::fprintf(stderr,
                             "loadstress_helper: fragmented allocation stopped after %zu of the requested "
                             "%zu MB (mmap failed, errno=%d) -- likely hit this process's vm.max_map_count\n",
                             (byteCount - remaining) / (1024 * 1024), byteCount / (1024 * 1024), errno);
                break;
            }
            // 不具合報告F-02: the guard is this mapping's own trailing
            // kGuardPageBytes -- see this branch's own top comment.
            ::mprotect(static_cast<unsigned char *>(mapped) + chunkSize, kGuardPageBytes, PROT_NONE);
            // 不具合報告F-01: fault every page of this chunk into residency
            // right now, matching single mode's own std::vector(byteCount, 0)
            // above (which faults its whole allocation in immediately, as a
            // side effect of zero-filling it). Without this, only the
            // touching loop below ever faults pages in, at a fixed rate (a
            // few KB per outer loop pass) regardless of how many chunks
            // there are to get through first --
            // observed taking on the order of minutes to become fully
            // resident for a GB-scale request, during which a short test run
            // could easily finish before the configured amount of memory
            // pressure was ever actually applied at all.
            for (std::size_t off = 0; off < chunkSize; off += kGuardPageBytes)
                static_cast<volatile unsigned char *>(mapped)[off] = 0;
            spans.push_back({static_cast<volatile unsigned char *>(mapped), chunkSize});
            remaining -= chunkSize;
        }
    }

    std::size_t spanIndex = 0;
    std::size_t offsetInSpan = 0;
    volatile double busySink = 0.0;
    while (!g_stop.load(std::memory_order_relaxed)) {
        // CPU load: pure floating-point churn with no syscalls/sleeps in the
        // hot path, so this one thread saturates whichever core it's
        // scheduled on. The inner bound (not a bare `while (!g_stop)`) keeps
        // the stop flag checked often enough to exit promptly on
        // terminate(), without the check itself being the bottleneck.
        for (int i = 0; i < 200000 && !g_stop.load(std::memory_order_relaxed); ++i)
            busySink += static_cast<double>(i) * 1.0000001;

        // Memory load: keep touching the allocated memory a little at a
        // time so the OS can't quietly page it out and the allocation can't
        // be optimized away, without memory traffic dominating runtime over
        // the CPU churn above. Walks `spans` round-robin -- a single span in
        // single mode (so this behaves exactly as before), many in
        // fragmented mode (so every mapping gets touched in turn, not just
        // the first one).
        if (!spans.empty()) {
            for (int i = 0; i < 4096; ++i) {
                Span &span = spans[spanIndex];
                span.ptr[offsetInSpan] = static_cast<unsigned char>(offsetInSpan);
                ++offsetInSpan;
                if (offsetInSpan >= span.size) {
                    offsetInSpan = 0;
                    spanIndex = (spanIndex + 1) % spans.size();
                }
            }
        }

        // Portable orphan check, once per outer pass (so at worst a fraction
        // of a second after the parent actually dies): covers macOS, where
        // there is no PDEATHSIG equivalent at all, and backstops Linux in
        // case the signal above is ever missed. See this function's top
        // comment for why this matters on a shared machine.
        if (getppid() != originalParentPid)
            break;
    }
    return 0;
}
