#include <QtTest/QtTest>
#include <QApplication>
#include <QPalette>
#include <QFile>
#include <QSettings>
#include <QColor>
#include <QHeaderView>
#include <QTableWidget>
#include <QToolButton>
#include <QMainWindow>
#include <QMenuBar>

#include "designer/SettingsController.h"
#include "designer/SettingsDialog.h"
#include "designer/ui/ThemeManager.h"
#include "designer/ui/NotificationBanner.h"

class ThemeDensityTest : public QObject
{
    Q_OBJECT

private slots:
    void cleanupTestCase()
    {
        ThemeManager::applyTheme(qApp, ThemeManager::ThemeMode::Light);
        ThemeManager::applyDensity(qApp, ThemeManager::Density::Compact);
    }

    void settingsPersistThemeAndDensity()
    {
        const QString org = QStringLiteral("LHThemeTest");
        const QString app = QStringLiteral("ThemeDensity-%1").arg(QCoreApplication::applicationPid());
        {
            SettingsController controller;
            controller.setSettingsStorage(org, app);
            controller.loadSettings();
            QCOMPARE(controller.themeMode(), 0);
            QCOMPARE(controller.density(), 0);
            controller.setThemeMode(1);
            controller.setDensity(1);
            controller.saveSettings();
        }
        {
            SettingsController controller;
            controller.setSettingsStorage(org, app);
            controller.loadSettings();
            QCOMPARE(controller.themeMode(), 1);
            QCOMPARE(controller.density(), 1);
        }
        QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, org, app);
        settings.clear();
    }

    void dialogExposesThemeAndDensity()
    {
        SettingsDialog dialog;
        dialog.setThemeMode(1);
        dialog.setDensity(1);
        QCOMPARE(dialog.themeMode(), 1);
        QCOMPARE(dialog.density(), 1);
        dialog.setThemeMode(0);
        dialog.setDensity(0);
        QCOMPARE(dialog.themeMode(), 0);
        QCOMPARE(dialog.density(), 0);
    }

    void densityStylesAreDistinct()
    {
        const QString compact = ThemeManager::densityStyleSheet(ThemeManager::Density::Compact);
        const QString comfortable = ThemeManager::densityStyleSheet(ThemeManager::Density::Comfortable);
        QVERIFY(!compact.isEmpty());
        QVERIFY(!comfortable.isEmpty());
        QVERIFY(compact != comfortable);
        QVERIFY(compact.contains(QStringLiteral("min-height: 18px")));
        QVERIFY(comfortable.contains(QStringLiteral("min-height: 24px")));
    }

    void themeAndDensityApplyAtRuntime()
    {
        ThemeManager::applyTheme(qApp, ThemeManager::ThemeMode::Light);
        const QColor lightWindow = qApp->palette().color(QPalette::Window);
        ThemeManager::applyTheme(qApp, ThemeManager::ThemeMode::Dark);
        const QColor darkWindow = qApp->palette().color(QPalette::Window);
        QVERIFY(lightWindow.lightness() > darkWindow.lightness());
        QVERIFY(qApp->styleSheet().contains(QStringLiteral("min-height: 18px")));

        ThemeManager::applyDensity(qApp, ThemeManager::Density::Comfortable);
        QVERIFY(qApp->styleSheet().contains(QStringLiteral("min-height: 24px")));
        QVERIFY(qApp->styleSheet().contains(QStringLiteral(":focus")));
        QCOMPARE(qApp->palette().color(QPalette::Window).lightness(), darkWindow.lightness());
        QCOMPARE(ThemeManager::density(), ThemeManager::Density::Comfortable);
        QCOMPARE(ThemeManager::themeMode(), ThemeManager::ThemeMode::Dark);

        QTableWidget table(1, 1);
        table.show();
        ThemeManager::applyDensity(qApp, ThemeManager::Density::Compact);
        const int compactRow = table.verticalHeader()->defaultSectionSize();
        ThemeManager::applyDensity(qApp, ThemeManager::Density::Comfortable);
        const int comfortableRow = table.verticalHeader()->defaultSectionSize();
        QVERIFY(comfortableRow > compactRow);
    }

    void notificationBannerAggregatesAndCloses()
    {
        NotificationBanner banner;
        banner.show();
        QSignalSpy detailsSpy(&banner, &NotificationBanner::detailsRequested);
        banner.showNotification(QStringLiteral("warning"), QStringLiteral("下载"),
                                QStringLiteral("设备写入失败\n第二行"));
        QVERIFY(banner.isVisible());
        QCOMPARE(banner.repeatCount(), 1);
        QVERIFY(banner.currentText().contains(QStringLiteral("下载")));

        banner.showNotification(QStringLiteral("warning"), QStringLiteral("下载"),
                                QStringLiteral("设备写入失败\n第二行"));
        QCOMPARE(banner.repeatCount(), 2);
        QVERIFY(banner.currentText().contains(QStringLiteral("×2")));

        auto* details = banner.findChild<QToolButton*>(QStringLiteral("NotificationDetailsButton"));
        QVERIFY(details);
        QTest::mouseClick(details, Qt::LeftButton);
        QCOMPARE(detailsSpy.count(), 1);

        auto* close = banner.findChild<QToolButton*>(QStringLiteral("NotificationCloseButton"));
        QVERIFY(close);
        QTest::mouseClick(close, Qt::LeftButton);
        QVERIFY(!banner.isVisible());
        QVERIFY(banner.currentText().isEmpty());
    }

    void layoutResetPreservesPreferences()
    {
        const QString org = QStringLiteral("LHThemeTest");
        const QString app = QStringLiteral("LayoutReset-%1").arg(QCoreApplication::applicationPid());
        SettingsController controller;
        controller.setSettingsStorage(org, app);
        controller.setThemeMode(1);
        controller.setDensity(1);
        controller.setFontSizeIndex(2);
        controller.setDefaultProjectDir(QStringLiteral("D:/preferred"));
        controller.setLayoutVersion(SettingsController::CURRENT_LAYOUT_VERSION);
        controller.setSystemViewFloating(QStringLiteral("monitor"), true);
        controller.setSystemViewFloatGeometry(QStringLiteral("monitor"), QByteArrayLiteral("geometry"));
        controller.setWorkspaceLayoutState(QStringLiteral("programming"), QByteArrayLiteral("state"));
        controller.saveSettings();

        controller.clearWorkspaceLayouts();
        QVERIFY(!controller.systemViewFloating(QStringLiteral("monitor")));
        QVERIFY(controller.systemViewFloatGeometry(QStringLiteral("monitor")).isEmpty());
        QCOMPARE(controller.themeMode(), 1);
        QCOMPARE(controller.density(), 1);
        QCOMPARE(controller.fontSizeIndex(), 2);
        QCOMPARE(controller.defaultProjectDir(), QStringLiteral("D:/preferred"));
        QCOMPARE(controller.layoutVersion(), 0);
        QVERIFY(controller.workspaceLayoutState(QStringLiteral("programming")).isEmpty());
        controller.saveSettings();

        SettingsController reloaded;
        reloaded.setSettingsStorage(org, app);
        reloaded.loadSettings();
        QVERIFY(!reloaded.systemViewFloating(QStringLiteral("monitor")));
        QVERIFY(reloaded.systemViewFloatGeometry(QStringLiteral("monitor")).isEmpty());
        QCOMPARE(reloaded.themeMode(), 1);
        QCOMPARE(reloaded.density(), 1);
        QCOMPARE(reloaded.fontSizeIndex(), 2);
        QCOMPARE(reloaded.defaultProjectDir(), QStringLiteral("D:/preferred"));
        QVERIFY(reloaded.workspaceLayoutState(QStringLiteral("programming")).isEmpty());
        QFile::remove(QSettings(QSettings::defaultFormat(), QSettings::UserScope, org, app).fileName());
    }

    void menuBarHasReadableThemeContrast_data()
    {
        QTest::addColumn<int>("theme");
        QTest::addColumn<int>("density");
        for (int theme = 0; theme < 2; ++theme) {
            for (int density = 0; density < 2; ++density) {
                const QByteArray name = QByteArray::number(theme) + '-' + QByteArray::number(density);
                QTest::newRow(name.constData()) << theme << density;
            }
        }
    }

    void menuBarHasReadableThemeContrast()
    {
        QFETCH(int, theme);
        QFETCH(int, density);
        ThemeManager::applyTheme(qApp, static_cast<ThemeManager::ThemeMode>(theme));
        ThemeManager::applyDensity(qApp, static_cast<ThemeManager::Density>(density));
        QMainWindow window;
        window.resize(640, 240);
        QMenuBar* bar = window.menuBar();
        bar->addMenu(QStringLiteral("文件(&F)"));
        bar->addMenu(QStringLiteral("工具(&T)"));
        window.show();
        QTest::qWait(50);
        const QImage rendered = bar->grab().toImage();
        QVERIFY(!rendered.isNull());
        const QColor background = rendered.pixelColor(rendered.width() - 2, rendered.height() / 2);
        const QColor foreground = bar->palette().color(QPalette::WindowText);
        const double contrast = ThemeManager::contrastRatio(foreground, background);
        qInfo() << "menu foreground" << foreground.name() << "rendered background"
                << background.name() << "contrast" << contrast;
        QVERIFY2(contrast >= 4.5, qPrintable(QStringLiteral("Menu contrast %1 on %2 = %3")
                                          .arg(foreground.name(), background.name()).arg(contrast)));
        if (theme == 1) {
            QVERIFY(background.lightness() < 100);
        }
    }

    void textContrastMeetsMinimum()
    {
        QVERIFY(ThemeManager::contrastRatio(QColor(QStringLiteral("#1f1f1f")),
                                            QColor(QStringLiteral("#ffffff"))) >= 4.5);
        QVERIFY(ThemeManager::contrastRatio(QColor(QStringLiteral("#e6edf3")),
                                            QColor(QStringLiteral("#171c22"))) >= 4.5);
        QVERIFY(ThemeManager::contrastRatio(QColor(QStringLiteral("#b7c4d2")),
                                            QColor(QStringLiteral("#20252b"))) >= 4.5);
        // Decorative group-box boundary; Qt native control borders are not themed.
        const double boundary = ThemeManager::contrastRatio(QColor(QStringLiteral("#d0d7de")),
                                                             QColor(QStringLiteral("#ffffff")));
        QVERIFY(boundary > 1.3);
        qInfo() << "contrast light-text/white="
                << ThemeManager::contrastRatio(QColor(QStringLiteral("#1f1f1f")), QColor(QStringLiteral("#ffffff")))
                << "dark-text/base="
                << ThemeManager::contrastRatio(QColor(QStringLiteral("#e6edf3")), QColor(QStringLiteral("#171c22")))
                << "groupbox-boundary/white=" << boundary;
    }
};

QTEST_MAIN(ThemeDensityTest)
#include "theme_density_test.moc"
