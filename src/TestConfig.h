#pragma once

#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>
#include <Qt>

enum class DragDirectionMode { Random, Up, Down, Left, Right };

enum class ContextMenuSelectionMode { ByName, ByIndex };

// When RandomActionEngine captures an internal screenshot of the current
// step's operation region (region boundary drawn on top, for later
// inspection via MainWindow's "操作領域画像を保存..." button -- SPEC.md
// 6.2/10). Only the single most recently captured image is kept in memory
// (each new capture overwrites the previous one), not a running history.
enum class ScreenshotCaptureMode { OnceAtStart, PerStepChange, FixedInterval };

// The detailed "how" of each action kind (drag distance/direction, key
// character set, scroll amount, shortcut list, context-menu candidates).
// TestConfig::defaultActionParams is the set edited in the "対象選択"
// panel; each RegionStep either uses that default or carries its own
// customActionParams (toggled as a whole, not per action kind -- see
// SPEC.md 6.9/6.4).
struct ActionParams
{
    int dragMinDistance = 20;
    int dragMaxDistance = 200;
    DragDirectionMode dragDirection = DragDirectionMode::Random;

    QString allowedKeyChars = "abcdefghijklmnopqrstuvwxyz0123456789";

    // Named/functional keys (no printable character of their own) that can
    // also be picked at random for the Key action, alongside allowedKeyChars.
    // These are the keys most likely to trigger focus-traversal / dialog
    // dismiss / list-navigation bugs that plain character input can't reach.
    bool keyIncludeTab = false;
    bool keyIncludeReturn = false;
    bool keyIncludeEscape = false;
    bool keyIncludeBackspace = false;
    bool keyIncludeDelete = false;
    bool keyIncludeArrowKeys = false;  // Up/Down/Left/Right, picked individually at random

    // Shortcut key combinations, as QKeySequence-parseable text (e.g.
    // "Ctrl+C", "Ctrl+Shift+Z"). "Ctrl" follows Qt's portable convention
    // (maps to Cmd on macOS).
    QStringList shortcutSequences = {"Ctrl+C", "Ctrl+V", "Ctrl+A", "Ctrl+Z"};

    // Scroll up and scroll down are configured independently (separate
    // amount ranges; whether each is offered at all and how often is a
    // per-step enable+weight, see RegionStep below) since real-world bugs
    // often only show up scrolling one particular direction (e.g. lazy-
    // loading content only appended when scrolling down).
    int scrollUpMinAmount = 1;
    int scrollUpMaxAmount = 10;
    int scrollDownMinAmount = 1;
    int scrollDownMaxAmount = 10;
    // Horizontal scroll is its own action kind (RegionStep::enableScrollHorizontal
    // /scrollHorizontalWeight), with its own amount range and a randomly
    // picked left/right direction each time it fires.
    int scrollHorizontalMinAmount = 1;
    int scrollHorizontalMaxAmount = 10;

    // Which window-level operations (see ActionKind::WindowOp in
    // RandomActionEngine) are allowed when a step enables that kind.
    bool windowOpMove = true;
    bool windowOpResize = true;
    bool windowOpMinimize = true;
    bool windowOpMaximize = true;

    // Experimental: when a right-click action opens a context/popup menu,
    // try to pick and activate one of its items at random. Two selection
    // modes:
    //  - ByName: pick among contextMenuItemNames that are actually present
    //    in the menu that opened (robust to menus whose item set/order
    //    varies, but requires knowing the exact label text).
    //  - ByIndex: pick among contextMenuIndices (1-based, "Nth from the
    //    top") that are within the opened menu's item count. Simpler to
    //    set up (no label text needed) but riskier if the menu's item
    //    count/order varies between occurrences, since the same index can
    //    then land on a different -- possibly destructive -- item.
    // If no candidate matches/applies, the menu is dismissed with Escape.
    // Relies on OS accessibility trees (macOS Accessibility API / Linux
    // AT-SPI) and has not been exhaustively verified against every
    // toolkit -- see SPEC.md "既知の制約".
    bool enableContextMenuSelection = false;
    ContextMenuSelectionMode contextMenuSelectionMode = ContextMenuSelectionMode::ByName;
    QStringList contextMenuItemNames;  // ByName candidates
    QList<int> contextMenuIndices;     // ByIndex candidates, 1-based
};

