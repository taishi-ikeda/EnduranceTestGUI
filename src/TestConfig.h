#pragma once

#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>
#include <Qt>

enum class DragDirectionMode { Random, Up, Down, Left, Right };

enum class ContextMenuSelectionMode { ByName, ByIndex };

// SPEC.md 追加実装依頼「異常入力ファジング」: how a chosen fuzz-string
// corpus entry (ActionKind::FuzzString) actually reaches the target.
// KeyByKey sends it one character at a time via the same keyTap()/
// keyTapNamed() mechanism ActionKind::Key uses (so it exercises per-
// keystroke input handling, but -- a pre-existing limitation of keyTap()'s
// one-QChar-at-a-time X11 keysym mapping, not something this feature tries
// to work around -- can't reliably deliver characters outside the Latin-1
// range, e.g. most emoji or combining marks). Paste writes the whole
// string to the clipboard and sends Ctrl+V, so it reaches the target as a
// single paste event regardless of its content (correctly handling any
// Unicode), exercising a different code path than per-keystroke input
// (some apps validate as you type but not on paste). Random picks between
// the two, independently, each time this action fires, so a single step
// exercises both without needing two separately-weighted entries.
enum class FuzzStringDeliveryMode { KeyByKey, Paste, Random };

// When RandomActionEngine captures an internal screenshot of the current
// step's operation region (region boundary drawn on top, for later
// inspection via MainWindow's "操作領域画像を保存..." button -- SPEC.md
// 6.2/10). Only the single most recently captured image is kept in memory
// (each new capture overwrites the previous one), not a running history.
enum class ScreenshotCaptureMode { OnceAtStart, PerStepChange, FixedInterval };

// --- SPEC.md 追加実装依頼「ビューワー耐久テスト強化」 -----------------------
// A 3D viewer (the motivating case: VTK's default trackball-camera
// interaction style, though none of this is VTK-specific) typically binds
// camera manipulation to a long mouse-button hold with continuous
// movement, not an instantaneous drag -- and different button/modifier
// combinations select different camera operations (plain left = rotate,
// Shift+left = pan, Ctrl+left = spin, middle = pan, right = zoom being
// VTK's own bindings). RegionStep::enableHoldDrag/ActionKind::HoldDrag
// generalizes the existing instantaneous Drag into exactly that: press a
// button (optionally with modifier keys held) down, move the pointer
// along a path for a randomized duration -- optionally layering wheel/key/
// window-resize input on top while still held, to reproduce a target
// concurrently handling more than one kind of input mid-gesture -- then
// release.

enum class HoldDragPathShape { Circle, ZigZag, RandomWalk };

// One (button, modifiers) combination HoldDrag can pick for a given
// occurrence, weighted the same way every other candidate list in this
// app is (ActionParams::holdDragStyles). An empty holdDragStyles list
// falls back to a single implicit {LeftButton, NoModifier, weight 1}
// entry, so a preset that predates this feature keeps behaving like a
// plain left-button hold.
struct HoldDragStyle
{
    Qt::MouseButton button = Qt::LeftButton;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    int weight = 1;
};

// Injects wheel "notches" while a HoldDrag is in progress (SPEC.md 想定
// パターンP3「ドラッグ中のホイール」/P6「描画より速いホイール」、および
// 「0msの連打」「連打と完全停止の繰り返し」): alternates between a burst
// of notchesPerBurst[Min,Max] notches sent notchInterval[Min,Max]Ms apart,
// then a pause of pauseBetweenBursts[Min,Max]Ms with no wheel activity,
// repeating for as long as the hold lasts. The defaults (a single notch,
// no pause) mean "one notch every notchIntervalMs" -- the simple P3 case;
// an interval of (0, 0) with a longer pause gives "as fast as possible,
// then a dead stop" (P6/"0ms連打"), and widening notchesPerBurst covers
// "N notches back to back" before each pause.
struct HoldDragWheelOverlay
{
    bool enabled = false;
    int notchesPerBurstMin = 1;
    int notchesPerBurstMax = 1;
    int notchIntervalMinMs = 30;
    int notchIntervalMaxMs = 30;
    int pauseBetweenBurstsMinMs = 0;
    int pauseBetweenBurstsMaxMs = 0;
};

