#include "TestConfigJson.h"

#include <QJsonArray>

namespace
{

QString dragDirectionToString(DragDirectionMode mode)
{
    switch (mode) {
    case DragDirectionMode::Up: return QStringLiteral("up");
    case DragDirectionMode::Down: return QStringLiteral("down");
    case DragDirectionMode::Left: return QStringLiteral("left");
    case DragDirectionMode::Right: return QStringLiteral("right");
    case DragDirectionMode::Random:
    default: return QStringLiteral("random");
    }
}

DragDirectionMode dragDirectionFromString(const QString &s)
{
    if (s == QStringLiteral("up")) return DragDirectionMode::Up;
    if (s == QStringLiteral("down")) return DragDirectionMode::Down;
    if (s == QStringLiteral("left")) return DragDirectionMode::Left;
    if (s == QStringLiteral("right")) return DragDirectionMode::Right;
    return DragDirectionMode::Random;
}

QString setupActionTypeToString(SetupActionType type)
{
    switch (type) {
    case SetupActionType::Click: return QStringLiteral("click");
    case SetupActionType::DoubleClick: return QStringLiteral("doubleClick");
    case SetupActionType::RightClick: return QStringLiteral("rightClick");
    case SetupActionType::Drag: return QStringLiteral("drag");
    case SetupActionType::TypeText: return QStringLiteral("typeText");
    case SetupActionType::KeyPress: return QStringLiteral("keyPress");
    case SetupActionType::Wait: return QStringLiteral("wait");
    }
    return QStringLiteral("click");
}

SetupActionType setupActionTypeFromString(const QString &s)
{
    if (s == QStringLiteral("doubleClick")) return SetupActionType::DoubleClick;
    if (s == QStringLiteral("rightClick")) return SetupActionType::RightClick;
    if (s == QStringLiteral("drag")) return SetupActionType::Drag;
    if (s == QStringLiteral("typeText")) return SetupActionType::TypeText;
    if (s == QStringLiteral("keyPress")) return SetupActionType::KeyPress;
    if (s == QStringLiteral("wait")) return SetupActionType::Wait;
    return SetupActionType::Click;
}

QString contextMenuModeToString(ContextMenuSelectionMode mode)
{
    return mode == ContextMenuSelectionMode::ByIndex ? QStringLiteral("byIndex") : QStringLiteral("byName");
}

ContextMenuSelectionMode contextMenuModeFromString(const QString &s)
{
    return s == QStringLiteral("byIndex") ? ContextMenuSelectionMode::ByIndex
                                           : ContextMenuSelectionMode::ByName;
}

QJsonArray rectListToJson(const QList<QRect> &rects)
{
    QJsonArray arr;
    for (const QRect &r : rects) {
        QJsonObject o;
        o["x"] = r.x();
        o["y"] = r.y();
        o["w"] = r.width();
        o["h"] = r.height();
        arr.append(o);
    }
    return arr;
}

QList<QRect> rectListFromJson(const QJsonArray &arr)
{
    QList<QRect> rects;
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        rects.append(QRect(o["x"].toInt(), o["y"].toInt(), o["w"].toInt(), o["h"].toInt()));
    }
    return rects;
}

QJsonArray stringListToJson(const QStringList &list)
{
    QJsonArray arr;
    for (const QString &s : list)
        arr.append(s);
    return arr;
}

QStringList stringListFromJson(const QJsonArray &arr)
{
    QStringList list;
    for (const QJsonValue &v : arr)
        list.append(v.toString());
    return list;
}

QJsonArray intListToJson(const QList<int> &list)
{
    QJsonArray arr;
    for (int v : list)
        arr.append(v);
    return arr;
}

QList<int> intListFromJson(const QJsonArray &arr)
{
    QList<int> list;
    for (const QJsonValue &v : arr)
        list.append(v.toInt());
    return list;
}

}  // namespace

