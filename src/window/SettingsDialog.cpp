#include "SettingsDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QStyleFactory>
#include <QApplication>
#include <QActionGroup>

SettingsDialog::SettingsDialog(QSettings &settings, QActionGroup *languageGroup, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(350);

    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    // --- Theme ---
    m_themeCombo = new QComboBox(this);
    QStringList styles = QStyleFactory::keys();
    m_themeCombo->addItems(styles);

    QString currentStyle = QApplication::style()->objectName();
    for (int i = 0; i < styles.size(); ++i)
    {
        if (styles[i].compare(currentStyle, Qt::CaseInsensitive) == 0)
        {
            m_themeCombo->setCurrentIndex(i);
            break;
        }
    }
    form->addRow(tr("Theme:"), m_themeCombo);

    // --- Language ---
    m_languageCombo = new QComboBox(this);
    QString savedLocale = settings.value("ui/language", "en_US").toString();
    int langIdx = 0;

    if (languageGroup)
    {
        int i = 0;
        for (QAction *action : languageGroup->actions())
        {
            m_languageCombo->addItem(action->text(), action->data());
            if (action->data().toString() == savedLocale)
            {
                langIdx = i;
            }
            ++i;
        }
    }
    m_languageCombo->setCurrentIndex(langIdx);
    form->addRow(tr("Language:"), m_languageCombo);

    // --- Font size ---
    m_fontSizeSpin = new QSpinBox(this);
    m_fontSizeSpin->setRange(6, 24);
    int defaultSize = QApplication::font().pointSize();
    if (defaultSize < 6) { defaultSize = 9; }
    m_fontSizeSpin->setValue(settings.value("ui/fontSize", defaultSize).toInt());
    m_fontSizeSpin->setSuffix(" pt");
    form->addRow(tr("Font size:"), m_fontSizeSpin);

    // --- Restore window ---
    m_restoreWindowCheck = new QCheckBox(tr("Restore window layout on startup"), this);
    m_restoreWindowCheck->setChecked(settings.value("ui/restoreWindowGeometry", false).toBool());
    form->addRow(m_restoreWindowCheck);

    mainLayout->addLayout(form);
    mainLayout->addSpacing(10);

    // --- Buttons ---
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

QString SettingsDialog::selectedTheme() const
{
    return m_themeCombo->currentText();
}

QString SettingsDialog::selectedLanguage() const
{
    return m_languageCombo->currentData().toString();
}

bool SettingsDialog::restoreWindowEnabled() const
{
    return m_restoreWindowCheck->isChecked();
}

int SettingsDialog::selectedFontSize() const
{
    return m_fontSizeSpin->value();
}
