#pragma once

#include <QDialog>
#include <QSettings>

class QComboBox;
class QCheckBox;
class QSpinBox;
class QActionGroup;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QSettings &settings, QActionGroup *languageGroup, QWidget *parent = nullptr);

    QString selectedTheme() const;
    QString selectedLanguage() const;
    bool restoreWindowEnabled() const;
    int selectedFontSize() const;

private:
    QComboBox *m_themeCombo;
    QComboBox *m_languageCombo;
    QCheckBox *m_restoreWindowCheck;
    QSpinBox *m_fontSizeSpin;
};
