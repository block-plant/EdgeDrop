#include "app/PresetManager.hpp"
#include "app/SettingsManager.hpp"
#include <QCoreApplication>
#include <cassert>
#include <iostream>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "[RUN] Testing Preset Validation..." << std::endl;

    EdgeDrop::PresetManager manager;
    bool loaded = manager.loadPresets("resources/presets.json");
    if (!loaded) {
        loaded = manager.loadPresets("../resources/presets.json");
    }
    assert(loaded);

    const auto& all = manager.presets();
    std::cout << "Loaded " << all.size() << " verified default presets." << std::endl;
    assert(all.size() >= 8);

    // Verify key portals exist
    auto gatePhoto = manager.presetById("gate_photo");
    assert(gatePhoto.has_value());
    assert(gatePhoto->category == EdgeDrop::FileCategory::Photo);
    assert(gatePhoto->format == EdgeDrop::TargetFormat::JPG);
    assert(gatePhoto->width == 480 && gatePhoto->height == 640);
    assert(gatePhoto->dpi == 300);
    assert(gatePhoto->maxKb == 600);

    auto gateSig = manager.presetById("gate_sig");
    assert(gateSig.has_value());
    assert(gateSig->category == EdgeDrop::FileCategory::Signature);
    assert(gateSig->width == 480 && gateSig->height == 150);

    auto panPhoto = manager.presetById("pan_photo");
    assert(panPhoto.has_value());
    assert(panPhoto->width == 213 && panPhoto->height == 213);
    assert(panPhoto->maxKb == 50);

    auto passportPhoto = manager.presetById("passport_photo");
    assert(passportPhoto.has_value());
    assert(passportPhoto->width == 630 && passportPhoto->height == 810);

    auto resumePdf = manager.presetById("resume_pdf");
    assert(resumePdf.has_value());
    assert(resumePdf->category == EdgeDrop::FileCategory::Document);
    assert(resumePdf->format == EdgeDrop::TargetFormat::PDF);

    // Category filtering
    auto photos = manager.presetsForCategory(EdgeDrop::FileCategory::Photo);
    auto sigs = manager.presetsForCategory(EdgeDrop::FileCategory::Signature);
    auto docs = manager.presetsForCategory(EdgeDrop::FileCategory::Document);

    std::cout << "Categories: " << photos.size() << " photos, "
              << sigs.size() << " signatures, " << docs.size() << " documents." << std::endl;

    assert(!photos.isEmpty());
    assert(!sigs.isEmpty());
    assert(!docs.isEmpty());

    // SettingsManager Custom Card Size Persistence & Reset
    EdgeDrop::SettingsManager settings;
    settings.setCustomCardWidth(450);
    settings.setCustomCardHeight(520);
    assert(settings.customCardWidth() == 450);
    assert(settings.customCardHeight() == 520);
    settings.resetCustomCardSize();
    assert(settings.customCardWidth() == 0);
    assert(settings.customCardHeight() == 0);
    std::cout << "[PASS] SettingsManager custom resize persistence verified." << std::endl;

    std::cout << "[PASS] Preset Validation test passed successfully!" << std::endl;
    return 0;
}
