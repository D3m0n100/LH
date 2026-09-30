#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

class QApplication;
class QString;

class ThemeManager
{
public:
    enum class ThemeMode {
        Light,
        Dark
    };

    static void applyModernTheme(QApplication* app);
    static void applyTheme(QApplication* app, ThemeMode mode);
    static QString groupBoxStyleSheet(bool muted = false);

private:
    static void applyLightPalette(QApplication* app);
    static QString buildLightStyleSheet();
};

#endif // THEME_MANAGER_H
