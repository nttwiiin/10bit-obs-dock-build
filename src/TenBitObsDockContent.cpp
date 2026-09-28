#include "TenBitObsDockContent.hpp"

TenBitObsDockContent::TenBitObsDockContent(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);
    setContentsMargins(0, 0, 0, 0);
}