QJsonObject actionParamsToJson(const ActionParams &p)
{
    QJsonObject o;
    o["dragMinDistance"] = p.dragMinDistance;
    o["dragMaxDistance"] = p.dragMaxDistance;
    o["dragDirection"] = dragDirectionToString(p.dragDirection);
    o["allowedKeyChars"] = p.allowedKeyChars;
    o["keyIncludeTab"] = p.keyIncludeTab;
    o["keyIncludeReturn"] = p.keyIncludeReturn;
    o["keyIncludeEscape"] = p.keyIncludeEscape;
    o["keyIncludeBackspace"] = p.keyIncludeBackspace;
    o["keyIncludeDelete"] = p.keyIncludeDelete;
    o["keyIncludeArrowKeys"] = p.keyIncludeArrowKeys;
    o["shortcutSequences"] = stringListToJson(p.shortcutSequences);
    o["scrollUpMinAmount"] = p.scrollUpMinAmount;
    o["scrollUpMaxAmount"] = p.scrollUpMaxAmount;
    o["scrollDownMinAmount"] = p.scrollDownMinAmount;
    o["scrollDownMaxAmount"] = p.scrollDownMaxAmount;
    o["scrollHorizontalMinAmount"] = p.scrollHorizontalMinAmount;
    o["scrollHorizontalMaxAmount"] = p.scrollHorizontalMaxAmount;
    o["windowOpMove"] = p.windowOpMove;
    o["windowOpResize"] = p.windowOpResize;
    o["windowOpMinimize"] = p.windowOpMinimize;
    o["windowOpMaximize"] = p.windowOpMaximize;
    o["enableContextMenuSelection"] = p.enableContextMenuSelection;
    o["contextMenuSelectionMode"] = contextMenuModeToString(p.contextMenuSelectionMode);
    o["contextMenuItemNames"] = stringListToJson(p.contextMenuItemNames);
    o["contextMenuIndices"] = intListToJson(p.contextMenuIndices);
    return o;
}

ActionParams actionParamsFromJson(const QJsonObject &o)
{
    ActionParams p;
    p.dragMinDistance = o["dragMinDistance"].toInt(p.dragMinDistance);
    p.dragMaxDistance = o["dragMaxDistance"].toInt(p.dragMaxDistance);
    p.dragDirection = dragDirectionFromString(o["dragDirection"].toString());
    p.allowedKeyChars = o["allowedKeyChars"].toString(p.allowedKeyChars);
    p.keyIncludeTab = o["keyIncludeTab"].toBool(p.keyIncludeTab);
    p.keyIncludeReturn = o["keyIncludeReturn"].toBool(p.keyIncludeReturn);
    p.keyIncludeEscape = o["keyIncludeEscape"].toBool(p.keyIncludeEscape);
    p.keyIncludeBackspace = o["keyIncludeBackspace"].toBool(p.keyIncludeBackspace);
    p.keyIncludeDelete = o["keyIncludeDelete"].toBool(p.keyIncludeDelete);
    p.keyIncludeArrowKeys = o["keyIncludeArrowKeys"].toBool(p.keyIncludeArrowKeys);
    if (o.contains("shortcutSequences"))
        p.shortcutSequences = stringListFromJson(o["shortcutSequences"].toArray());
    p.scrollUpMinAmount = o["scrollUpMinAmount"].toInt(p.scrollUpMinAmount);
    p.scrollUpMaxAmount = o["scrollUpMaxAmount"].toInt(p.scrollUpMaxAmount);
    p.scrollDownMinAmount = o["scrollDownMinAmount"].toInt(p.scrollDownMinAmount);
    p.scrollDownMaxAmount = o["scrollDownMaxAmount"].toInt(p.scrollDownMaxAmount);
    p.scrollHorizontalMinAmount = o["scrollHorizontalMinAmount"].toInt(p.scrollHorizontalMinAmount);
    p.scrollHorizontalMaxAmount = o["scrollHorizontalMaxAmount"].toInt(p.scrollHorizontalMaxAmount);
    p.windowOpMove = o["windowOpMove"].toBool(p.windowOpMove);
    p.windowOpResize = o["windowOpResize"].toBool(p.windowOpResize);
    p.windowOpMinimize = o["windowOpMinimize"].toBool(p.windowOpMinimize);
    p.windowOpMaximize = o["windowOpMaximize"].toBool(p.windowOpMaximize);
    p.enableContextMenuSelection = o["enableContextMenuSelection"].toBool(p.enableContextMenuSelection);
    p.contextMenuSelectionMode = contextMenuModeFromString(o["contextMenuSelectionMode"].toString());
    if (o.contains("contextMenuItemNames"))
        p.contextMenuItemNames = stringListFromJson(o["contextMenuItemNames"].toArray());
    if (o.contains("contextMenuIndices"))
        p.contextMenuIndices = intListFromJson(o["contextMenuIndices"].toArray());
    return p;
}

