#include "PresetManager.hpp"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <nlohmann/json.hpp>
#include <fstream>

namespace EdgeDrop {

QVariantMap Preset::toVariantMap() const {
    QVariantMap map;
    map["id"] = id;
    map["name"] = name;
    map["category"] = QString::fromStdString(categoryToString(category));
    map["format"] = QString::fromStdString(formatToString(format));
    map["width"] = width;
    map["height"] = height;
    map["minWidth"] = minWidth;
    map["minHeight"] = minHeight;
    map["maxWidth"] = maxWidth;
    map["maxHeight"] = maxHeight;
    map["maxLongEdge"] = maxLongEdge;
    map["dpi"] = dpi;
    map["minKb"] = minKb;
    map["maxKb"] = maxKb;
    map["targetKb"] = targetKb;
    map["autoCropFace"] = autoCropFace;
    map["faceRatio"] = faceRatio;
    map["autoCropInk"] = autoCropInk;
    map["whitenBg"] = whitenBg;
    map["greyscale"] = greyscale;
    map["specSummary"] = specSummary;
    return map;
}

std::optional<Preset> Preset::fromVariantMap(const QVariantMap& map) {
    if (!map.contains("id") || !map.contains("name")) return std::nullopt;

    Preset p;
    p.id = map["id"].toString();
    p.name = map["name"].toString();
    p.category = stringToCategory(map.value("category", "Photo").toString());
    p.format = stringToFormat(map.value("format", "JPG").toString());
    p.width = map.value("width", 0).toInt();
    p.height = map.value("height", 0).toInt();
    p.minWidth = map.value("minWidth", 0).toInt();
    p.minHeight = map.value("minHeight", 0).toInt();
    p.maxWidth = map.value("maxWidth", 0).toInt();
    p.maxHeight = map.value("maxHeight", 0).toInt();
    p.maxLongEdge = map.value("maxLongEdge", 0).toInt();
    p.dpi = map.value("dpi", 300).toInt();
    p.minKb = map.value("minKb", 0).toInt();
    p.maxKb = map.value("maxKb", 0).toInt();
    p.targetKb = map.value("targetKb", 0).toInt();
    p.autoCropFace = map.value("autoCropFace", false).toBool();
    p.faceRatio = map.value("faceRatio", 0.70).toDouble();
    p.autoCropInk = map.value("autoCropInk", false).toBool();
    p.whitenBg = map.value("whitenBg", false).toBool();
    p.greyscale = map.value("greyscale", false).toBool();
    p.specSummary = map.value("specSummary", "").toString();

    return p;
}

PresetManager::PresetManager(QObject* parent)
    : QObject(parent) {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    m_presetsFilePath = configDir + "/presets.json";

    loadPresets();
}

bool PresetManager::loadPresets(const QString& customPath) {
    QString path = customPath;
    if (path.isEmpty()) {
        if (QFile::exists(m_presetsFilePath)) {
            path = m_presetsFilePath;
        } else if (QFile::exists("resources/presets.json")) {
            path = "resources/presets.json";
        } else if (QFile::exists(":/presets.json")) {
            path = ":/presets.json";
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        return false;
    }

    QList<Preset> loadedPresets;
    QJsonArray array = doc.array();
    for (const auto& item : array) {
        if (!item.isObject()) continue;
        QJsonObject obj = item.toObject();

        Preset p;
        p.id = obj["id"].toString();
        p.name = obj["name"].toString();
        p.category = stringToCategory(obj["category"].toString());
        p.format = stringToFormat(obj["format"].toString());
        p.width = obj["width"].toInt();
        p.height = obj["height"].toInt();
        p.minWidth = obj["min_width"].toInt();
        p.minHeight = obj["min_height"].toInt();
        p.maxWidth = obj["max_width"].toInt();
        p.maxHeight = obj["max_height"].toInt();
        p.maxLongEdge = obj["max_long_edge"].toInt();
        p.dpi = obj["dpi"].toInt(300);
        p.minKb = obj["min_kb"].toInt();
        p.maxKb = obj["max_kb"].toInt();
        p.targetKb = obj["target_kb"].toInt();
        p.autoCropFace = obj["auto_crop_face"].toBool();
        p.faceRatio = obj["face_ratio"].toDouble(0.70);
        p.autoCropInk = obj["auto_crop_ink"].toBool();
        p.whitenBg = obj["whiten_bg"].toBool();
        p.greyscale = obj["greyscale"].toBool();
        p.specSummary = obj["spec_summary"].toString();

        if (!p.id.isEmpty() && !p.name.isEmpty()) {
            loadedPresets.append(p);
        }
    }

    if (!loadedPresets.isEmpty()) {
        m_presets = std::move(loadedPresets);
        emit presetsChanged();
        return true;
    }

    return false;
}

bool PresetManager::saveUserPresets() {
    QJsonArray array;
    for (const auto& p : m_presets) {
        QJsonObject obj;
        obj["id"] = p.id;
        obj["name"] = p.name;
        obj["category"] = QString::fromStdString(categoryToString(p.category));
        obj["format"] = QString::fromStdString(formatToString(p.format));
        obj["width"] = p.width;
        obj["height"] = p.height;
        obj["min_width"] = p.minWidth;
        obj["min_height"] = p.minHeight;
        obj["max_width"] = p.maxWidth;
        obj["max_height"] = p.maxHeight;
        obj["max_long_edge"] = p.maxLongEdge;
        obj["dpi"] = p.dpi;
        obj["min_kb"] = p.minKb;
        obj["max_kb"] = p.maxKb;
        obj["target_kb"] = p.targetKb;
        obj["auto_crop_face"] = p.autoCropFace;
        obj["face_ratio"] = p.faceRatio;
        obj["auto_crop_ink"] = p.autoCropInk;
        obj["whiten_bg"] = p.whitenBg;
        obj["greyscale"] = p.greyscale;
        obj["spec_summary"] = p.specSummary;
        array.append(obj);
    }

    QJsonDocument doc(array);
    QFile file(m_presetsFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

void PresetManager::resetToDefaults() {
    if (QFile::exists(m_presetsFilePath)) {
        QFile::remove(m_presetsFilePath);
    }
    loadPresets("resources/presets.json");
}

QList<Preset> PresetManager::presetsForCategory(FileCategory cat) const {
    QList<Preset> filtered;
    for (const auto& p : m_presets) {
        if (p.category == cat) {
            filtered.append(p);
        }
    }
    return filtered;
}

std::optional<Preset> PresetManager::presetById(const QString& id) const {
    for (const auto& p : m_presets) {
        if (p.id == id) return p;
    }
    return std::nullopt;
}

QVariantList PresetManager::presetsAsVariantList() const {
    QVariantList list;
    for (const auto& p : m_presets) {
        list.append(p.toVariantMap());
    }
    return list;
}

QVariantList PresetManager::getPresetsForCategory(const QString& categoryStr) const {
    FileCategory cat = stringToCategory(categoryStr);
    QVariantList list;
    for (const auto& p : presetsForCategory(cat)) {
        list.append(p.toVariantMap());
    }
    return list;
}

QVariantMap PresetManager::getPreset(const QString& id) const {
    auto p = presetById(id);
    if (p) return p->toVariantMap();
    return {};
}

} // namespace EdgeDrop