// A named, reusable operation region: one or more rectangles (absolute
// screen coordinates) plus mask/exclude sub-rectangles within them. Managed
// as a pool in the "①対象選択" column (add/edit/delete, each given a name)
// so the same region can be referenced by multiple steps in "②ステップ構成"
// without having to redraw it each time -- see SPEC.md 6.3.
struct NamedRegion
{
    QString name;
    QList<QRect> regions;         // screen coordinates, as drawn
    QList<QRect> excludeRegions;  // mask rectangles within `regions`, screen coordinates, as drawn

    // If true, `regions`/`excludeRegions` above are treated as having been
    // drawn while the target window's top-left corner was at
    // `anchorTopLeft`; RandomActionEngine::resolveStepRegion() translates
    // them by (current target top-left - anchorTopLeft) before use, so the
    // region follows the target window if it moves -- unlike the default
    // (false), which keeps using the same fixed screen coordinates forever
    // (SPEC.md 6.3/8's "既知の制約", addressed in 10). Only meaningful for
    // a region actually used against a single, currently-moving window; a
    // region reused across differently-positioned windows should leave
    // this off.
    bool followsTargetWindow = false;
    QPoint anchorTopLeft;

    bool isEmpty() const { return regions.isEmpty(); }
};

// One step of an endurance-test run: a region (either the live target-
// window bounds, or one of TestConfig::namedRegions by name) plus which
// action kinds are enabled while that region is active, and how many
// actions to perform there before advancing to the next step.
// TestConfig::steps is executed in order and loops back to the first step
// once the last one finishes, until a global stop condition (time/iteration
// limit, manual stop, or target crash) is hit.
struct RegionStep
{
    // If true, this step is a pure pause: it performs no actions at all
    // (every field below is ignored) and simply waits waitDurationMs
    // before advancing to the next step. Used to insert a deliberate pause
    // into a step sequence, e.g. to let the target app settle after a
    // burst of activity (SPEC.md 6.2). Added/edited via ②'s "待機を追加..."
    // button rather than StepEditorDialog.
    bool isWaitStep = false;
    int waitDurationMs = 1000;

    // If true, this step operates over the live target-window bounds
    // (re-queried every iteration, so it follows the window if it moves/
    // resizes) and `regionName`/exclude regions are not used. If false,
    // it operates over the NamedRegion in TestConfig::namedRegions whose
    // name matches `regionName` (including that region's own exclude
    // rectangles).
    bool useWholeWindow = true;
    QString regionName;  // used when useWholeWindow == false

    bool enableClick = true;
    bool enableLeftClick = true;
    bool enableRightClick = false;
    // Double-click is its own action kind (always the left button -- a
    // double-right-click has no consistent OS meaning), separate from a
    // plain Click.
    bool enableDoubleClick = false;
    bool enableDrag = false;
    bool enableKey = false;
    bool enableScrollUp = false;
    bool enableScrollDown = false;
    bool enableScrollHorizontal = false;
    bool enableShortcut = false;
    bool enableWindowOp = false;  // move/resize/minimize/maximize the target window itself

    // Relative frequency of each enabled action kind within this step
    // (default 1 each = uniform random, matching the original behavior).
    // Kinds that aren't enabled are simply never picked; their weight is
    // ignored. Weights <= 0 are treated as 1.
    int clickWeight = 1;
    int doubleClickWeight = 1;
    int dragWeight = 1;
    int keyWeight = 1;
    int scrollUpWeight = 1;
    int scrollDownWeight = 1;
    int scrollHorizontalWeight = 1;
    int shortcutWeight = 1;
    int windowOpWeight = 1;

    qint64 actionCount = 50;  // number of actions to perform in this step before moving on

    // Whether this step uses TestConfig::defaultActionParams (drag
    // distance/direction, key charset, scroll amount, shortcuts, context
    // menu candidates) or its own customActionParams instead. Toggled as
    // a whole for the step, not per action kind.
    bool useDefaultActionParams = true;
    ActionParams customActionParams;