// Sends one keystroke at a time -- the same allowedKeyChars/keyInclude*
// candidate pool ActionKind::Key/TypingBurst already draw from, there is
// no separate charset for this -- at a randomized interval while a
// HoldDrag is in progress ("ドラッグ中にキー").
struct HoldDragKeyOverlay
{
    bool enabled = false;
    int keyIntervalMinMs = 300;
    int keyIntervalMaxMs = 800;
};

// Resizes, or minimizes then restores, the target window at a randomized
// interval while a HoldDrag is in progress (SPEC.md 想定パターンP5「ドラッ
// グ中のウィンドウ操作」): stresses the target's render-surface-recreation
// path concurrently with an in-progress camera gesture. Always acts on the
// main target window (TestConfig::targetWindowId), the same as
// ActionKind::WindowOp -- covering a window of some OTHER, unrelated
// application being raised over the target mid-gesture is not supported
// (SPEC.md「既知の制約」).
struct HoldDragWindowOpOverlay
{
    bool enabled = false;
    bool allowResize = true;
    bool allowMinimizeRestore = true;
    int intervalMinMs = 800;
    int intervalMaxMs = 2000;
};

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

    // 一定時間キーボード入力 (RegionStep::enableTypingBurst, ActionKind::
    // TypingBurst): unlike a plain Key action (exactly one keystroke per
    // occurrence), this keeps sending keystrokes at randomized short
    // intervals for a randomized total duration before counting as the one
    // action its weight picked. Draws from the exact same character/named-
    // key candidate pool as Key above (allowedKeyChars + keyInclude*) --
    // there is no separate charset for it.
    int typingBurstMinDurationMs = 2000;
    int typingBurstMaxDurationMs = 5000;
    int typingBurstKeyIntervalMinMs = 50;
    int typingBurstKeyIntervalMaxMs = 200;

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

    // Candidate button/widget names for RegionStep::enableDialogButtonPress
    // (SPEC.md 6.2追加実装及び修正依頼) -- one is picked at random among
    // whichever of these are actually found (by accessible name) in the
    // popup dialog when this action kind fires. Same accessibility-tree
    // caveats as enableContextMenuSelection above.
    QStringList dialogButtonNames;

    // SPEC.md 追加実装依頼「異常入力ファジング」 (RegionStep::enableFuzzString,
    // ActionKind::FuzzString): true (default) picks a candidate uniformly
    // at random from the entire app-wide fuzz-string corpus
    // (TestConfig::fuzzStringCorpus) every time this action fires. false
    // restricts the random pick to just fuzzStringSelectedEntries below --
    // a literal copy of whichever corpus entries the user checked in this
    // step's own settings (not a reference by index/name into the corpus),
    // so it stays meaningful even if the corpus is later reordered or
    // trimmed, and remains reproducible when this preset is shared.
    bool fuzzStringUseAllEntries = true;
    QStringList fuzzStringSelectedEntries;
    FuzzStringDeliveryMode fuzzStringDeliveryMode = FuzzStringDeliveryMode::Random;

    // SPEC.md 追加実装依頼「ビューワー耐久テスト強化」(RegionStep::
    // enableHoldDrag, ActionKind::HoldDrag) -- see HoldDragStyle's own
    // comment for holdDragStyles' empty-list fallback.
    QList<HoldDragStyle> holdDragStyles;
    int holdDragDurationMinMs = 2000;
    int holdDragDurationMaxMs = 5000;
    HoldDragPathShape holdDragPathShape = HoldDragPathShape::Circle;
    // How far from the start point the path wanders, in pixels (Circle:
    // radius; ZigZag: amplitude of each zigzag leg; RandomWalk: the walk
    // is pulled back toward the start point once it strays past this far).
    int holdDragPathExtentMinPx = 20;
    int holdDragPathExtentMaxPx = 150;
    // How often the pointer advances to a new point along the path while
    // held -- SPEC.md 想定パターンP1「長押し回転」の「数百回動かす」は、
    // 専用の移動回数設定ではなく、長い継続時間とこの短い間隔の組み合わせ
    // から自然に生じる。
    int holdDragStepIntervalMinMs = 10;
    int holdDragStepIntervalMaxMs = 30;
    HoldDragWheelOverlay holdDragWheelOverlay;
    HoldDragKeyOverlay holdDragKeyOverlay;
    HoldDragWindowOpOverlay holdDragWindowOpOverlay;

    // SPEC.md 追加実装依頼「ビューワー耐久テスト強化」「現状では右ドラッグ
    // のたびにEscapeが入る…3Dビューでは右ドラッグはズームなので、メニュー
    // が出ないのにEscapeが送られ、実行中の処理を取り消す可能性がある」: a
    // 3D viewer commonly binds the right mouse button to a camera operation
    // (zoom) that never opens a context/popup menu, unlike most ordinary
    // applications. Without this flag, every right-button Click/Drag/
    // HoldDrag still goes through handlePossibleContextMenu()'s "resolve or
    // dismiss" logic, which sends Escape whenever nothing was selected --
    // silently cancelling whatever the viewer itself was in the middle of,
    // once per right-button action. true skips that handling entirely for
    // this step's right-button actions (treated as "no menu could possibly
    // have appeared"); false (default) preserves the existing behavior for
    // every app that does use the right button for a real context menu.
    bool viewerRightButtonNeverOpensMenu = false;
};

