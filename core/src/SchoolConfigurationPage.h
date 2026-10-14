// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "ConfigurationPage.h"

class QCheckBox;
class QLineEdit;
class QTextEdit;
class VeyonConfiguration;

class VEYON_CORE_EXPORT SchoolConfigurationPage : public ConfigurationPage
{
    Q_OBJECT
public:
    explicit SchoolConfigurationPage(VeyonConfiguration& configuration, QWidget* parent = nullptr);
    void resetWidgets() override;
    void connectWidgetsToProperties() override;
    bool validateConfiguration() override;
    void applyConfiguration() override;
    QString validationError() const;

private:
    QStringList resources() const;
    VeyonConfiguration& m_configuration;
    QLineEdit* m_master;
    QTextEdit* m_resources;
    QCheckBox* m_enabled;
    bool m_connected = false;
};
