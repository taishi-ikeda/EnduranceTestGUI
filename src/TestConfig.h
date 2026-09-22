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
    // region follows the target window if it moves -- unlike disabling it,
    // which keeps using the same fixed screen coordinates forever
    // (SPEC.md 6.3/8's "既知の制約", addressed in 10). Only meaningful for
    // a region actually used against a single, currently-moving window; a
    // region reused across differently-positioned windows should leave
    // this off.
    //
    // Defaults to true (SPEC.md追加実装及び修正依頼): this only matters for
    // a *freshly default-constructed* NamedRegion -- i.e. MainWindow::
    // onAddNamedRegion()'s starting point for a brand-new region, which is
    // what NamedRegionEditorDialog's "対象ウィンドウの移動に追従させる"
    // checkbox's initial checked state is seeded from. A region loaded from
    // a saved preset always gets this explicitly from the JSON (defaulting
    // to false there if the key is absent, for old presets predating this
    // field) via TestConfigJson::namedRegionFromJson(), so this default
    // never affects deserialization.
    bool followsTargetWindow = true;
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

    // If true, this "step" is actually a task: a fixed, ordered sequence
    // of other steps (taskMembers) that RandomActionEngine runs through in
    // full, in order, exactly once, every time this step's turn comes up
    // in the top-level sequence (SPEC.md 6.2追加実装及び修正依頼 --
    // "タスクはいくつかの操作フローを一つにまとめた操作で、タスク全体を
    // 一つの操作としてください。タスク内で操作はランダムに前後などは
    // しない"). This is the fixed-order counterpart to isGroup above
    // (which instead randomly picks ONE member per action, repeated
    // groupTotalCallCount times): a task has no "total call count" --
    // running a task always means running every member exactly once, in
    // list order, and that whole pass counts as exactly one action for
    // iteration-count purposes, then the top-level sequence advances to
    // the next step. A task's own region/enable*/weight*/actionCount/
    // useDefaultActionParams/customActionParams fields are unused; only
    // taskMembers matters. Members must themselves be plain steps
    // (isGroup == isTask == isWaitStep == false on each) -- same
    // no-nesting/no-wait-member restriction as group members, and for the
    // same reason (a wait "flow" doesn't compose the same way a discrete
    // action does; nesting containers has no well-defined execution
    // order). Mutually exclusive with isGroup/isWaitStep.
    bool isTask = false;
    QList<RegionStep> taskMembers;

    // Only meaningful on a task member (an entry of some other step's
    // taskMembers; see isTask above) -- SPEC.md 6.2追加実装及び修正依頼
    // "タスクの実装に伴って、テスト対象のアプリの操作中に出現したダイアログの
    // 操作をできるようにしてください". When true, this member ignores its
    // own useWholeWindow/regionName entirely and instead operates on
    // whichever top-level window currently belongs to the target process
    // but is *not* the main target window (config.targetWindowId) -- i.e.
    // a dialog/popup a *preceding* task member's action is expected to have
    // just opened (e.g. a confirmation dialog after a right-click menu
    // selection). The member's operation region is that window's entire
    // current bounds (equivalent to useWholeWindow, but against the popup
    // instead of the main window); exclude regions are not supported for
    // it, same as useWholeWindow. If no such extra window exists yet when
    // this member's turn comes up, RandomActionEngine retries the same
    // member on later ticks (the dialog may just not have opened yet)
    // rather than failing immediately, up to a bounded number of attempts
    // -- see RandomActionEngine::m_popupDialogWaitStrikes. This is
    // deliberately task-member-only: a task's fixed execution order is
    // what makes "the previous member's action opened this dialog" a
    // meaningful assumption; a group's per-action random member pick or a
    // plain top-level step have no equivalent "the step right before this
    // one" relationship. enableWindowOp is ignored when this is true (see
    // RandomActionEngine::pickWeightedActionKind): window-level operations
    // always target the main target window specifically
    // (config.targetWindowId), never "whichever window this step
    // resolved to", so they would silently act on the wrong window here.
    bool targetsPopupDialog = false;

    bool hasAnyActionEnabled() const
    {
        if (isWaitStep || isGroup || isTask)
            return true;  // waiting/grouping/tasking is this step's whole purpose, not a missing setting
        return enableClick || enableDoubleClick || enableDrag || enableKey || enableScrollUp ||
               enableScrollDown || enableScrollHorizontal || enableShortcut ||
               (enableWindowOp && !targetsPopupDialog);
    }
};

// What kind of operation a single SetupAction (below) performs.
enum class SetupActionType { Click, DoubleClick, RightClick, Drag, TypeText, KeyPress, Wait };

// One deterministic action performed exactly once, in the order it appears
// in TestConfig::setupActions ("起動時セットアップ", SPEC.md 6.x) --
// unlike RegionStep (which repeats a *randomly chosen* action many times
// within a region), a SetupAction always does the exact same thing every
// run: click this exact point, type this exact text, press this exact key.
// Meant for the fixed login/navigation/configuration sequence some target
// apps require right after launch, before randomized testing should begin
// (e.g. "click the username field, type the account name, press Tab, type
// the password, press Return").
struct SetupAction
{
    SetupActionType type = SetupActionType::Click;

    // Click/DoubleClick/RightClick/Drag(from): position relative to the
    // target window's top-left corner *at the moment this action runs*
    // (RandomActionEngine re-resolves against the window's current bounds
    // every run, the same way a step referencing "対象GUIの全領域" does --
    // there is no separate "follow window" toggle here, since a setup
    // macro re-run against a window that's since moved is the normal case,
    // not an opt-in one).
    QPoint point;
    QPoint dragToPoint;  // Drag only: the release point, also window-relative

    QString text;         // TypeText only: the literal text to type, one key event per character
    QString keySequence;  // KeyPress only: QKeySequence-parseable, e.g. "Return", "Ctrl+A"
    int waitMs = 500;      // Wait only

    // Purely descriptive (e.g. "ユーザー名欄"), shown in the setup list;
    // has no effect on execution. Falls back to a generic per-type
    // description when empty (see MainWindow::describeSetupAction()).
    QString label;
};

// Random action engine configuration. Holds everything the UI collects
// before a run starts; RandomActionEngine only reads from this.
struct TestConfig
{
    qint64 targetPid = -1;
    QString targetAppName;
    quint32 targetWindowId = 0;

    // Fixed, deterministic sequence run exactly once, in order, right
    // after the target is confirmed alive and before the randomized
    // `steps` loop begins (SPEC.md 6.x "起動時セットアップ"). Empty by
    // default (no setup phase, same as before this existed). Whether this
    // is skipped on batch runs after the first (SPEC.md 10 ②) is decided
    // by MainWindow before calling RandomActionEngine::start() -- this
    // field always means "run these now", never "run these only if...".
    QList<SetupAction> setupActions;

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