// A named, reusable operation target resolved via the OS accessibility tree
// (AT-SPI on Linux / Accessibility API on macOS) instead of fixed screen
// coordinates -- SPEC.md 追加実装依頼「名前付きオブジェクト」. Only
// meaningful when NamedRegion::isObjectTarget is true, in which case it
// replaces `regions`/`excludeRegions` entirely: RandomActionEngine searches
// the target app's accessibility tree for an object whose role+name match
// (role/name are stored verbatim as the backend reports them -- e.g. Linux's
// `atspi_accessible_get_role_name()` returns strings like "push button",
// "menu item", "check box"; macOS's kAXRoleAttribute returns strings like
// "AXButton", "AXMenuItem" -- so an ObjectTarget registered on one platform
// is not portable to the other, same as any other platform-specific
// automation detail this tool already has), then uses whatever bounding box
// that object currently has as the step's region, same as any other
// NamedRegion from that point on -- so weighted random click/drag/key/
// scroll/action-count logic is entirely unchanged; only *where the rectangle
// comes from* differs. Searching the tree is comparatively expensive (D-Bus
// IPC on Linux, cross-process AX calls on macOS), so RandomActionEngine
// caches the resolved object (see AccessibleObjectHandle) rather than
// re-searching before every single action -- see reresolveEveryActions.
// 不具合報告D28(低): how ObjectTarget::name is compared against each
// candidate's own accessible name. Exact (default) preserves every existing
// preset's behavior unchanged. A widget whose own name changes at runtime
// (e.g. TestTarget's own buttons, which append their running click counts --
// "ボタン1\n左:20 ...") can never re-resolve by exact name after the first
// run that actually clicked it -- StartsWith lets a fixed prefix still
// match, and Regex allows an arbitrary pattern for names that vary in the
// middle or have no stable prefix at all.
enum class ObjectNameMatchMode
{
    Exact,
    StartsWith,
    Regex
};

struct ObjectTarget
{
    QString role;  // accessible role, exactly as the backend reports it
    QString name;  // accessible name/label to match -- interpreted per nameMatchMode
    ObjectNameMatchMode nameMatchMode = ObjectNameMatchMode::Exact;

