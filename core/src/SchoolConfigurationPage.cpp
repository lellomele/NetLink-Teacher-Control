// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "SchoolConfigurationPage.h"
#include "SchoolNetworkPolicy.h"
#include "VeyonConfiguration.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTextEdit>

SchoolConfigurationPage::SchoolConfigurationPage(VeyonConfiguration& configuration, QWidget* parent) :
    ConfigurationPage(parent), m_configuration(configuration),
    m_master(new QLineEdit(this)), m_resources(new QTextEdit(this)),
    m_enabled(new QCheckBox("Abilita WS-Discovery", this))
{
    setWindowTitle("NetLink");
    setWindowIcon(QIcon(":/core/veyon.png"));
    m_master->setObjectName("schoolDiscoveryMaster");
    m_resources->setObjectName("schoolResources");
    m_enabled->setObjectName("schoolDiscoveryEnabled");
    m_master->setMaxLength(253);
    m_resources->setAcceptRichText(false);
    auto layout = new QFormLayout(this);
    layout->addRow(m_enabled);
    layout->addRow("Nome/IP del Master (client di altre subnet)", m_master);
    layout->addRow("Risorse autorizzate: un IP o CIDR per riga", m_resources);
    auto help = new QLabel("I PC selezionati nel Master sono riutilizzati automaticamente.\n"
        "Non inserire intere subnet senza necessità: ogni destinazione consentita resta raggiungibile su tutte le porte.\n"
        "Il discovery non sostituisce le chiavi e le regole di accesso Veyon.", this);
    help->setTextFormat(Qt::PlainText);
    help->setWordWrap(true);
    layout->addRow(help);
    resetWidgets();
    connectWidgetsToProperties();
}

void SchoolConfigurationPage::resetWidgets()
{
    // Reset must not change the model or mark freshly loaded settings as dirty.
    const QSignalBlocker masterBlocker(m_master);
    const QSignalBlocker resourcesBlocker(m_resources);
    const QSignalBlocker enabledBlocker(m_enabled);
    m_master->setText(m_configuration.schoolDiscoveryMaster());
    m_resources->setPlainText(m_configuration.schoolResources().join('\n'));
    m_enabled->setChecked(m_configuration.schoolDiscoveryEnabled());
}

void SchoolConfigurationPage::connectWidgetsToProperties()
{
    if(m_connected) return;
    m_connected = true;
    // Configurator listens to configurationChanged, not widgetsChanged, to enable Apply.
    // Bind before persistence, just like the other configuration pages.
    connect(m_master, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_configuration.setSchoolDiscoveryMaster(text.trimmed());
    });
    connect(m_enabled, &QCheckBox::toggled, this, [this](bool enabled) {
        m_configuration.setSchoolDiscoveryEnabled(enabled);
    });
    connect(m_resources, &QTextEdit::textChanged, this, [this] {
        m_configuration.setSchoolResources(resources());
    });
}

QStringList SchoolConfigurationPage::resources() const
{
    QStringList result;
    for(const auto& line : m_resources->toPlainText().split('\n')) {
        const auto trimmed = line.trimmed();
        if(!trimmed.isEmpty()) result.append(trimmed);
    }
    return result;
}

QString SchoolConfigurationPage::validationError() const
{
    const auto entries = resources();
    if(entries.size() > 128) return "Massimo 128 risorse aggiuntive.";
    for(const auto& entry : entries) {
        if(!SchoolNetwork::validAddress(entry)) return "Risorsa non valida: " + entry.left(120);
    }
    return {};
}

bool SchoolConfigurationPage::validateConfiguration()
{
    const auto error = validationError();
    if(error.isEmpty()) return true;
    QMessageBox::warning(this, "NetLink", error);
    return false;
}

void SchoolConfigurationPage::applyConfiguration()
{
    // Values are already staged in the configuration and validated before saving.
    // No post-save mutations: these would be lost on restart.
}
