#pragma once

#include <QWidget>

// Standard base for every 10BIT dock that is registered through
// obs_frontend_add_dock_by_id().
//
// OBS wraps the widget in its native OBSDock and its theme styles the direct
// child using the rule: OBSDock > QWidget.  Custom QWidget subclasses need
// WA_StyledBackground enabled so that OBS can actually paint the native body
// background, 1px border and bottom corner radius.
//
// Rule for future 10BIT docks:
//   1. Derive the direct dock content widget from TenBitObsDockContent.
//   2. Never set a stylesheet on this direct widget.
//   3. Put 10BIT branding/styles on a nested child panel only.
// This preserves the current OBS theme, including future theme changes.
class TenBitObsDockContent : public QWidget {
public:
    explicit TenBitObsDockContent(QWidget *parent = nullptr);
    ~TenBitObsDockContent() override = default;
};