    // 0-based: which match to use among multiple objects that share the
    // same role+name (common for e.g. several same-styled buttons with no
    // distinguishing label). Recorded automatically by the object-picker
    // overlay when more than one match exists for the point the user
    // clicked.
    int occurrenceIndex = 0;

    // false (default): a Click action performed while this region is active
    // picks a random point within the resolved object's current bounding
    // box and synthesizes an ordinary mouse click there, exactly like any
    // other region -- Drag/Scroll/Key/etc. always work this way regardless
    // of this flag, since those have no equivalent "invoke it directly"
    // operation.
    // true: a Click action instead calls AccessibleObjectHandle::
    // performDefaultAction() on the cached object reference directly (the
    // object's registered default action -- "press" for a button or menu
    // item, "toggle" for a check box, etc.), without synthesizing any mouse
    // event or needing a valid on-screen point at all. Appropriate for
    // objects you want operated semantically rather than by coordinate --
    // e.g. a menu-bar item, whose on-screen position reliably exists but
    // whose default action is more direct and less layout-sensitive than a
    // synthetic click would be.
    bool useDefaultAction = false;

    // 0 (default): resolve this object once when a step starts using this
    // region (i.e. when RandomActionEngine's current top-level step/group/
    // task member switches to one referencing this NamedRegion) and reuse
    // that same cached reference for every action for as long as that same
    // step keeps running, only re-resolving if the cached reference becomes
    // invalid (e.g. the object was destroyed -- a dialog closed, a widget
    // was removed). This is the fast path: the tree search happens at most
    // once per step entry, never once per action.
    // >0: in addition to the above, force a fresh re-resolve every N
    // actions performed against this region, to notice the object having
    // moved (not destroyed/recreated, just repositioned by a layout change)
    // partway through a long-running step. Trades some speed for staying
    // current -- see SPEC.md's discussion of this tradeoff.
    int reresolveEveryActions = 0;
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

    // SPEC.md 追加実装依頼「名前付きオブジェクト」: when true, this
    // NamedRegion is resolved from `objectTarget` (see its own comment)
    // instead of the `regions`/`excludeRegions`/followsTargetWindow/
    // anchorTopLeft fields below, which are then unused/empty. Mutually
    // exclusive with the rectangle-based fields by construction (the
    // registration UI offers one or the other, never both for the same
    // NamedRegion).
    bool isObjectTarget = false;
    ObjectTarget objectTarget;

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
    // a *freshly default-constructed* NamedRegion -- i.e. StepEditorDialog::
    // onCreateRegion()'s starting point for a brand-new region, which is
    // what NamedRegionEditorDialog's "対象ウィンドウの移動に追従させる"
    // checkbox's initial checked state is seeded from. A region loaded from
    // a saved preset always gets this explicitly from the JSON (defaulting
    // to false there if the key is absent, for old presets predating this
    // field) via TestConfigJson::namedRegionFromJson(), so this default
    // never affects deserialization.
    bool followsTargetWindow = true;
    QPoint anchorTopLeft;

