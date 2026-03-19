#pragma once

#include "AppTypes.h"

#include <QString>

class SettingsManager
{
public:
    SettingsManager();

    AppSettings load() const;
    void save(const AppSettings& settings) const;
    QString configPath() const;

private:
    QString m_configPath;
};
