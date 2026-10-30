#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

class QApplication;
class QString;
class QColor;

class ThemeManager
{
public:
    enum class ThemeMode {
        Light,
        Dark
    };
    enum class Density {
        Compact,
        Comfortable
    };

    static void applyModernTheme(QApplication* app);
    static void applyTheme(QApplication* app, ThemeMode mode);
    static void applyDensity(QApplication* app, Density density);
    static ThemeMode themeMode();
    static Density density();
    static QString densityStyleSheet(Density density);
    static double contrastRatio(const QColor& foreground, const QColor& background);
    static QString groupBoxStyleSheet(bool muted = false);

private:
    static void applyLightPalette(QApplication* app);
    static void applyDarkPalette(QApplication* app);
    static QString buildLightStyleSheet();
    static QString buildDarkStyleSheet();
};

#endif // THEME_MANAGER_H