    // SPEC.md 追加実装依頼「操作領域を点列（スイープ）で指定」/「中点対応」:
    // when true, this NamedRegion is resolved as a fixed sequence of evenly
    // spaced points walking through every consecutive pair in
    // `sweepWaypoints` in order (sweepWaypoints[0] -> [1] -> ... ->
    // last, each consecutive pair subdivided sweepIntervalPx apart -- see
    // RandomActionEngine::sweepPointSequence()), visited in order and
    // wrapping back to the first waypoint once exhausted, one point per
    // action against this region -- rather than a rectangle a point is
    // picked randomly within. At least 2 waypoints (a start and an end;
    // any number of midpoints may sit between them) are required for this
    // region to be usable. Mutually exclusive with isObjectTarget and with
    // the regions/excludeRegions/followsTargetWindow/anchorTopLeft fields
    // above by construction (the registration UI offers exactly one of the
    // three), which are then unused/empty. Defining the sweep at the
    // NamedRegion level (the only place it's defined -- a step referencing
    // this region only chooses, via RegionStep::sweepRegionUseRandomPoint,
    // between walking this sequence deterministically or picking a random
    // point along it) lets the same point sequence be reused by name across
    // multiple steps, the same way a drawn rectangle or a named object can
    // be.
    //
    // Each waypoint is stored as a window-relative offset from the target
    // window's top-left corner (same convention as RegionStep's own
    // sweepStart/sweepEnd and SetupAction::point), so -- unlike the
    // rectangle fields above -- they always track the target window moving
    // without needing a separate "follow" opt-in/anchor: RandomActionEngine
    // re-adds the *current* window top-left at pick time instead of
    // translating by how far it's moved since some earlier anchor.
    bool isSweepTarget = false;
    QList<QPoint> sweepWaypoints;
    int sweepIntervalPx = 50;
    int sweepJitterPx = 0;

    bool isEmpty() const
    {
        if (isObjectTarget)
            return objectTarget.name.isEmpty();
        if (isSweepTarget)
            return sweepWaypoints.size() < 2;
        return regions.isEmpty();
    }
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

    // Only meaningful when `regionName` refers to a NamedRegion with
    // isSweepTarget == true (a 点列(スイープ) region -- see NamedRegion's
    // own comment); ignored otherwise, including for an ordinary rectangle
    // region or useWholeWindow, which always pick a uniform-random point
    // within the region/window. When the resolved region IS a sweep
    // target, this chooses between the region's two offered behaviors:
    // false (default) -- deterministic 点列スイープ: visit the region's
    // sweepWaypoints in order, sweepIntervalPx apart, wrapping back to the
    // first once exhausted (RandomActionEngine::pickSweepPoint()). true --
    // ランダム: a uniform-random point along the whole connected path
    // through sweepWaypoints, independently chosen for every action
    // (RandomActionEngine::pickRandomPointOnSweepPath()). Either way the
    // region's own sweepIntervalPx/sweepJitterPx and sweepWaypoints are
    // the sole source of the path; a step no longer carries its own
    // start/end points.
    bool sweepRegionUseRandomPoint = false;

