#pragma once

#include "models/CrtSettings.h"
#include <QString>
#include <QList>

struct CrtPreset {
    QString id;
    QString name;
    CrtSettings settings;
    bool builtIn = false;

    static QList<CrtPreset> builtInPresets() {
        return {
            // Clean — all defaults (no effects)
            {"clean", "Clean", CrtSettings{}, true},

            // Home CRT
            {"homeCRT", "Home CRT", CrtSettings{
                .scanlineIntensity = 0.4f, .blurRadius = 0.3f,
                .bloomIntensity = 0.25f, .bloomRadius = 0.5f,
                .maskType = 2, .maskIntensity = 0.3f,
                .curvatureAmount = 0.3f, .vignetteStrength = 0.2f
            }, true},

            // P3 Amber
            {"p3Amber", "P3 Amber", CrtSettings{
                .scanlineIntensity = 0.35f, .blurRadius = 0.2f,
                .bloomIntensity = 0.3f, .bloomRadius = 0.5f,
                .afterglowStrength = 0.5f, .tintMode = 1,
                .maskType = 1, .maskIntensity = 0.15f,
                .curvatureAmount = 0.25f, .vignetteStrength = 0.3f
            }, true},

            // P1 Green
            {"p1Green", "P1 Green", CrtSettings{
                .scanlineIntensity = 0.35f, .blurRadius = 0.2f,
                .bloomIntensity = 0.35f, .bloomRadius = 0.5f,
                .afterglowStrength = 0.6f, .tintMode = 2,
                .maskType = 1, .maskIntensity = 0.15f,
                .curvatureAmount = 0.25f, .vignetteStrength = 0.3f
            }, true},

            // Crisp
            {"crisp", "Crisp", CrtSettings{
                .scanlineIntensity = 0.15f
            }, true},

            // Warm Glow
            {"warmGlow", "Warm Glow", CrtSettings{
                .scanlineIntensity = 0.25f, .blurRadius = 0.3f,
                .bloomIntensity = 0.4f, .bloomRadius = 0.5f,
                .afterglowStrength = 0.7f, .afterglowDecaySpeed = 3.5f,
                .maskType = 2, .maskIntensity = 0.2f,
                .vignetteStrength = 0.15f
            }, true},

            // Old TV
            {"oldTV", "Old TV", CrtSettings{
                .scanlineIntensity = 0.5f, .scanlineWidth = 0.6f,
                .blurRadius = 0.4f, .bloomIntensity = 0.3f, .bloomRadius = 0.5f,
                .afterglowStrength = 0.3f,
                .maskType = 3, .maskIntensity = 0.35f,
                .curvatureAmount = 0.5f, .vignetteStrength = 0.4f
            }, true},

            // Arcade
            {"arcade", "Arcade", CrtSettings{
                .scanlineIntensity = 0.35f, .blurRadius = 0.2f,
                .bloomIntensity = 0.5f, .bloomRadius = 0.5f,
                .maskType = 1, .maskIntensity = 0.25f,
                .curvatureAmount = 0.2f, .vignetteStrength = 0.15f
            }, true},
        };
    }
};
