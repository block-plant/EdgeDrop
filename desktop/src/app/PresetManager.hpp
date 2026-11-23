#pragma once

#include "../engine/ProcessingTypes.hpp"
#include <QObject>
#include <QString>
#include <QList>
#include <QVariantMap>
#include <optional>

namespace EdgeDrop {

struct Preset {
    QString id;
    QString name;
    FileCategory category = FileCategory::Photo;
    TargetFormat format = TargetFormat::JPG;
    int width = 0;
    int height = 0;
    int minWidth = 0;
    int minHeight = 0;
    int maxWidth = 0;
    int maxHeight = 0;
    int maxLongEdge = 0;
    int dpi = 300;
    int minKb = 0;
    int maxKb = 0;
    int targetKb = 0;
    bool autoCropFace = false;
    double faceRatio = 0.70;
    bool autoCropInk = false;
    bool whitenBg = false;
    bool greyscale = false;
    QString specSummary;

    [[nodiscard]] QVariantMap toVariantMap() const;
    static std::optional<Preset> fromVariantMap(const QVariantMap& map);
};

class PresetManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList presets READ presetsAsVariantList NOTIFY presetsChanged)

public:
    explicit PresetManager(QObject* parent = nullptr);

    bool loadPresets(const QString& customPath = "");
    bool saveUserPresets();
    void resetToDefaults();

    [[nodiscard]] const QList<Preset>& presets() const { return m_presets; }
    [[nodiscard]] QList<Preset> presetsForCategory(FileCategory cat) const;
    [[nodiscard]] std::optional<Preset> presetById(const QString& id) const;

    [[nodiscard]] QVariantList presetsAsVariantList() const;
    Q_INVOKABLE QVariantList getPresetsForCategory(const QString& categoryStr) const;
    Q_INVOKABLE QVariantMap getPreset(const QString& id) const;

signals:
    void presetsChanged();

private:
    QList<Preset> m_presets;
    QString m_presetsFilePath;
};

} // namespace EdgeDrop
