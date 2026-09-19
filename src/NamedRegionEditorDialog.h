#pragma once

#include <QDialog>

#include "TestConfig.h"

class QLineEdit;
class QListWidget;
class QPushButton;
class QLabel;

// Modal dialog for creating/editing one NamedRegion: a name, one or more
// rectangles drawn via RegionSelectorOverlay, and optional mask/exclude
// sub-rectangles within them. Used from the "①対象選択" column's operation-
// region list (add/edit) -- see SPEC.md 6.3.
class NamedRegionEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NamedRegionEditorDialog(const NamedRegion &initial, QWidget *parent = nullptr);

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

    QLineEdit *m_nameEdit = nullptr;
    QListWidget *m_regionListWidget = nullptr;
    QPushButton *m_drawButton = nullptr;
    QPushButton *m_removeRegionButton = nullptr;
    QListWidget *m_excludeListWidget = nullptr;
    QPushButton *m_drawExcludeButton = nullptr;
    QPushButton *m_removeExcludeButton = nullptr;

    QList<QRect> m_regions;
    QList<QRect> m_excludeRegions;
};
