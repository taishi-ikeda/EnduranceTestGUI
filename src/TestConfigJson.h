#pragma once

// JSON (de)serialization for the structures in TestConfig.h, used to save/
// load a full test setup (named regions, steps, default action params,
// timing & limits) as a reusable preset file -- see SPEC.md 6.2/6.3/10 and
// MainWindow::onSavePreset()/onLoadPreset(). Kept separate from
// TestConfig.h itself so that header can stay free of JSON includes for
// the (more numerous) call sites that only need the plain data structures.

#include <QJsonObject>

#include "TestConfig.h"

QJsonObject actionParamsToJson(const ActionParams &params);
ActionParams actionParamsFromJson(const QJsonObject &obj);

QJsonObject namedRegionToJson(const NamedRegion &region);
NamedRegion namedRegionFromJson(const QJsonObject &obj);

// Also used to save/load RegionStep::customActionParams's owner step as a
// whole, and separately to save/load the "デフォルト" preset's kind/weight/
// count template (MainWindow::m_defaultActionKinds), which is just a
// RegionStep whose region/customActionParams fields are unused.
QJsonObject regionStepToJson(const RegionStep &step);
RegionStep regionStepFromJson(const QJsonObject &obj);

// TestConfig::setupActions entries ("起動時セットアップ", SPEC.md 6.x).
QJsonObject setupActionToJson(const SetupAction &action);
SetupAction setupActionFromJson(const QJsonObject &obj);
