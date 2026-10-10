#pragma once

#include "TestConfig.h"

#include <QPoint>
#include <QStringList>

#include <functional>

// Bundles everything StepEditorDialog (and, by extension,
// StepGroupEditorDialog/TaskEditorDialog, which forward this same bundle to
// their own nested StepEditorDialog instances) needs to let the user author
// a NamedRegion -- create/edit/delete -- directly from within a step's own
// edit dialog, now that MainWindow no longer has a standalone "①対象選択"
// region-management panel (SPEC.md 追加実装依頼「操作領域の指定をステップ
// 単位のダイアログへ統合」). Every step-editing dialog shares the SAME
// underlying QList<NamedRegion> (MainWindow::m_namedRegions) by pointer, so
// a region created while editing one step is immediately selectable from
// any other step's dialog, including ones opened later in the same run.
struct RegionEditContext
{
    // The shared pool itself. Mutated directly (append/in-place edit/
    // removeAt) by whichever dialog is currently open -- there is no
    // separate "commit" step; a region created or edited here persists
    // even if the step dialog it was opened from is later cancelled,
    // exactly like the old ①対象選択 panel's add/edit/delete buttons did.
    QList<NamedRegion> *namedRegions = nullptr;

    // The target window's top-left/pid at the moment the outermost step
    // dialog was opened -- passed straight through to NamedRegionEditorDialog
    // for its own "対象ウィンドウの移動に追従させる"/"画面上の部品を指定"
    // features. Captured once, not live-refreshed while a dialog stays open
    // (same as the old MainWindow::onAddNamedRegion()/onEditSelectedNamedRegion()).
    QPoint targetTopLeft;
    bool hasTarget = false;
    qint64 targetPid = -1;

    // Returns the human-facing list of steps that reference regionName
    // (MainWindow::stepsReferencing(), recursing into group/task members);
    // empty means safe to delete. StepGroupEditorDialog/TaskEditorDialog
    // wrap this (see their constructors) to also report their own
    // in-progress, not-yet-committed members, so deleting a region doesn't
    // silently orphan a sibling member being edited in the same still-open
    // dialog.
    std::function<QStringList(const QString &regionName)> stepsReferencing;

    // Propagates a rename (oldName -> newName) into every *top-level*
    // step's regionName (MainWindow::renameRegionReferences(), recursing
    // into group/task members already committed to MainWindow::m_steps).
    // Known limitation: a sibling member inside a StepGroupEditorDialog/
    // TaskEditorDialog that is itself still open and uncommitted is not
    // reached by this -- only the one member actually being edited picks
    // up the new name (via its own dialog.regionName() return value).
    std::function<void(const QString &oldName, const QString &newName)> renameReferences;
};
