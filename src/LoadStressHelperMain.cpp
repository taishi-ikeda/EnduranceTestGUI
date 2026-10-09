#include <atomic>
#include <cstdlib>
#include <csignal>
#include <vector>

namespace
{
std::atomic<bool> g_stop{false};
void handleSignal(int) { g_stop.store(true); }
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
// Takes one optional argument: how many MB to allocate and keep touching
// (default 64). Anything unparseable is ignored and the default is used.
int main(int argc, char **argv)
{
    std::signal(SIGTERM, handleSignal);
    std::signal(SIGINT, handleSignal);

    long memoryMb = 64;
    if (argc > 1) {
        char *end = nullptr;
        const long parsed = std::strtol(argv[1], &end, 10);
        if (end != argv[1] && parsed > 0)
            memoryMb = parsed;
    }

    const std::size_t byteCount = static_cast<std::size_t>(memoryMb) * 1024 * 1024;
    // std::vector<T> requires a non-cv-qualified value_type, so the buffer
    // itself is plain unsigned char; every access below instead goes through
    // a volatile pointer to it, so the compiler can't prove the touching
    // writes are dead and optimize them away entirely.
    std::vector<unsigned char> memoryBlock(byteCount, 0);
    volatile unsigned char *memoryPtr = memoryBlock.data();

    std::size_t touchCursor = 0;
    volatile double busySink = 0.0;
    while (!g_stop.load(std::memory_order_relaxed)) {
        // CPU load: pure floating-point churn with no syscalls/sleeps in the
        // hot path, so this one thread saturates whichever core it's
        // scheduled on. The inner bound (not a bare `while (!g_stop)`) keeps
        // the stop flag checked often enough to exit promptly on
        // terminate(), without the check itself being the bottleneck.
        for (int i = 0; i < 200000 && !g_stop.load(std::memory_order_relaxed); ++i)
            busySink += static_cast<double>(i) * 1.0000001;

        // Memory load: keep touching the allocated block a little at a time
        // so the OS can't quietly page it out and the allocation can't be
        // optimized away, without memory traffic dominating runtime over
        // the CPU churn above.
        if (byteCount > 0) {
            for (int i = 0; i < 4096 && touchCursor < byteCount; ++i, ++touchCursor)
                memoryPtr[touchCursor] = static_cast<unsigned char>(touchCursor);
            if (touchCursor >= byteCount)
                touchCursor = 0;
        }
    }
    return 0;
}
