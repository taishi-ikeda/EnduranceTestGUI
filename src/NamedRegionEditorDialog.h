#pragma once

#include <QDialog>

#include "TestConfig.h"

class QLineEdit;
class QListWidget;
class QPushButton;
class QLabel;
class QCheckBox;
class RegionHighlightOverlay;

// Modal dialog for creating/editing one NamedRegion: a name, one or more
// rectangles drawn via RegionSelectorOverlay, and optional mask/exclude
// sub-rectangles within them. Used from the "①対象選択" column's operation-
// region list (add/edit) -- see SPEC.md 6.3.
//
// While this dialog is open, the region being built/edited is shown on
// screen the whole time via RegionHighlightOverlay (updated after every
// add/remove of a rectangle), rather than MainWindow showing a highlight
// just from a list selection -- selecting an item in the list no longer
// shows anything on screen by itself.
class NamedRegionEditorDialog : public QDialog
{
    Q_OBJECT

public:
    // `targetTopLeft`/`hasTarget`: the currently-selected target window's
    // top-left corner at the moment this dialog was opened, as recorded by
    // MainWindow (which re-reads it live from the target combo box) --
    // used as the reference point if the user enables "対象ウィンドウの
    // 移動に追従させる" (SPEC.md 6.3/10). hasTarget is false when no
    // target window is currently selectable, in which case that option is
    // disabled.
    explicit NamedRegionEditorDialog(const NamedRegion &initial, const QPoint &targetTopLeft,
                                      bool hasTarget, QWidget *parent = nullptr);

    NamedRegion result() const;

private slots:
    void onDrawRegions();
    void onRemoveSelectedRegion();
    void onDrawExcludeRegions();
    void onRemoveSelectedExcludeRegion();
    void onAccept();

private:
    void refreshRegionList();
    void refreshExcludeList();
    void updateHighlight();

    QLineEdit *m_nameEdit = nullptr;
    QListWidget *m_regionListWidget = nullptr;
    QPushButton *m_drawButton = nullptr;
    QPushButton *m_removeRegionButton = nullptr;
    QListWidget *m_excludeListWidget = nullptr;
    QPushButton *m_drawExcludeButton = nullptr;
    QPushButton *m_removeExcludeButton = nullptr;
    QCheckBox *m_followTargetCheck = nullptr;

    QList<QRect> m_regions;
    QList<QRect> m_excludeRegions;
    QPoint m_targetTopLeft;
    bool m_hasTarget = false;
    // If editing a region that already had followsTargetWindow set, its
    // original anchor is preserved unless the user's current target
    // selection replaces it (see onAccept()).
    QPoint m_existingAnchorTopLeft;
    bool m_hadExistingAnchor = false;
    RegionHighlightOverlay *m_highlightOverlay = nullptr;
};