QJsonObject namedRegionToJson(const NamedRegion &r)
{
    QJsonObject o;
    o["name"] = r.name;
    o["regions"] = rectListToJson(r.regions);
    o["excludeRegions"] = rectListToJson(r.excludeRegions);
    o["followsTargetWindow"] = r.followsTargetWindow;
    o["anchorTopLeftX"] = r.anchorTopLeft.x();
    o["anchorTopLeftY"] = r.anchorTopLeft.y();
    return o;
}

NamedRegion namedRegionFromJson(const QJsonObject &o)
{
    NamedRegion r;
    r.name = o["name"].toString();
    r.regions = rectListFromJson(o["regions"].toArray());
    r.excludeRegions = rectListFromJson(o["excludeRegions"].toArray());
    r.followsTargetWindow = o["followsTargetWindow"].toBool(false);
    r.anchorTopLeft = QPoint(o["anchorTopLeftX"].toInt(), o["anchorTopLeftY"].toInt());
    return r;
}

QJsonObject regionStepToJson(const RegionStep &s)
{
    QJsonObject o;
    o["isWaitStep"] = s.isWaitStep;
    o["waitDurationMs"] = s.waitDurationMs;
    o["useWholeWindow"] = s.useWholeWindow;
    o["regionName"] = s.regionName;
    o["enableClick"] = s.enableClick;
    o["enableLeftClick"] = s.enableLeftClick;
    o["enableRightClick"] = s.enableRightClick;
    o["enableDoubleClick"] = s.enableDoubleClick;
    o["enableDrag"] = s.enableDrag;
    o["enableKey"] = s.enableKey;
    o["enableScrollUp"] = s.enableScrollUp;
    o["enableScrollDown"] = s.enableScrollDown;
    o["enableScrollHorizontal"] = s.enableScrollHorizontal;
    o["enableShortcut"] = s.enableShortcut;
    o["enableWindowOp"] = s.enableWindowOp;
    o["clickWeight"] = s.clickWeight;
    o["doubleClickWeight"] = s.doubleClickWeight;
    o["dragWeight"] = s.dragWeight;
    o["keyWeight"] = s.keyWeight;
    o["scrollUpWeight"] = s.scrollUpWeight;
    o["scrollDownWeight"] = s.scrollDownWeight;
    o["scrollHorizontalWeight"] = s.scrollHorizontalWeight;
    o["shortcutWeight"] = s.shortcutWeight;
    o["windowOpWeight"] = s.windowOpWeight;
    o["actionCount"] = double(s.actionCount);
    o["useDefaultActionParams"] = s.useDefaultActionParams;
    o["customActionParams"] = actionParamsToJson(s.customActionParams);
    o["isGroup"] = s.isGroup;
    o["groupTotalCallCount"] = double(s.groupTotalCallCount);
    o["groupWeight"] = s.groupWeight;
    QJsonArray membersArr;
    for (const RegionStep &member : s.groupMembers)
        membersArr.append(regionStepToJson(member));
    o["groupMembers"] = membersArr;
    o["isTask"] = s.isTask;
    QJsonArray taskMembersArr;
    for (const RegionStep &member : s.taskMembers)
        taskMembersArr.append(regionStepToJson(member));
    o["taskMembers"] = taskMembersArr;
    o["targetsPopupDialog"] = s.targetsPopupDialog;
    return o;
}

