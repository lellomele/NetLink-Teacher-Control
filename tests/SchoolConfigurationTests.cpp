// SPDX-License-Identifier: GPL-2.0-or-later
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include <QtTest>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <memory>
#include "Configuration/JsonStore.h"
#include "Configuration/LocalStore.h"
#include "SchoolConfigurationPage.h"
#include "VeyonConfiguration.h"

// Uses the real page/model with an isolated JSON store, never HKLM, keys or services.
// Apply uses Configurator's order: validate, persist model, then page apply hooks.
class SchoolConfigurationTests : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_directory;
    std::unique_ptr<Configuration::JsonStore> m_store;
    std::unique_ptr<VeyonConfiguration> m_configuration;
    std::unique_ptr<SchoolConfigurationPage> m_page;
    std::unique_ptr<QDialogButtonBox> m_buttons;
    QLineEdit* master() { return m_page->findChild<QLineEdit*>("schoolDiscoveryMaster"); }
    QCheckBox* enabled() { return m_page->findChild<QCheckBox*>("schoolDiscoveryEnabled"); }
    QTextEdit* resources() { return m_page->findChild<QTextEdit*>("schoolResources"); }
    QPushButton* apply() { return m_buttons->button(QDialogButtonBox::Apply); }

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_directory.isValid());
        const auto path = m_directory.filePath("configuration.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("{}"), qint64(2));
        file.close();
        m_store.reset(new Configuration::JsonStore(Configuration::Store::System, path));
        m_configuration.reset(new VeyonConfiguration(m_store.get()));
        m_configuration->setSchoolDiscoveryMaster({});
        m_configuration->setSchoolDiscoveryEnabled(true);
        m_configuration->setSchoolResources({});
        m_store->flush(m_configuration.get());
        m_page.reset(new SchoolConfigurationPage(*m_configuration));
        m_buttons.reset(new QDialogButtonBox(QDialogButtonBox::Apply));
        apply()->setEnabled(false);
        connect(m_configuration.get(), &VeyonConfiguration::configurationChanged,
            m_buttons.get(), [this] { apply()->setEnabled(true); });
        connect(apply(), &QPushButton::clicked, m_page.get(), [this] {
            if(!m_page->validateConfiguration()) return;
            m_store->flush(m_configuration.get());
            m_page->applyConfiguration();
            apply()->setEnabled(false);
        });
        QVERIFY(master()); QVERIFY(enabled()); QVERIFY(resources());
    }

    void cleanup()
    {
        m_buttons.reset(); m_page.reset(); m_configuration.reset(); m_store.reset();
    }

    void individualFields_data()
    {
        QTest::addColumn<QString>("field");
        QTest::newRow("master-address") << QString("master");
        QTest::newRow("discovery-checkbox") << QString("enabled");
        QTest::newRow("allowed-resources") << QString("resources");
    }

    void individualFields()
    {
        QFETCH(QString, field);
        if(field == "master") master()->setText("192.0.2.25");
        else if(field == "enabled") enabled()->setChecked(false);
        else resources()->setPlainText("192.0.2.30\n2001:db8::/64");
        QVERIFY(apply()->isEnabled());
        const auto staged = m_configuration->data();
        apply()->click();
        QVERIFY(!apply()->isEnabled());
        // Closing and recreating both the configuration and the page must retain values.
        m_page.reset();
        VeyonConfiguration reopened(m_store.get());
        QCOMPARE(reopened.data(), staged);
        SchoolConfigurationPage reopenedPage(reopened);
        QCOMPARE(reopenedPage.findChild<QLineEdit*>("schoolDiscoveryMaster")->text(), reopened.schoolDiscoveryMaster());
        QCOMPARE(reopenedPage.findChild<QCheckBox*>("schoolDiscoveryEnabled")->isChecked(), reopened.schoolDiscoveryEnabled());
        QCOMPARE(reopenedPage.findChild<QTextEdit*>("schoolResources")->toPlainText(), reopened.schoolResources().join('\n'));
    }

    void simultaneousEditsAndClearing()
    {
        master()->setText(" teacher.example.test ");
        enabled()->setChecked(false);
        resources()->setPlainText(" 192.0.2.30 \n \n2001:db8::/64\n");
        apply()->click();
        VeyonConfiguration reopened(m_store.get());
        QCOMPARE(reopened.schoolDiscoveryMaster(), QString("teacher.example.test"));
        QVERIFY(!reopened.schoolDiscoveryEnabled());
        QCOMPARE(reopened.schoolResources(), QStringList({"192.0.2.30", "2001:db8::/64"}));
        master()->clear(); resources()->clear(); enabled()->setChecked(true);
        QVERIFY(apply()->isEnabled()); apply()->click();
        reopened.clear(); reopened.reloadFromStore();
        QVERIFY(reopened.schoolDiscoveryMaster().isEmpty());
        QVERIFY(reopened.schoolResources().isEmpty());
        QVERIFY(reopened.schoolDiscoveryEnabled());
    }

    void resetDoesNotDirtyModel()
    {
        const auto before = m_configuration->data();
        QSignalSpy changes(m_configuration.get(), &VeyonConfiguration::configurationChanged);
        m_page->resetWidgets(); m_page->resetWidgets();
        QCOMPARE(changes.count(), 0); QCOMPARE(m_configuration->data(), before);
        QVERIFY(!apply()->isEnabled());
    }

    void resetDiscardsUnsavedChanges()
    {
        const auto saved = m_configuration->data();
        master()->setText("192.0.2.25"); enabled()->setChecked(false);
        resources()->setPlainText("192.0.2.30");
        m_configuration->clear(); m_configuration->reloadFromStore();
        m_page->resetWidgets(); apply()->setEnabled(false);
        QCOMPARE(m_configuration->data(), saved);
        QVERIFY(master()->text().isEmpty()); QVERIFY(resources()->toPlainText().isEmpty());
        QVERIFY(enabled()->isChecked()); QVERIFY(!apply()->isEnabled());
    }

    void invalidResourceDoesNotPersistAnyField()
    {
        const auto saved = m_configuration->data();
        master()->setText("192.0.2.25"); enabled()->setChecked(false);
        resources()->setPlainText("192.0.2.30\n0.0.0.0/0");
        QVERIFY(apply()->isEnabled()); QVERIFY(!m_page->validationError().isEmpty());
        bool warningShown = false;
        QTimer::singleShot(0, this, [&warningShown] {
            for(auto widget : QApplication::topLevelWidgets()) {
                if(auto warning = qobject_cast<QMessageBox*>(widget)) {
                    warningShown = true; warning->accept();
                }
            }
        });
        apply()->click();
        QVERIFY(warningShown); QVERIFY(apply()->isEnabled());
        VeyonConfiguration reopened(m_store.get()); QCOMPARE(reopened.data(), saved);
        // Correcting the input makes the same staged edits saveable.
        resources()->setPlainText("192.0.2.30"); apply()->click();
        reopened.clear(); reopened.reloadFromStore();
        QCOMPARE(reopened.schoolDiscoveryMaster(), QString("192.0.2.25"));
        QVERIFY(!reopened.schoolDiscoveryEnabled());
        QCOMPARE(reopened.schoolResources(), QStringList({"192.0.2.30"}));
    }

    void resourceLimit()
    {
        QStringList addresses;
        for(int index = 1; index <= 128; ++index) addresses.append(QString("192.0.2.%1").arg(index));
        resources()->setPlainText(addresses.join('\n'));
        QVERIFY(m_page->validationError().isEmpty());
        addresses.append("192.0.2.129"); resources()->setPlainText(addresses.join('\n'));
        QVERIFY(!m_page->validationError().isEmpty());
    }

    void repeatedConnectionIsIdempotent()
    {
        m_page->connectWidgetsToProperties(); m_page->connectWidgetsToProperties();
        QSignalSpy changes(m_configuration.get(), &VeyonConfiguration::configurationChanged);
        master()->setText("192.0.2.25");
        QCOMPARE(changes.count(), 1); QVERIFY(apply()->isEnabled());
    }

    void localStoreReopensSavedValues()
    {
        // Exercise Configurator's actual LocalStore/QSettings path, but force an
        // isolated INI backend. No production registry/settings are touched.
        class IsolatedLocalStore : public Configuration::LocalStore {
        public:
            explicit IsolatedLocalStore(const QString& path) : LocalStore(System), m_path(path) {}
            QSettings* createSettingsObject() const override { return new QSettings(m_path, QSettings::IniFormat); }
        private:
            const QString m_path;
        } localStore(m_directory.filePath("local-settings.ini"));
        const std::unique_ptr<QSettings> settings(localStore.createSettingsObject());
        QCOMPARE(settings->format(), QSettings::IniFormat);
        QVERIFY(QDir::cleanPath(settings->fileName()).startsWith(QDir::cleanPath(m_directory.path()) + '/'));
        QVERIFY(localStore.isWritable());
        VeyonConfiguration configuration(&localStore);
        SchoolConfigurationPage page(configuration);
        page.findChild<QLineEdit*>("schoolDiscoveryMaster")->setText("192.0.2.25");
        page.findChild<QCheckBox*>("schoolDiscoveryEnabled")->setChecked(false);
        page.findChild<QTextEdit*>("schoolResources")->setPlainText("192.0.2.30\n2001:db8::/64");
        QVERIFY(page.validateConfiguration());
        localStore.flush(&configuration);
        page.applyConfiguration();
        QVERIFY(QFileInfo::exists(settings->fileName()));
        VeyonConfiguration reopened(&localStore);
        QCOMPARE(reopened.schoolDiscoveryMaster(), QString("192.0.2.25"));
        QVERIFY(!reopened.schoolDiscoveryEnabled());
        QCOMPARE(reopened.schoolResources(), QStringList({"192.0.2.30", "2001:db8::/64"}));
        SchoolConfigurationPage reopenedPage(reopened);
        QCOMPARE(reopenedPage.findChild<QLineEdit*>("schoolDiscoveryMaster")->text(), QString("192.0.2.25"));
    }
};

QTEST_MAIN(SchoolConfigurationTests)
#include "SchoolConfigurationTests.moc"
