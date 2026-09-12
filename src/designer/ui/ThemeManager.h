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

private:
    static void applyLightPalette(QApplication* app);
    static QString buildLightStyleSheet();
};

#endif // THEME_MANAGER_H