    bool enableClick = true;
    bool enableLeftClick = true;
    bool enableRightClick = false;
    // Double-click is its own action kind (always the left button -- a
    // double-right-click has no consistent OS meaning), separate from a
    // plain Click.
    bool enableDoubleClick = false;
    bool enableDrag = false;
    // SPEC.md 追加実装依頼「ビューワー耐久テスト強化」(ActionKind::
    // HoldDrag): a long-press, continuously-moving camera-manipulation-
    // style gesture -- see ActionParams::holdDragStyles/
    // holdDragDurationMinMs/etc. and HoldDragStyle's own comment. Distinct
    // from enableDrag (an instantaneous flick) so a step can mix both.
    bool enableHoldDrag = false;
    bool enableKey = false;
    // 一定時間キーボード入力 (ActionKind::TypingBurst in RandomActionEngine):
    // like enableKey, but one occurrence keeps sending keystrokes for a
    // randomized duration (ActionParams::typingBurstMinDurationMs/
    // typingBurstMaxDurationMs) instead of exactly one keystroke, counting
    // as this step's one action only once that duration has elapsed --
    // the same "don't advance until the configured time is up" contract
    // isWaitStep's own wait uses, just scoped to a single weighted-pick
    // action kind instead of a whole step.
    bool enableTypingBurst = false;
    // SPEC.md 追加実装依頼「異常入力ファジング」 (ActionKind::FuzzString):
    // picks one entry at random from either the whole app-wide fuzz-string
    // corpus or this step's own fuzzStringSelectedEntries (see
    // ActionParams::fuzzStringUseAllEntries) and sends it to the target --
    // via per-keystroke input, a clipboard paste, or a random mix of both
    // (ActionParams::fuzzStringDeliveryMode) -- to probe how the target
    // handles malformed/adversarial text input (long strings, control
    // characters, SQL/script injection patterns, Unicode edge cases, etc.).
    bool enableFuzzString = false;
    // SPEC.md 追加実装依頼「ファジングテスト: 領域内の全入力項目」
    // (ActionKind::FuzzAllFieldsInRegion): unlike enableFuzzString (which
    // sends one fuzz string to whatever currently has keyboard focus),
    // this searches the target's accessibility tree (AT-SPI on Linux /
    // Accessibility API on macOS -- see PlatformAutomation::
    // findEditableObjectsInRegion()) for every editable widget whose
    // on-screen bounds fall within this step's resolved region, clicks each
    // one to focus it, and sends it its own randomly-picked fuzz string --
    // so a single occurrence can exercise every text field, spin box, and
    // text area in a form at once, not just whichever one a prior click
    // happened to land on. Shares ActionParams::fuzzStringUseAllEntries/
    // fuzzStringSelectedEntries/fuzzStringDeliveryMode with enableFuzzString
    // above -- the corpus and delivery method are the same setting either
    // way, only how many widgets receive it differs. Same accessibility-
    // backend caveats as NamedRegion::isObjectTarget (not exhaustively
    // verified against every toolkit, no-op when accessibility is disabled/
    // unusable in this environment).
    bool enableFuzzAllFields = false;
    bool enableScrollUp = false;
    bool enableScrollDown = false;
    bool enableScrollHorizontal = false;
    bool enableShortcut = false;
    bool enableWindowOp = false;  // move/resize/minimize/maximize the target window itself
    // SPEC.md 6.2追加実装及び修正依頼「タスク内で出てきたダイアログのボタンを
    // 押したり特定の操作をできるようにしてほしい」: only meaningful (ever
    // selected -- see hasAnyActionEnabled()/pickWeightedActionKind()) when
    // targetsPopupDialog is true. Instead of picking a random point
    // somewhere within this step's whole region the way every other action
    // kind does, this looks up a button (or other actionable widget) by
    // accessible name -- one of ActionParams::dialogButtonNames, the exact
    // same "list several candidates, pick whichever are actually present"
    // convention enableContextMenuSelection's ByName mode already uses,
    // since a dialog's exact button set/wording can vary by state (e.g. an
    // "すべて上書き" button only appearing sometimes) -- and clicks it
    // directly via PlatformAutomation::clickButtonByName(), wherever it
    // happens to be positioned in the dialog.
    bool enableDialogButtonPress = false;

    // Relative frequency of each enabled action kind within this step
    // (default 1 each = uniform random, matching the original behavior).
    // Kinds that aren't enabled are simply never picked; their weight is
    // ignored. Weights <= 0 are treated as 1.
    int clickWeight = 1;
    int doubleClickWeight = 1;
    int dragWeight = 1;
    int holdDragWeight = 1;
    int keyWeight = 1;
    int typingBurstWeight = 1;
    int fuzzStringWeight = 1;
    int fuzzAllFieldsWeight = 1;
    int scrollUpWeight = 1;
    int scrollDownWeight = 1;
    int scrollHorizontalWeight = 1;
    int shortcutWeight = 1;
    int dialogButtonPressWeight = 1;
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
        return enableClick || enableDoubleClick || enableDrag || enableHoldDrag || enableKey ||
               enableTypingBurst || enableFuzzString || enableFuzzAllFields || enableScrollUp ||
               enableScrollDown || enableScrollHorizontal || enableShortcut ||
               (enableWindowOp && !targetsPopupDialog) || (enableDialogButtonPress && targetsPopupDialog);
    }
};

