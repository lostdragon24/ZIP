#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCompleter>
#include <QStandardItemModel>
#include <QTreeWidgetItem>
#include <QMenu>

// Добавляем forward declaration
class QComboBox;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class Database;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onAddItem();
    void onUpdateItem();
    void onDeleteItem();
    void onCancelEdit();
    void onEditItem();
    void onSearchTextChanged(const QString &text);
    void onMaterialTypeChanged(const QString &text);
    void onManufacturerChanged(const QString &text);
    void onGenerateReport();
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onTableSelectionChanged();
    void updateInterfaceVisibility();

    void onSortByDateDesc();
    void onSortByDateAsc();
    void onSortByType();
    void onSortByManufacturer();
    void onSortByModel();
    void onSortBySerial();

    // Новые слоты для контекстного меню дерева
    void onTreeCustomContextMenu(const QPoint &pos);
    void onDeleteFromTree();
    void onRefreshTree();

    // Слоты для списания
    void onWriteOffItem();
    void onReturnItem();
    void onShowWriteOffHistory();
    void onStatusFilterChanged();

private:
    Ui::MainWindow *ui;
    Database *db;
    QCompleter *materialCompleter;
    QCompleter *manufacturerCompleter;
    QCompleter *modelCompleter;
    QStandardItemModel *materialModel;
    QStandardItemModel *manufacturerModel;
    QStandardItemModel *modelModel;

    QMenu *sortMenu;
    QAction *sortByDateDescAction;
    QAction *sortByDateAscAction;
    QAction *sortByTypeAction;
    QAction *sortByManufacturerAction;
    QAction *sortByModelAction;
    QAction *sortBySerialAction;

    // Для контекстного меню
    QMenu *treeContextMenu;
    QAction *deleteAction;
    QAction *refreshAction;
    QTreeWidgetItem *contextMenuItem;

    // Для списания
    QMenu *inventoryContextMenu;
    QAction *writeOffAction;
    QAction *returnAction;
    QAction *showHistoryAction;
    QComboBox *statusFilterCombo;

    int currentEditId; // ID редактируемой записи

    void setupUI();
    void setupConnections();
    void loadMaterialsTree();
    void loadInventoryTable(const QList<QVariantMap> &items = QList<QVariantMap>());
    void refreshCompleters();
    void clearForm();
    void setEditMode(bool editMode);
    void loadItemForEdit(int itemId);
    void setupSortMenu();
    QString formatDateForDisplay(const QString &dbDate);

    // Вспомогательные методы для списания
    void setupContextMenu();
    void updateRowAppearance(int row, bool isWrittenOff);
    void showWriteOffDialog(int itemId);
    int findRowByItemId(int itemId);
    void exportWriteOffHistory(const QString &fileName);

    // Новые вспомогательные методы
    QString getItemTextWithoutEmoji(const QString &textWithEmoji);
    void deleteSelectedTreeItem();
};
#endif // MAINWINDOW_H