RegionStep regionStepFromJson(const QJsonObject &o)
{
    RegionStep s;
    s.isWaitStep = o["isWaitStep"].toBool(s.isWaitStep);
    s.waitDurationMs = o["waitDurationMs"].toInt(s.waitDurationMs);
    s.useWholeWindow = o["useWholeWindow"].toBool(s.useWholeWindow);
    s.regionName = o["regionName"].toString();
    s.enableClick = o["enableClick"].toBool(s.enableClick);
    s.enableLeftClick = o["enableLeftClick"].toBool(s.enableLeftClick);
    s.enableRightClick = o["enableRightClick"].toBool(s.enableRightClick);
    s.enableDoubleClick = o["enableDoubleClick"].toBool(s.enableDoubleClick);
    s.enableDrag = o["enableDrag"].toBool(s.enableDrag);
    s.enableKey = o["enableKey"].toBool(s.enableKey);
    s.enableScrollUp = o["enableScrollUp"].toBool(s.enableScrollUp);
    s.enableScrollDown = o["enableScrollDown"].toBool(s.enableScrollDown);
    s.enableScrollHorizontal = o["enableScrollHorizontal"].toBool(s.enableScrollHorizontal);
    s.enableShortcut = o["enableShortcut"].toBool(s.enableShortcut);
    s.enableWindowOp = o["enableWindowOp"].toBool(s.enableWindowOp);
    s.clickWeight = o["clickWeight"].toInt(s.clickWeight);
    s.doubleClickWeight = o["doubleClickWeight"].toInt(s.doubleClickWeight);
    s.dragWeight = o["dragWeight"].toInt(s.dragWeight);
    s.keyWeight = o["keyWeight"].toInt(s.keyWeight);
    s.scrollUpWeight = o["scrollUpWeight"].toInt(s.scrollUpWeight);
    s.scrollDownWeight = o["scrollDownWeight"].toInt(s.scrollDownWeight);
    s.scrollHorizontalWeight = o["scrollHorizontalWeight"].toInt(s.scrollHorizontalWeight);
    s.shortcutWeight = o["shortcutWeight"].toInt(s.shortcutWeight);
    s.windowOpWeight = o["windowOpWeight"].toInt(s.windowOpWeight);
    s.actionCount = qint64(o["actionCount"].toDouble(double(s.actionCount)));
    s.useDefaultActionParams = o["useDefaultActionParams"].toBool(s.useDefaultActionParams);
    if (o.contains("customActionParams"))
        s.customActionParams = actionParamsFromJson(o["customActionParams"].toObject());
    s.isGroup = o["isGroup"].toBool(s.isGroup);
    s.groupTotalCallCount = qint64(o["groupTotalCallCount"].toDouble(double(s.groupTotalCallCount)));
    s.groupWeight = o["groupWeight"].toInt(s.groupWeight);
    if (o.contains("groupMembers")) {
        for (const QJsonValue &v : o["groupMembers"].toArray())
            s.groupMembers.append(regionStepFromJson(v.toObject()));
    }
    s.isTask = o["isTask"].toBool(s.isTask);
    if (o.contains("taskMembers")) {
        for (const QJsonValue &v : o["taskMembers"].toArray())
            s.taskMembers.append(regionStepFromJson(v.toObject()));
    }
    s.targetsPopupDialog = o["targetsPopupDialog"].toBool(s.targetsPopupDialog);
    return s;
}

QJsonObject setupActionToJson(const SetupAction &a)
{
    QJsonObject o;
    o["type"] = setupActionTypeToString(a.type);
    o["pointX"] = a.point.x();
    o["pointY"] = a.point.y();
    o["dragToPointX"] = a.dragToPoint.x();
    o["dragToPointY"] = a.dragToPoint.y();
    o["text"] = a.text;
    o["keySequence"] = a.keySequence;
    o["waitMs"] = a.waitMs;
    o["label"] = a.label;
    return o;
}

SetupAction setupActionFromJson(const QJsonObject &o)
{
    SetupAction a;
    a.type = setupActionTypeFromString(o["type"].toString());
    a.point = QPoint(o["pointX"].toInt(), o["pointY"].toInt());
    a.dragToPoint = QPoint(o["dragToPointX"].toInt(), o["dragToPointY"].toInt());
    a.text = o["text"].toString();
    a.keySequence = o["keySequence"].toString();
    a.waitMs = o["waitMs"].toInt(a.waitMs);
    a.label = o["label"].toString();
    return a;
}