// What kind of operation a single SetupAction (below) performs.
enum class SetupActionType {
    Click, DoubleClick, RightClick, Drag, TypeText, KeyPress, Wait,
    // SPEC.md 6.13追加実装及び修正依頼「起動時セットアップの操作にホイール操作や
    // 特定のメニューの選択を操作として追加してください」:
    Scroll,      // one wheel event at a fixed point/direction/amount
    MenuSelect,  // right-click to open a context menu, then pick one exact
                 // item (by name or by position) -- unlike RightClick above,
                 // which only ever dismisses whatever menu it opens (setup
                 // is deterministic, so "pick one specific item, always the
                 // same one" is its own action rather than a mode of
                 // RightClick).
};

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

    // Scroll only: one wheel event at `point` (same window-relative
    // resolution as above). dx/dy follow PlatformAutomation::scroll()'s own
    // convention (also used by RegionStep's ScrollUp/ScrollDown/
    // ScrollHorizontal in RandomActionEngine::runOneAction): positive dy
    // scrolls up, negative scrolls down; positive dx scrolls right,
    // negative scrolls left. Exactly one of dx/dy is normally set by the
    // editor UI (a single wheel gesture is either vertical or horizontal),
    // but both are applied if set -- there's nothing unsafe about a
    // diagonal scroll, the UI just doesn't expose one.
    int scrollDx = 0;
    int scrollDy = 0;

    // MenuSelect only: right-click at `point` to open a context menu, then
    // select the item named `menuItemName` (menuSelectionMode == ByName) or
    // at 1-based position `menuItemIndex` from the top (ByIndex) -- always
    // the same item every run, unlike ActionParams::enableContextMenuSelection's
    // random pick among several candidates for a regular step. If the named/
    // indexed item can't be found once the menu is open, the menu is simply
    // dismissed and the setup action counts as a (logged) no-op rather than
    // failing the whole run -- a target app whose menu contents vary slightly
    // run to run (e.g. a "Paste" entry only enabled when the clipboard has
    // content) shouldn't make setup itself unreliable.
    ContextMenuSelectionMode menuSelectionMode = ContextMenuSelectionMode::ByName;
    QString menuItemName;
    int menuItemIndex = 1;

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

    // SPEC.md 追加実装依頼「異常入力ファジング」: the app-wide fuzz-string
    // corpus (built-in defaults plus whatever the user has added/edited via
    // the "表示" menu's "ファズ文字列リストを編集..." dialog) that
    // ActionKind::FuzzString picks from when a step's ActionParams::
    // fuzzStringUseAllEntries is true. Deliberately NOT part of a saved
    // preset (TestConfigJson never serializes it): MainWindow populates
    // this fresh, straight from its own QSettings-backed master list, every
    // time a run actually starts, rather than loading/saving it like
    // namedRegions/defaultActionParams above. A step's own
    // fuzzStringSelectedEntries (ActionParams) IS part of the preset --
    // it's a self-contained list of literal strings, not a reference into
    // this corpus, so it stays reproducible even on a machine whose corpus
    // has since changed.
    QStringList fuzzStringCorpus;

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

    // If true, RandomActionEngine temporarily multiplies the action
    // interval (see kAutoSlowdownIntervalMultiplier) whenever the target's
    // WM_PING responsiveness check (checkTargetResponsiveness(), the same
    // probe used for hang detection below) misses once, on the theory that
    // a target which is merely slow -- not actually hung -- benefits from
    // fewer/less-frequent actions landing on it while it catches up. This
    // is independent from, and does not change, the existing hang-stop
    // threshold (two consecutive misses still stops the run as an anomaly
    // regardless of this option). Reverts to the normal interval as soon as
    // a ping succeeds again. Off by default: most targets never need it,
    // and it changes run timing, so it's opt-in like the other diagnostics
    // toggles above.
    bool autoSlowdownEnabled = false;

    // SPEC.md 10 「デバッグモード」: for when the target app itself is being
    // run under a debugger (gdb/lldb) during the endurance test -- a
    // breakpoint stop looks identical to a genuine hang from
    // checkTargetResponsiveness()'s point of view (the target simply stops
    // answering WM_PING for a while), so without this, stepping through a
    // breakpoint for more than a few seconds would trip the normal 2-miss
    // hang-stop. When true, RandomActionEngine requires many more
    // consecutive misses (kDebuggerModeHangConfirmThreshold) before
    // confirming a hang, trading prompt hang detection for tolerance of
    // long breakpoint pauses -- a target that is *truly* frozen (not just
    // paused by the debugger) still eventually trips it, just later. Off by
    // default, like the other diagnostics toggles above: most runs aren't
    // being debugged, and the whole point is to only relax hang detection
    // when the user says it's actually needed. RandomActionEngine also
    // auto-detects (PlatformAutomation::isBeingDebugged()) whether a
    // debugger is actually attached regardless of this flag, and warns
    // (before starting, and once during the run) if it finds one attached
    // while this option is left off -- see MainWindow::beginRun() and
    // RandomActionEngine::sampleResourceUsage().
    bool debuggerModeEnabled = false;

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

    // SPEC.md 10 追加提案「対象アプリ自身のログファイルの監視・クラッシュ時
    // 自動添付」: optional path to a log file the target app itself writes.
    // Empty (default) = feature off, same convention as MainWindow's
    // targetLaunchCommand field -- when empty, RandomActionEngine::
    // captureAnomalyArtifacts() skips this step entirely, so existing runs
    // that never set this see zero behavior change. When set, the last
    // portion of this file (bounded, see captureTargetLogTail()) is copied
    // alongside the other anomaly artifacts (screenshot/crash dump/preset
    // snapshot) whenever a run stops abnormally, giving far richer crash
    // context than the screenshot alone for a target app that logs its own
    // diagnostics.
    QString targetLogFilePath;

    // 0 = pick a fresh random seed each run (and log it). Any other value
    // seeds the run's random generator directly, so the exact same
    // sequence of actions can be reproduced later by re-entering the seed
    // that was logged when a run stopped unexpectedly (crash/anomaly).
    quint32 rngSeed = 0;

    // SPEC.md 追加実装依頼「名前付きオブジェクト」: master override, off by
    // default. When true, RandomActionEngine::start()/startSetupOnly() call
    // PlatformAutomation::setAccessibilityFeaturesDisabled(true) before
    // anything else runs, which makes every accessibility-tree-based feature
    // (context-menu item selection, dialog-button-press, and any
    // NamedRegion::isObjectTarget region) behave exactly as if AT-SPI/
    // Accessibility were unusable in this environment -- short-circuiting
    // before any OS accessibility API call is even attempted, not merely
    // discarding a found result. A step referencing an object-target region
    // is then simply skipped (same as a region that no longer exists), so a
    // run configured this way is guaranteed to pay zero AT-SPI/AX overhead,
    // for users who want the fastest possible, purely coordinate-based
    // execution regardless of what regions/steps happen to be configured.
    bool disableAccessibilityFeatures = false;

    // SPEC.md 10 追加提案「メモリリークの自動検知」: off by default (opt-in
    // diagnostic, matching the other toggles above). When true,
    // RandomActionEngine::sampleResourceUsage() (the existing, always-
    // running 5-second resource sampler) additionally feeds each memory
    // sample into a simple linear-regression trend check
    // (checkMemoryLeakTrend()); if the memory-usage slope over the
    // retained sample window sustains at or above
    // memoryLeakThresholdMbPerMinute, a warning is logged once and
    // RunSummary::memoryLeakSuspected is set. This never stops the run by
    // itself (a slow leak by itself isn't a crash) -- it's purely
    // detection/reporting -- so leaving this off (the default) costs
    // nothing beyond the sampler's own pre-existing 5-second tick, and
    // other features are completely unaffected either way.
    bool memoryLeakDetectionEnabled = false;
    double memoryLeakThresholdMbPerMinute = 5.0;
};
