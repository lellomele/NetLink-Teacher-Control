// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "NetLinkUi.h"
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
    m_enabled(new QCheckBox(NetLinkUi::text("Enable WS-Discovery"), this))
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
    layout->addRow(NetLinkUi::text("Master name/IP (clients on other subnets)"), m_master);
    layout->addRow(NetLinkUi::text("Allowed resources: one IP or CIDR per line"), m_resources);
    m_resources->setProperty("netlinkEasyHidden",true);
    layout->labelForField(m_resources)->setProperty("netlinkEasyHidden",true);
    m_master->setToolTip(NetLinkUi::text("Leave empty when teacher and client are on the same subnet. Across subnets, enter the teacher's name or IPv4 address. This is not an authentication key."));
    auto help = new QLabel(NetLinkUi::text("Find clients discovers remote computers and can import them into the Console.\nOn the same subnet the Master name/IP can stay empty; across subnets configure it on the client.\nDiscovery does not replace keys, access rules or network permissions."), this);
    help->setTextFormat(Qt::PlainText);
    help->setWordWrap(true);
    layout->addRow(help);
    auto policyHelp=new QLabel(NetLinkUi::text("InternetGuard reuses computers selected in the Master. Do not allow entire subnets unnecessarily: each allowed resource remains reachable on all ports."),this);
    policyHelp->setTextFormat(Qt::PlainText);policyHelp->setWordWrap(true);policyHelp->setProperty("netlinkEasyHidden",true);layout->addRow(policyHelp);
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
    if(entries.size() > 128) return NetLinkUi::text("At most 128 additional resources.");
    for(const auto& entry : entries) {
        if(!SchoolNetwork::validAddress(entry)) return NetLinkUi::text("Invalid resource: ") + entry.left(120);
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
