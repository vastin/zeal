#include "mainwindow2.h"
#include "ui_mainwindow2.h"

#include "aboutdialog.h"
#include "settingsdialog.h"
#include "widgets/tab.h"
#include "3rdparty/qxtglobalshortcut/qxtglobalshortcut.h"

#include "core/application.h"
#include "core/settings.h"
#include "registry/docsetregistry.h"
#include "registry/listmodel.h"
#include "registry/searchmodel.h"
#include "registry/searchquery.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QMessageBox>
#include <QSystemTrayIcon>

/// TODO: [Qt 5.5] Remove in favour of native Qt support (QTBUG-31762)
#ifdef USE_APPINDICATOR
#undef signals
#include <libappindicator/app-indicator.h>
#define signals public
#include <gtk/gtk.h>
#endif

using namespace Zeal;
using namespace Zeal::WidgetUi;

MainWindow::MainWindow(Core::Application *app, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow2()),
    m_application(app),
    m_settings(app->settings()),
    m_zealListModel(new ListModel(app->docsetRegistry(), this)),
    m_settingsDialog(new SettingsDialog(app, m_zealListModel, this)),
    m_globalShortcut(new QxtGlobalShortcut(m_settings->showShortcut, this))
{
    ui->setupUi(this);
    setWindowIcon(QIcon::fromTheme(QStringLiteral("zeal"), QIcon(QStringLiteral(":/zeal.ico"))));

    if (m_settings->showSystrayIcon)
        createTrayIcon();

    connect(m_settings, &Core::Settings::updated, this, &MainWindow::applySettings);
    connect(m_globalShortcut, &QxtGlobalShortcut::activated, this, &MainWindow::toggleWindow);

    setupActions();

    addTab();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::bringToFront(const SearchQuery &searchQuery)
{
    show();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise();
    activateWindow();

    currentTab()->setSearchQuery(searchQuery);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (m_settings->showSystrayIcon && m_settings->minimizeToSystray
            && event->type() == QEvent::WindowStateChange && isMinimized()) {
        hide();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_settings->windowGeometry = saveGeometry();
    if (m_settings->showSystrayIcon && m_settings->hideOnClose) {
        event->ignore();
        toggleWindow();
    }
}

void MainWindow::applySettings()
{
    m_globalShortcut->setShortcut(m_settings->showShortcut);

    if (m_settings->showSystrayIcon)
        createTrayIcon();
    else
        removeTrayIcon();
}

void MainWindow::toggleWindow()
{
    const bool checkActive = sender() == m_globalShortcut;

    if (!isVisible() || (checkActive && !isActiveWindow())) {
#ifdef USE_APPINDICATOR
        if (m_appIndicator) {
            gtk_menu_item_set_label(GTK_MENU_ITEM(m_appIndicatorShowHideMenuItem),
                                    qPrintable(tr("Hide")));
        }
#endif
        bringToFront(SearchQuery());
    } else {
#ifdef USE_APPINDICATOR
        if (m_trayIcon || m_appIndicator) {
            if (m_appIndicator) {
                gtk_menu_item_set_label(GTK_MENU_ITEM(m_appIndicatorShowHideMenuItem),
                                        qPrintable(tr("Show")));
            }
#else
        if (m_trayIcon) {
#endif
            hide();
        } else {
            showMinimized();
        }
    }
}

void MainWindow::setupActions()
{
    // Setup actions
    if (QKeySequence(QKeySequence::Quit) != QKeySequence(QStringLiteral("Ctrl+Q"))) {
        ui->actionQuit->setShortcuts(QList<QKeySequence>{QKeySequence(QStringLiteral("Ctrl+Q")),
                                                         QKeySequence::Quit});
    } else {
        // Quit == Ctrl+Q - don't set the same sequence twice because it causes
        // "QAction::eventFilter: Ambiguous shortcut overload: Ctrl+Q"
        ui->actionQuit->setShortcuts(QList<QKeySequence>{QKeySequence::Quit});
    }
    connect(ui->actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);

    connect(ui->actionOptions, &QAction::triggered, [=]() {
        m_globalShortcut->setEnabled(false);
        m_settingsDialog->exec();
        m_globalShortcut->setEnabled(true);
    });

    // Help Menu
    connect(ui->actionReportProblem, &QAction::triggered, [this]() {
        QDesktopServices::openUrl(QStringLiteral("https://github.com/zealdocs/zeal/issues"));
    });
    connect(ui->actionCheckForUpdate, &QAction::triggered,
            m_application, &Core::Application::checkForUpdate);
    connect(ui->actionAboutZeal, &QAction::triggered, [this]() {
        QScopedPointer<AboutDialog> dialog(new AboutDialog(this));
        dialog->exec();
    });
    connect(ui->actionAboutQt, &QAction::triggered, [this]() {
        QMessageBox::aboutQt(this);
    });

    // Update check
    connect(m_application, &Core::Application::updateCheckError, [this](const QString &message) {
        QMessageBox::warning(this, QStringLiteral("Zeal"), message);
    });

    connect(m_application, &Core::Application::updateCheckDone, [this](const QString &version) {
        if (version.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Zeal"), tr("You are using the latest Zeal version."));
            return;
        }

        const int ret = QMessageBox::information(this, QStringLiteral("Zeal"),
                                                 QString(tr("A new version <b>%1</b> is available. Open download page?")).arg(version),
                                                 QMessageBox::Yes, QMessageBox::No);
        if (ret == QMessageBox::Yes)
            QDesktopServices::openUrl(QStringLiteral("http://zealdocs.org/download.html"));
    });
}

#ifdef USE_APPINDICATOR
void appIndicatorToggleWindow(GtkMenu *menu, gpointer data)
{
    Q_UNUSED(menu);
    static_cast<MainWindow *>(data)->toggleWindow();
}
#endif

void MainWindow::createTrayIcon()
{
#ifdef USE_APPINDICATOR
    if (m_trayIcon || m_appIndicator)
        return;
#else
    if (m_trayIcon)
        return;
#endif

#ifdef USE_APPINDICATOR
    const QString desktop = getenv("XDG_CURRENT_DESKTOP");
    const bool isUnity = (desktop.toLower() == QLatin1String("unity"));

    if (isUnity) { // Application Indicators for Unity
        m_appIndicatorMenu = gtk_menu_new();

        m_appIndicatorShowHideMenuItem = gtk_menu_item_new_with_label(qPrintable(tr("Hide")));
        gtk_menu_shell_append(GTK_MENU_SHELL(m_appIndicatorMenu), m_appIndicatorShowHideMenuItem);
        g_signal_connect(m_appIndicatorShowHideMenuItem, "activate",
                         G_CALLBACK(appIndicatorToggleWindow), this);

        m_appIndicatorMenuSeparator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(m_appIndicatorMenu), m_appIndicatorMenuSeparator);

        m_appIndicatorQuitMenuItem = gtk_menu_item_new_with_label(qPrintable(tr("Quit")));
        gtk_menu_shell_append(GTK_MENU_SHELL(m_appIndicatorMenu), m_appIndicatorQuitMenuItem);
        g_signal_connect(m_appIndicatorQuitMenuItem, "activate",
                         G_CALLBACK(QCoreApplication::quit), NULL);

        gtk_widget_show_all(m_appIndicatorMenu);

        /// NOTE: Zeal icon has to be installed, otherwise app indicator won't be shown
        m_appIndicator = app_indicator_new("zeal", "zeal", APP_INDICATOR_CATEGORY_OTHER);

        app_indicator_set_status(m_appIndicator, APP_INDICATOR_STATUS_ACTIVE);
        app_indicator_set_menu(m_appIndicator, GTK_MENU(m_appIndicatorMenu));
    } else {  // others
#endif
        m_trayIcon = new QSystemTrayIcon(this);
        m_trayIcon->setIcon(windowIcon());
        m_trayIcon->setToolTip(QStringLiteral("Zeal"));

        connect(m_trayIcon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick)
                return;

            // Disable, when settings window is open
            if (m_settingsDialog->isVisible()) {
                m_settingsDialog->activateWindow();
                return;
            }

            toggleWindow();
        });

        QMenu *trayIconMenu = new QMenu(this);
        QAction *quitAction = trayIconMenu->addAction(tr("&Quit"));
        connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

        m_trayIcon->setContextMenu(trayIconMenu);

        m_trayIcon->show();
#ifdef USE_APPINDICATOR
    }
#endif
}

