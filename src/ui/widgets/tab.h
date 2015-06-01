#ifndef TAB_H
#define TAB_H

#include <QWidget>

class QUrl;

namespace Ui {
class Tab;
}

namespace Zeal {

class ListModel;
class SearchModel;
class SearchQuery;

namespace WidgetUi {

class Tab : public QWidget
{
    Q_OBJECT
public:
    explicit Tab(QWidget *parent = nullptr);
    explicit Tab(const SearchQuery &searchQuery, QWidget *parent = nullptr);
    ~Tab() override;

    void setSearchQuery(const SearchQuery &searchQuery);

    QIcon icon() const;
    QString title() const;

signals:
    void iconChanged(const QIcon &icon);
    void titleChanged(const QString &title);

protected:
    void keyPressEvent(QKeyEvent *keyEvent) override;

private slots:
    void openDocset(const QModelIndex &index);

private:
    static QString docsetName(const QUrl &url);

    Ui::Tab *ui = nullptr;
    ListModel *m_docsetModel = nullptr;
    SearchModel *m_searchModel = nullptr;
    SearchModel *m_tocModel = nullptr;
};


} // namespace WidgetUi
} // namespace Zeal

#endif // TAB_H
