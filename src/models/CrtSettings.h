#pragma once

#include <QJsonObject>

struct CrtSettings {
    float scanlineIntensity = 0.0f;
    float scanlineWidth = 0.5f;
    float blurRadius = 0.0f;
    float bloomIntensity = 0.0f;
    float bloomRadius = 0.5f;
    float afterglowStrength = 0.0f;
    float afterglowDecaySpeed = 5.0f;
    int tintMode = 0;       // 0=off, 1=amber, 2=green, 3=mono
    float tintStrength = 1.0f;
    int maskType = 0;       // 0=off, 1=aperture, 2=shadow, 3=slot
    float maskIntensity = 0.0f;
    float curvatureAmount = 0.0f;
    float vignetteStrength = 0.0f;

    bool operator==(const CrtSettings &) const = default;

    QJsonObject toJson() const {
        return {
            {"scanlineIntensity", scanlineIntensity},
            {"scanlineWidth", scanlineWidth},
            {"blurRadius", blurRadius},
            {"bloomIntensity", bloomIntensity},
            {"bloomRadius", bloomRadius},
            {"afterglowStrength", afterglowStrength},
            {"afterglowDecaySpeed", afterglowDecaySpeed},
            {"tintMode", tintMode},
            {"tintStrength", tintStrength},
            {"maskType", maskType},
            {"maskIntensity", maskIntensity},
            {"curvatureAmount", curvatureAmount},
            {"vignetteStrength", vignetteStrength}
        };
    }

    static CrtSettings fromJson(const QJsonObject &obj) {
        CrtSettings s;
        s.scanlineIntensity = static_cast<float>(obj["scanlineIntensity"].toDouble());
        s.scanlineWidth = static_cast<float>(obj["scanlineWidth"].toDouble(0.5));
        s.blurRadius = static_cast<float>(obj["blurRadius"].toDouble());
        s.bloomIntensity = static_cast<float>(obj["bloomIntensity"].toDouble());
        s.bloomRadius = static_cast<float>(obj["bloomRadius"].toDouble(0.5));
        s.afterglowStrength = static_cast<float>(obj["afterglowStrength"].toDouble());
        s.afterglowDecaySpeed = static_cast<float>(obj["afterglowDecaySpeed"].toDouble(5.0));
        s.tintMode = obj["tintMode"].toInt();
        s.tintStrength = static_cast<float>(obj["tintStrength"].toDouble(1.0));
        s.maskType = obj["maskType"].toInt();
        s.maskIntensity = static_cast<float>(obj["maskIntensity"].toDouble());
        s.curvatureAmount = static_cast<float>(obj["curvatureAmount"].toDouble());
        s.vignetteStrength = static_cast<float>(obj["vignetteStrength"].toDouble());
        return s;
    }

    bool hasEffects() const {
        return scanlineIntensity > 0 || blurRadius > 0 || bloomIntensity > 0 ||
               tintMode > 0 || afterglowStrength > 0 || maskType > 0 ||
               curvatureAmount > 0 || vignetteStrength > 0;
    }
};