void MainWindow::removeTrayIcon()
{
#ifdef USE_APPINDICATOR
    if (!m_trayIcon && !m_appIndicator)
        return;
#else
    if (!m_trayIcon)
        return;
#endif

#ifdef USE_APPINDICATOR
    const QString desktop = getenv("XDG_CURRENT_DESKTOP");
    const bool isUnity = (desktop.toLower() == QLatin1String("unity"));

    if (isUnity) {
        g_clear_object(&m_appIndicator);
        g_clear_object(&m_appIndicatorMenu);
        g_clear_object(&m_appIndicatorShowHideMenuItem);
        g_clear_object(&m_appIndicatorMenuSeparator);
        g_clear_object(&m_appIndicatorQuitMenuItem);
    } else {
#endif
        QMenu *trayIconMenu = m_trayIcon->contextMenu();
        delete m_trayIcon;
        m_trayIcon = nullptr;
        delete trayIconMenu;
#ifdef USE_APPINDICATOR
    }
#endif
}

Tab *MainWindow::addTab()
{
    Tab *tab = new Tab(this);
    ui->tabWidget->addTab(tab, tab->title());

    connect(tab, &Tab::iconChanged, [this, tab](const QIcon &icon) {
        const int index = ui->tabWidget->indexOf(tab);
        ui->tabWidget->setTabIcon(index, icon);
    });

    connect(tab, &Tab::titleChanged, [this, tab](const QString &title) {
        const int index = ui->tabWidget->indexOf(tab);
        ui->tabWidget->setTabText(index, title);
    });

    return tab;
}

Tab *MainWindow::currentTab() const
{
    return qobject_cast<Tab *>(ui->tabWidget->currentWidget());
}
