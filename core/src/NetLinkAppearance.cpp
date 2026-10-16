// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025-26 prof. Ing. Raffaele Mele
#undef QT_NO_CAST_FROM_ASCII
#undef QT_USE_QSTRINGBUILDER
#include "NetLinkAppearance.h"
#include "NetLinkUi.h"
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QSettings* userSettings(QObject* parent = nullptr)
{
    // Deliberately separate from shared Veyon system configuration and keys.
    return new QSettings(QSettings::NativeFormat, QSettings::UserScope,
        "Veyon", "NetLinkAppearance", parent);
}
bool highContrast()
{
#ifdef Q_OS_WIN
    HIGHCONTRASTW settings{};
    settings.cbSize = sizeof(settings);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(settings), &settings, 0) &&
        (settings.dwFlags & HCF_HIGHCONTRASTON);
#else
    return false;
#endif
}
bool systemDark(const QPalette& original)
{
#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
    return !settings.value("AppsUseLightTheme", 1).toBool();
#else
    return original.color(QPalette::Window).lightness() < 128;
#endif
}
}

NetLinkAppearance::Preferences NetLinkAppearance::normalized(Preferences preferences)
{
    if(!QStringList({"System", "Light", "Dark"}).contains(preferences.mode)) preferences.mode = "System";
    if(!QStringList({"Oceano", "Bosco", "Prugna", "Personalizzato"}).contains(preferences.preset)) preferences.preset = "Oceano";
    if(preferences.preset != "Personalizzato") preferences.accent = presetColor(preferences.preset);
    if(!preferences.accent.isValid()) preferences.accent = presetColor("Oceano");
    preferences.accent.setAlpha(255);
    if(!QStringList({"Organic", "Outline", "Classic"}).contains(preferences.iconSet)) preferences.iconSet="Organic";
    if(!QList<int>({28,36,44}).contains(preferences.toolbarIconSize)) preferences.toolbarIconSize=36;
    return preferences;
}

QColor NetLinkAppearance::presetColor(const QString& preset)
{
    if(preset == "Bosco") return QColor("#0f766e");
    if(preset == "Prugna") return QColor("#7c3aed");
    return QColor("#2563eb");
}

NetLinkAppearance::Preferences NetLinkAppearance::load(QSettings& settings)
{
    Preferences preferences;
    preferences.mode = settings.value("Mode", "System").toString();
    preferences.preset = settings.value("Preset", "Oceano").toString();
    preferences.accent = QColor(settings.value("Accent", "#2563eb").toString());
    preferences.iconSet = settings.value("IconSet", "Organic").toString();
    preferences.toolbarIconSize = settings.value("ToolbarIconSize", 36).toInt();
    return normalized(preferences);
}

bool NetLinkAppearance::save(QSettings& settings, const Preferences& input)
{
    const auto preferences = normalized(input);
    settings.setValue("Mode", preferences.mode);
    settings.setValue("Preset", preferences.preset);
    settings.setValue("Accent", preferences.accent.name());
    settings.setValue("IconSet", preferences.iconSet);
    settings.setValue("ToolbarIconSize", preferences.toolbarIconSize);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

QColor NetLinkAppearance::contrastingText(const QColor& background)
{
    const auto linear = [](double value) { return value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4); };
    const auto luminance = .2126 * linear(background.redF()) + .7152 * linear(background.greenF()) + .0722 * linear(background.blueF());
    return (luminance + .05) / .05 >= 1.05 / (luminance + .05) ? QColor(Qt::black) : QColor(Qt::white);
}

QPalette NetLinkAppearance::makePalette(const Preferences& input, bool darkSystem)
{
    const auto preferences = normalized(input);
    const bool dark = preferences.mode == "Dark" || (preferences.mode == "System" && darkSystem);
    const QColor window(dark ? "#111923" : "#f3f5f9");
    const QColor panel(dark ? "#1a2533" : "#ffffff");
    const QColor text(dark ? "#e9eef5" : "#202c3d");
    const QColor muted(dark ? "#b4c0d0" : "#536278");
    const QColor line(dark ? "#354458" : "#dce3ec");
    QPalette palette;
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, panel);
    palette.setColor(QPalette::AlternateBase, window);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, panel);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::ToolTipBase, panel);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Light, dark ? QColor("#43546a") : QColor("#ffffff"));
    palette.setColor(QPalette::Midlight, dark ? QColor("#253449") : QColor("#eaf0f8"));
    palette.setColor(QPalette::Mid, line);
    palette.setColor(QPalette::Dark, line.darker(125));
    palette.setColor(QPalette::Shadow, QColor(dark ? "#080d14" : "#b3bdca"));
    palette.setColor(QPalette::Highlight, preferences.accent);
    palette.setColor(QPalette::HighlightedText, contrastingText(preferences.accent));
    // Link contrast does not depend on arbitrary user-selected accent colors.
    palette.setColor(QPalette::Link, dark ? QColor("#93c5fd") : QColor("#1d4ed8"));
    palette.setColor(QPalette::LinkVisited, dark ? QColor("#d8b4fe") : QColor("#6d28d9"));
    for(auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText}) palette.setColor(QPalette::Disabled, role, muted);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, line);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, text);
    return palette;
}

