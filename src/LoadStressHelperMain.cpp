#include <atomic>
#include <cstdlib>
#include <csignal>
#include <vector>

#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

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
