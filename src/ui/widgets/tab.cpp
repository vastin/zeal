#include "tab.h"
#include "ui_tab.h"

#include "../searchitemdelegate.h"

#include "core/application.h"
#include "registry/docsetregistry.h"
#include "registry/listmodel.h"
#include "registry/searchquery.h"
#include "registry/searchmodel.h"

#include <QDesktopServices>
#include <QKeyEvent>
#include <QMessageBox>

#ifdef USE_WEBENGINE
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#else
#include <QWebFrame>
#include <QWebHistory>
#include <QWebPage>
#endif

using namespace Zeal;
using namespace Zeal::WidgetUi;

namespace {
const char startPageUrl[] = "qrc:///browser/start.html";
}

Tab::Tab(QWidget *parent) :
    Tab(SearchQuery(), parent)
{
}

Tab::Tab(const Zeal::SearchQuery &searchQuery, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Tab()),
    m_docsetModel(new ListModel(Core::Application::docsetRegistry(), this)),
    m_searchModel(new SearchModel(this)),
    m_tocModel(new SearchModel(this))
{
    ui->setupUi(this);

    // Search Edit
    ui->searchEdit->setTreeView(ui->treeView);
    QStringList completions;
    for (Docset *docset : Core::Application::docsetRegistry()->docsets())
        completions << docset->keyword() + QLatin1Char(':');
    ui->searchEdit->setCompletions(completions);
    setSearchQuery(searchQuery);
    ui->searchEdit->setFocus();
    connect(ui->searchEdit, &QLineEdit::textChanged, [this](const QString &text) {
        Core::Application::docsetRegistry()->search(text);
        if (text.isEmpty()) {
            m_tocModel->setResults();
            ui->treeView->setModel(m_docsetModel);
        }
    });

    // Has to be a queued connection
    connect(Core::Application::docsetRegistry(), &DocsetRegistry::queryCompleted, this, [this]() {
        m_searchModel->setResults(Core::Application::docsetRegistry()->queryResults());
    });

    // Search Model
    ui->treeView->setModel(m_docsetModel);
    ui->treeView->setColumnHidden(1, true);
    SearchItemDelegate *delegate = new SearchItemDelegate(ui->treeView);
    connect(ui->searchEdit, &QLineEdit::textChanged, [delegate](const QString &text) {
        delegate->setHighlight(Zeal::SearchQuery::fromString(text).query());
    });
    ui->treeView->setItemDelegate(delegate);

    connect(m_searchModel, &SearchModel::queryCompleted, [this]() {
        ui->treeView->setModel(m_searchModel);
        //ui->treeView->setColumnHidden(1, true);
        ui->treeView->setCurrentIndex(m_searchModel->index(0, 0, QModelIndex()));
        emit ui->treeView->activated(ui->treeView->currentIndex());
    });

    /*connect(ui->treeView, &QTreeView::clicked, [this](const QModelIndex &index) {
        ui->treeView->activated(index);
    });
    connect(ui->sections, &QListView::clicked, [this](const QModelIndex &index) {
        ui->sections->activated(index);
    });*/
    connect(ui->treeView, &QTreeView::activated, this, &Tab::openDocset);
    //connect(ui->forwardButton, &QPushButton::clicked, this, &MainWindow::forward);
    //connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::back);

    // TOC Model
    ui->sections->setModel(m_tocModel);
    ui->sections->hide();
    ui->seeAlsoLabel->hide();
    connect(m_tocModel, &SearchModel::queryCompleted, [this]() {
        const bool hasResults = m_tocModel->rowCount(QModelIndex());
        ui->sections->setVisible(hasResults);
        ui->seeAlsoLabel->setVisible(hasResults);
    });
    connect(ui->sections, &QListView::activated, this, &Tab::openDocset);

    // WebView
    connect(ui->webView, &SearchableWebView::urlChanged, [this](const QUrl &url) {
        Docset *docset = Core::Application::docsetRegistry()->docset(docsetName(url));
        if (docset)
            m_tocModel->setResults(docset->relatedLinks(url));

        emit iconChanged(icon());
    });

    connect(ui->webView, &SearchableWebView::titleChanged, this, &Tab::titleChanged);

    connect(ui->webView, &SearchableWebView::linkClicked, [this](const QUrl &url) {
        const QString message = tr("Do you want to open an external link?<br>URL: <b>%1</b>");
        int ret = QMessageBox::question(this, QStringLiteral("Zeal"), message.arg(url.toString()));
        if (ret == QMessageBox::Yes)
            QDesktopServices::openUrl(url);
    });

#ifdef USE_WEBENGINE
    ui->webView->page()->load(QUrl(startPageUrl));
#else
    //ui->webView->page()->setNetworkAccessManager(m_zealNetworkManager);
    ui->webView->page()->setLinkDelegationPolicy(QWebPage::DelegateExternalLinks);
    ui->webView->page()->mainFrame()->load(QUrl(startPageUrl));
#endif

#ifdef Q_OS_OSX
    ui->treeView->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->sections->setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
}

Tab::~Tab()
{
    delete ui;
}

void Tab::setSearchQuery(const SearchQuery &searchQuery)
{
    ui->searchEdit->setFocus();

    if (searchQuery.isEmpty())
        return;

    ui->searchEdit->setText(searchQuery.toString());
    ui->treeView->setFocus();
    ui->treeView->activated(ui->treeView->currentIndex());
}

QIcon Tab::icon() const
{
    const Docset * const docset = Core::Application::docsetRegistry()->docset(docsetName(ui->webView->url()));
    if (!docset)
        return QIcon(QStringLiteral(":/icons/logo/icon.png"));
    return docset->icon();
}

QString Tab::title() const
{
    return ui->webView->title();
}

void Tab::keyPressEvent(QKeyEvent *keyEvent)
{
    switch (keyEvent->key()) {
    case Qt::Key_Escape:
        ui->searchEdit->setFocus();
        ui->searchEdit->clearQuery();
        break;
    case Qt::Key_Question:
        ui->searchEdit->setFocus();
        ui->searchEdit->selectQuery();
        break;
    default:
        QWidget::keyPressEvent(keyEvent);
        break;
    }
}

void Tab::openDocset(const QModelIndex &index)
{
    const QVariant urlStr = index.sibling(index.row(), 1).data();
    if (urlStr.isNull())
        return;

    /// TODO: Keep anchor separately from file address
    QStringList urlParts = urlStr.toString().split(QLatin1Char('#'));
    QUrl url = QUrl::fromLocalFile(urlParts[0]);
    if (urlParts.count() > 1)
        /// NOTE: QUrl::DecodedMode is a fix for #121. Let's hope it doesn't break anything.
        url.setFragment(urlParts[1], QUrl::DecodedMode);

    ui->webView->load(url);
    //ui->webView->focus();
}

QString Tab::docsetName(const QUrl &url)
{
    const QRegExp docsetRegex(QStringLiteral("/([^/]+)[.]docset"));
    return docsetRegex.indexIn(url.path()) != -1 ? docsetRegex.cap(1) : QString();
}
