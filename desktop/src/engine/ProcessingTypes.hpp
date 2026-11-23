#pragma once

#include <edgedrop/types.hpp>
#include <QString>
#include <QFileInfo>
#include <QMetaType>

namespace EdgeDrop {

inline QString categoryToQString(FileCategory cat) {
    return QString::fromStdString(categoryToString(cat));
}

inline FileCategory stringToCategory(const QString& str) {
    return EdgeDrop::stringToCategory(str.toStdString());
}

inline QString formatToQString(TargetFormat fmt) {
    return QString::fromStdString(formatToString(fmt));
}

inline TargetFormat stringToFormat(const QString& str) {
    return EdgeDrop::stringToFormat(str.toStdString());
}

} // namespace EdgeDrop

Q_DECLARE_METATYPE(EdgeDrop::JobRequest)
Q_DECLARE_METATYPE(EdgeDrop::JobResult)
Q_DECLARE_METATYPE(EdgeDrop::DetectedFormat)