    // If true, this "step" is actually a group: a container of other
    // steps (groupMembers) that RandomActionEngine picks from at random
    // (weighted by each member's own groupWeight below), performing
    // exactly one action from the chosen member each time, until
    // groupTotalCallCount actions have been performed in total across the
    // whole group -- then it advances to the next top-level step/group the
    // same way a normal step does once its own actionCount is reached
    // (SPEC.md 6.2). A grouped step's own region/enable*/weight*/
    // actionCount/useDefaultActionParams/customActionParams fields above
    // are unused; only groupMembers and groupTotalCallCount matter. Groups
    // cannot be nested: every entry in groupMembers must itself have
    // isGroup == false. Mutually exclusive with isWaitStep (a group cannot
    // also be a wait step, and wait steps cannot be added as group
    // members -- waiting isn't a per-action thing groupWeight could pick
    // among).
    bool isGroup = false;
    QList<RegionStep> groupMembers;
    qint64 groupTotalCallCount = 50;

    // This step's selection weight when it is itself a member inside
    // some *other* step's groupMembers (SPEC.md 6.2). Meaningless
    // otherwise (a top-level step/group ignores its own groupWeight).
    // Weights <= 0 are treated as 1, same convention as the action-kind
    // weights above.
    int groupWeight = 1;

    bool hasAnyActionEnabled() const
    {
        if (isWaitStep || isGroup)
            return true;  // waiting/grouping is this step's whole purpose, not a missing setting
        return enableClick || enableDoubleClick || enableDrag || enableKey || enableScrollUp ||
               enableScrollDown || enableScrollHorizontal || enableShortcut || enableWindowOp;
    }
};

// Random action engine configuration. Holds everything the UI collects
// before a run starts; RandomActionEngine only reads from this.
struct TestConfig
{
    qint64 targetPid = -1;
    QString targetAppName;
    quint32 targetWindowId = 0;

    // Pool of named, reusable operation regions (see NamedRegion above),
    // managed in the "①対象選択" column. Referenced by name from steps
    // that don't use the whole window.
    QList<NamedRegion> namedRegions;

    QList<RegionStep> steps;

    // Default action parameters, used by any step whose
    // useDefaultActionParams is true.
    ActionParams defaultActionParams;

    int minIntervalMs = 20;
    int maxIntervalMs = 100;

    // Must always be a positive, finite cap (the UI does not offer an
    // "unlimited" choice for any of these three) so a run can never be
    // started without some hard upper bound on how long it can run
    // unattended.
    qint64 maxIterations = 1000;   // global cap across all steps combined
    qint64 maxSequenceLoops = 10;  // counts full passes through `steps`
    int maxDurationSec = 120;

    bool keepTargetActive = true;

    // Controls when RandomActionEngine captures its internal operation-
    // region screenshot (SPEC.md 6.2/10): once right before the first
    // action of a run, every time the current top-level step changes, or
    // every screenshotCaptureIntervalActions actions (that field is only
    // used when the mode is FixedInterval; values <= 0 are treated as 1).
    ScreenshotCaptureMode screenshotCaptureMode = ScreenshotCaptureMode::PerStepChange;
    int screenshotCaptureIntervalActions = 50;

    // If true, RandomActionEngine keeps a rolling buffer of periodic
    // screenshots (see kRecordingFrameIntervalMs/kMaxRecordingFrames in
    // RandomActionEngine.cpp) while running, and writes them out as a
    // numbered sequence of PNG frames when a run stops abnormally --
    // showing the seconds of on-screen activity leading up to the anomaly,
    // not just its final frame (SPEC.md 6.7/10). Off by default: capturing
    // and holding these frames is extra CPU/memory/disk overhead a normal
    // (non-diagnostic) run doesn't need, so this is opt-in per run.
    bool enableScreenRecording = false;

    // If true, when a run stops abnormally RandomActionEngine also does a
    // best-effort search for a native OS crash report/core dump referencing
    // the target process (PlatformAutomation::findRecentCrashReport()) and
    // logs/saves whatever it finds alongside the anomaly screenshots
    // (SPEC.md 6.7/10). On by default: unlike screen recording this is a
    // one-shot filesystem lookup with no ongoing overhead during the run.
    bool enableCrashDumpCollection = true;

    // 0 = pick a fresh random seed each run (and log it). Any other value
    // seeds the run's random generator directly, so the exact same
    // sequence of actions can be reproduced later by re-entering the seed
    // that was logged when a run stopped unexpectedly (crash/anomaly).
    quint32 rngSeed = 0;
};
