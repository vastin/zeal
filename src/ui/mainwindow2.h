#ifndef MAINWINDOW2_H
#define MAINWINDOW2_H

#include <QMainWindow>

class QSystemTrayIcon;

#ifdef USE_APPINDICATOR
struct _AppIndicator;
struct _GtkWidget;
#endif

class QxtGlobalShortcut;

namespace Ui {
class MainWindow2;
}

namespace Zeal {

class SearchQuery;
class ListModel;
class SettingsDialog;

namespace Core {
class Application;
class Settings;
}

namespace WidgetUi {

class Tab;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(Zeal::Core::Application *app, QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void bringToFront(const Zeal::SearchQuery &searchQuery);

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void applySettings();
    void toggleWindow();

private:
    void setupActions();
    void createTrayIcon();
    void removeTrayIcon();

    // Tab management
    Tab *addTab();
    inline Tab *currentTab() const;

    Ui::MainWindow2 *ui = nullptr;

    Zeal::Core::Application *m_application = nullptr;
    Zeal::Core::Settings *m_settings = nullptr;
    Zeal::ListModel *m_zealListModel = nullptr;
    Zeal::SettingsDialog *m_settingsDialog = nullptr;

    QxtGlobalShortcut *m_globalShortcut = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;

#ifdef USE_APPINDICATOR
    _AppIndicator *m_appIndicator = nullptr;
    _GtkWidget *m_appIndicatorMenu = nullptr;
    _GtkWidget *m_appIndicatorQuitMenuItem = nullptr;
    _GtkWidget *m_appIndicatorShowHideMenuItem = nullptr;
    _GtkWidget *m_appIndicatorMenuSeparator = nullptr;
#endif
};

} // namespace WidgetUi
} // namespace Zeal

#endif // MAINWINDOW2_H