NetLinkAppearance* NetLinkAppearance::install(QSettings* settings)
{
    auto manager = qApp->findChild<NetLinkAppearance*>("netLinkAppearanceManager", Qt::FindDirectChildrenOnly);
    return manager ? manager : new NetLinkAppearance(qApp, settings);
}

NetLinkAppearance::NetLinkAppearance(QObject* parent, QSettings* settings) : QObject(parent),
    m_originalPalette(qApp->palette()), m_originalStyle(qApp->style()->objectName()),
    m_settings(settings ? settings : userSettings(this))
{
    setObjectName("netLinkAppearanceManager");
    m_preferences = load(*m_settings);
    qApp->installNativeEventFilter(this);
    apply();
}

NetLinkAppearance::~NetLinkAppearance()
{
    if(qApp) qApp->removeNativeEventFilter(this);
}

void NetLinkAppearance::apply()
{
    m_highContrast = highContrast();
    qApp->setProperty("netlinkHighContrast",m_highContrast);
    m_darkSystem = systemDark(m_originalPalette);
    // Preserve the Windows accessibility palette and native style in high contrast.
    if(m_highContrast) {
        if(qApp->style()->objectName() != m_originalStyle) qApp->setStyle(m_originalStyle);
        qApp->setStyleSheet({});
        qApp->setPalette(qApp->style()->standardPalette());
    } else {
        if(qApp->style()->objectName().compare("fusion", Qt::CaseInsensitive) != 0) qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setPalette(makePalette(m_preferences, m_darkSystem));
        qApp->setStyleSheet("QToolBar{border:0;padding:6px;spacing:5px;}"
            "QToolButton[netlinkFeatureButton=\"true\"]{border:1px solid palette(mid);border-radius:10px;padding:8px;background:palette(button);color:palette(button-text);margin:1px;}"
            "QToolButton[netlinkFeatureButton=\"true\"]:hover{background:palette(midlight);border-color:palette(highlight);}"
            "QToolButton[netlinkFeatureButton=\"true\"]:checked{background:palette(midlight);border:2px solid palette(highlight);padding:7px;}"
            "QToolButton[netlinkFeatureButton=\"true\"]:focus{border:2px solid palette(highlight);padding:7px;}"
            "QToolTip{padding:8px;border:1px solid palette(mid);background:palette(tool-tip-base);color:palette(tool-tip-text);}"
            "QPushButton,QComboBox,QLineEdit{padding:6px;border:1px solid palette(mid);border-radius:5px;}"
            "QPushButton{background:palette(button);color:palette(button-text);}"
            "QPushButton:hover{background:palette(midlight);}"
            "QPushButton:default,QPushButton:checked{background:palette(highlight);color:palette(highlighted-text);}"
            "QPushButton:focus,QComboBox:focus,QLineEdit:focus{border:2px solid palette(highlight);padding:5px;}"
            "QPushButton:disabled{color:palette(mid);}"
            "QGroupBox{border:1px solid palette(mid);border-radius:6px;margin-top:10px;padding:10px;}"
            "QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 5px;}"
            "QTabWidget::pane{border:1px solid palette(mid);}QStatusBar{border-top:1px solid palette(mid);}"
            "QSplitter::handle:hover{background:palette(highlight);}");
    }
    Q_EMIT changed();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool NetLinkAppearance::nativeEventFilter(const QByteArray&, void* message, qintptr*)
#else
bool NetLinkAppearance::nativeEventFilter(const QByteArray&, void* message, long*)
#endif
{
#ifdef Q_OS_WIN
    const auto native = static_cast<MSG*>(message);
    if(native && (native->message == WM_SETTINGCHANGE || native->message == WM_THEMECHANGED || native->message == WM_SYSCOLORCHANGE) && !m_queued) {
        m_queued = true;
        QTimer::singleShot(0, this, [this] {
            m_queued = false;
            if(highContrast() != m_highContrast || (m_preferences.mode == "System" && systemDark(m_originalPalette) != m_darkSystem)) apply();
        });
    }
#else
    Q_UNUSED(message)
#endif
    return false;
}

void NetLinkAppearance::showSettings(QWidget* parent)
{
    if(m_dialogOpen) return;
    m_dialogOpen = true;
    const auto previous = m_preferences;
    auto pending = previous;
    QDialog dialog(parent);
    dialog.setWindowTitle(NetLinkUi::windowTitle("Aspetto di NetLink"));
    dialog.setObjectName("netLinkAppearanceDialog");
    dialog.setMinimumWidth(360);
    auto layout = new QVBoxLayout(&dialog);
    auto form = new QFormLayout;
    auto mode = new QComboBox(&dialog);
    mode->setObjectName("appearanceMode");
    mode->addItem("Sistema", "System"); mode->addItem("Chiaro", "Light"); mode->addItem("Scuro", "Dark");
    mode->setCurrentIndex(mode->findData(pending.mode));
    auto preset = new QComboBox(&dialog);
    preset->setObjectName("appearancePreset");
    preset->addItems({"Oceano", "Bosco", "Prugna", "Personalizzato"});
    preset->setCurrentText(pending.preset);
    auto color = new QPushButton(pending.accent.name(), &dialog);
    color->setObjectName("appearanceColor");
    form->addRow("Tema", mode); form->addRow("Palette", preset); form->addRow("Colore d’accento", color);
    auto iconSet=new QComboBox(&dialog);iconSet->setObjectName("appearanceIconSet");
    iconSet->addItem("Organico — forme morbide e coordinate","Organic");
    iconSet->addItem("Lineare — contorni essenziali","Outline");
    iconSet->addItem("Classico — icone precedenti","Classic");
    iconSet->setCurrentIndex(iconSet->findData(pending.iconSet));
    auto iconSize=new QComboBox(&dialog);iconSize->setObjectName("appearanceIconSize");
    iconSize->addItem("Compatta (28)",28);iconSize->addItem("Comoda (36)",36);iconSize->addItem("Grande (44)",44);
    iconSize->setCurrentIndex(iconSize->findData(pending.toolbarIconSize));
    form->addRow("Set di icone",iconSet);form->addRow("Dimensione pulsanti",iconSize);
    auto iconPreview=new QLabel(&dialog);iconPreview->setObjectName("appearanceIconPreview");
    form->addRow("Anteprima",iconPreview);
    layout->addLayout(form);
    auto note = new QLabel("Anteprima immediata. Le preferenze sono personali e non richiedono diritti amministrativi.\nAnnulla ripristina l’aspetto precedente.", &dialog);
    note->setWordWrap(true); layout->addWidget(note);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("Salva");
    buttons->button(QDialogButtonBox::Cancel)->setText("Annulla");
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText("Predefiniti");
    layout->addWidget(buttons);
    const auto preview = [&] { pending = normalized(pending); color->setText(pending.accent.name()); m_preferences = pending; apply();
        auto pixels=NetLinkUi::icon("InternetGuard",pending.iconSet,pending.accent).pixmap(QSize(48,48)*dialog.devicePixelRatioF());
        pixels.setDevicePixelRatio(dialog.devicePixelRatioF());iconPreview->setPixmap(pixels); };
    preview();
    connect(mode, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [&](int) { pending.mode = mode->currentData().toString(); preview(); });
    connect(preset, &QComboBox::currentTextChanged, &dialog, [&](const QString& value) { pending.preset = value; preview(); });
    connect(iconSet,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&](int){pending.iconSet=iconSet->currentData().toString();preview();});
    connect(iconSize,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&](int){pending.toolbarIconSize=iconSize->currentData().toInt();preview();});
    connect(color, &QPushButton::clicked, &dialog, [&] {
        const auto chosen = QColorDialog::getColor(pending.accent, &dialog, "Colore d’accento");
        if(!chosen.isValid()) return;
        pending.preset = "Personalizzato"; pending.accent = chosen;
        preset->setCurrentText(pending.preset); preview();
    });
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &dialog, [&] {
        pending = Preferences{}; mode->setCurrentIndex(mode->findData(pending.mode)); preset->setCurrentText(pending.preset);
        iconSet->setCurrentIndex(iconSet->findData(pending.iconSet));iconSize->setCurrentIndex(iconSize->findData(pending.toolbarIconSize));preview();
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if(!save(*m_settings, pending)) { QMessageBox::warning(&dialog, "NetLink", "Salvataggio delle preferenze non riuscito."); return; }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if(dialog.exec() != QDialog::Accepted) { m_preferences = previous; apply(); }
    m_dialogOpen = false;
}
