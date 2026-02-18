#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "database.h"
#include <QMessageBox>
#include <QDate>
#include <QFileDialog>
#include <QTextStream>
#include <QTreeWidgetItem>
#include <QStandardItemModel>
#include <QCompleter>
#include <QHeaderView>
#include <QStyle>
#include <QTableWidgetSelectionRange>
#include <QInputDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QTimer>

#include "labelprintdialog.h"
#include "advancedfilterdialog.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , db(new Database(this))
    , contextMenuItem(nullptr)
    , currentEditId(-1)
{
    ui->setupUi(this);


    QString styleSheet = R"(
        QComboBox {
            min-width: 200px;
            max-width: 300px;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox::down-arrow {
            image: none;
            border: none;
        }
    )";


    if (!db->initDatabase()) {
            QMessageBox::critical(this, "Ошибка", "Не удалось инициализировать базу данных");
            return;
        }

        setupUI();
        setupConnections();
        setupSortMenu();
        refreshCompleters();
        loadMaterialsTree();
        loadInventoryTable();

        ui->arrivalDateEdit->setDate(QDate::currentDate());
        updateInterfaceVisibility();

        connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);

        // НЕ СОЗДАЕМ НОВЫЙ виджет, а используем существующий из UI
            dashboardWidget = ui->dashboardWidget;  // Просто присваиваем указатель

            // Устанавливаем database для существующего виджета
            dashboardWidget->setDatabase(db);

            // Убедимся, что виджет видим
            dashboardWidget->setVisible(true);

            // Подключаем обновление статистики при переключении на вкладку
            connect(ui->tabWidget, &QTabWidget::currentChanged, [this](int index) {
                if (index == 2) { // Индекс вкладки статистики
                    qDebug() << "Stats tab activated, refreshing...";
                    dashboardWidget->refreshStats();
                }
            });

    }

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    // Настройка контекстного меню для дерева
    treeContextMenu = new QMenu(this);
    deleteAction = new QAction("🗑️ Удалить выбранное", this);
    refreshAction = new QAction("🔄 Обновить дерево", this);

    treeContextMenu->addAction(deleteAction);
    treeContextMenu->addSeparator();
    treeContextMenu->addAction(refreshAction);

    // Настройка таблицы инвентаря
    QStringList headers = {"ID", "Статус", "Тип", "Производитель", "Модель", "Part Number",
                              "Серийный номер", "Объем", "Интерфейс", "Дата прихода", "Накладная"};
        ui->inventoryTable->setColumnCount(headers.size());
        ui->inventoryTable->setHorizontalHeaderLabels(headers);

        // Скрываем колонки ID и статус (будем использовать визуальные обозначения)
        ui->inventoryTable->setColumnHidden(0, true);  // ID
        ui->inventoryTable->setColumnHidden(1, true);  // Статус

    // Настраиваем режимы отображения
    ui->inventoryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->inventoryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->inventoryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Включаем сортировку по клику на заголовки
    ui->inventoryTable->setSortingEnabled(true);

    // Настраиваем ширину колонок
    ui->inventoryTable->horizontalHeader()->setStretchLastSection(true);
    ui->inventoryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    // Скрываем колонку ID
    ui->inventoryTable->setColumnHidden(0, true);

    // Устанавливаем минимальные ширины для важных колонок
    ui->inventoryTable->setColumnWidth(1, 120);  // Тип
    ui->inventoryTable->setColumnWidth(2, 120);  // Производитель
    ui->inventoryTable->setColumnWidth(5, 150);  // Серийный номер
    ui->inventoryTable->setColumnWidth(8, 100);  // Дата прихода

    // Автодополнение
    materialCompleter = new QCompleter(this);
    materialModel = new QStandardItemModel(this);
    materialCompleter->setModel(materialModel);
    materialCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    materialCompleter->setCompletionMode(QCompleter::PopupCompletion);
    ui->materialTypeCombo->setCompleter(materialCompleter);
    ui->materialTypeCombo->setInsertPolicy(QComboBox::InsertAtTop);

    manufacturerCompleter = new QCompleter(this);
    manufacturerModel = new QStandardItemModel(this);
    manufacturerCompleter->setModel(manufacturerModel);
    manufacturerCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    manufacturerCompleter->setCompletionMode(QCompleter::PopupCompletion);
    ui->manufacturerCombo->setCompleter(manufacturerCompleter);
    ui->manufacturerCombo->setInsertPolicy(QComboBox::InsertAtTop);

    modelCompleter = new QCompleter(this);
    modelModel = new QStandardItemModel(this);
    modelCompleter->setModel(modelModel);
    modelCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    modelCompleter->setCompletionMode(QCompleter::PopupCompletion);
    ui->modelCombo->setCompleter(modelCompleter);
    ui->modelCombo->setInsertPolicy(QComboBox::InsertAtTop);


    // Создаем сплиттер
        QSplitter *splitter = new QSplitter(Qt::Horizontal, ui->centralwidget);
        splitter->addWidget(ui->leftFrame);
        splitter->addWidget(ui->tabWidget);

        // Устанавливаем начальное процентное соотношение
        QTimer::singleShot(100, [splitter, this]() {
            int totalWidth = ui->centralwidget->width();
            if (totalWidth > 0) {
                QList<int> sizes;
                sizes << totalWidth * 25 / 100;  // 25%
                sizes << totalWidth * 75 / 100;  // 75%
                splitter->setSizes(sizes);
            }
        });

        // Сохраняем возможность пользователя изменять размеры
        splitter->setChildrenCollapsible(false);

        // Удаляем старый layout и добавляем новый со сплиттером
        QLayout *oldLayout = ui->centralwidget->layout();
        if (oldLayout) {
            delete oldLayout;
        }

        QHBoxLayout *newLayout = new QHBoxLayout(ui->centralwidget);
        newLayout->addWidget(splitter);

        // Устанавливаем ограничения для leftFrame
        ui->leftFrame->setMinimumWidth(200);
        // Не ставим maximumWidth, чтобы пользователь мог расширять

    // Интерфейсы
    QStringList interfaces = {"", "SATA I", "SATA II", "SATA III", "IDE", "SAS",
                             "NVMe PCIe 3.0", "NVMe PCIe 4.0", "NVMe PCIe 5.0",
                             "USB 2.0", "USB 3.0", "USB 3.1", "USB 3.2",
                             "USB-C", "Thunderbolt 3", "Thunderbolt 4",
                             "SCSI", "M.2 SATA", "M.2 PCIe", "U.2", "FC"};
    ui->interfaceCombo->addItems(interfaces);
    ui->interfaceCombo->setCurrentIndex(0);

    // Скрываем поля для HDD/SSD по умолчанию
    ui->capacityLabel->setVisible(false);
    ui->capacityLineEdit->setVisible(false);
    ui->interfaceLabel->setVisible(false);
    ui->interfaceCombo->setVisible(false);

    // Настройка дерева
    ui->materialsTree->setHeaderHidden(true);
    ui->leftFrame->setMaximumWidth(550);

    // DateEdit
    ui->arrivalDateEdit->setDisplayFormat("dd.MM.yyyy");

    // Поле примечания
    ui->notesTextEdit->setMaximumHeight(100);

    // Начальный режим - добавление
    setEditMode(false);

    // Изменение имен вкладок программно
    ui->tabWidget->setTabText(0, "Добавление");
    ui->tabWidget->setTabText(1, "Просмотр");

    // Настройка контекстного меню для дерева
    treeContextMenu = new QMenu(this);
    deleteAction = new QAction("🗑️ Удалить выбранное", this);
    refreshAction = new QAction("🔄 Обновить дерево", this);

    treeContextMenu->addAction(deleteAction);
    treeContextMenu->addSeparator();
    treeContextMenu->addAction(refreshAction);

      // Добавляем колонку статуса в таблицу

        ui->inventoryTable->setColumnCount(headers.size());
        ui->inventoryTable->setHorizontalHeaderLabels(headers);

        // Скрываем колонки ID и статус (будем использовать иконки)
        ui->inventoryTable->setColumnHidden(0, true);  // ID
        ui->inventoryTable->setColumnHidden(1, true);  // Статус

        // Настраиваем контекстное меню для таблицы
        setupContextMenu();



    // Связываем кнопки
    connect(ui->refreshTreeButton, &QPushButton::clicked, this, &MainWindow::onRefreshTree);
    connect(ui->deleteFromTreeButton, &QPushButton::clicked, this, &MainWindow::onDeleteFromTree);

    connect(ui->advancedFilterButton, &QPushButton::clicked, this, &MainWindow::onAdvancedFilter);

    // Автоматическое включение/выключение кнопки удаления
    connect(ui->materialsTree, &QTreeWidget::itemSelectionChanged, [this]() {
        QTreeWidgetItem* item = ui->materialsTree->currentItem();
        bool enableDelete = item && item != ui->materialsTree->topLevelItem(0);
        ui->deleteFromTreeButton->setEnabled(enableDelete);
    });

    connect(ui->printLabelsButton, &QPushButton::clicked, this, &MainWindow::onPrintLabels);

    // Добавляем фильтр статуса
    QHBoxLayout *filterLayout = new QHBoxLayout();
        filterLayout->addWidget(new QLabel("Фильтр по статусу:"));

        statusFilterCombo = new QComboBox(this);
        statusFilterCombo->addItem("Все позиции", "all");
        statusFilterCombo->addItem("✅ В наличии", "available");
        statusFilterCombo->addItem("❌ Списано", "written_off");
        statusFilterCombo->setMaximumWidth(200);

        filterLayout->addWidget(statusFilterCombo);
        filterLayout->addStretch();

        // Добавляем фильтр перед таблицей во вкладке просмотра
        QVBoxLayout *viewLayout = qobject_cast<QVBoxLayout*>(ui->viewTab->layout());
        if (viewLayout) {
            // Вставляем фильтр перед таблицей (индекс 1, так как кнопки на 0)
            viewLayout->insertLayout(1, filterLayout);
        }

        connect(statusFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MainWindow::onStatusFilterChanged);
}

void MainWindow::onStatusFilterChanged()
{
    QString filter = statusFilterCombo->currentData().toString();
    qDebug() << "Filter changed to:" << filter;

    if (filter == "all") {
        loadInventoryTable();
    } else {
        // Загружаем все данные из БД
        QList<QVariantMap> allItems = db->getInventoryItems();
        QList<QVariantMap> filteredItems;

        for (const QVariantMap &item : allItems) {
            QString status = item["status"].toString();
            qDebug() << "Checking item ID:" << item["id"].toInt()
                     << "Status:" << status << "Filter:" << filter;

            if (status == filter) {
                filteredItems.append(item);
            }
        }

        qDebug() << "Filtered items count:" << filteredItems.size();
        loadInventoryTable(filteredItems);
    }
}


void MainWindow::setupContextMenu()
{
    inventoryContextMenu = new QMenu(this);

    writeOffAction = new QAction("❌ Списать (выдать)", this);
    returnAction = new QAction("✅ Вернуть в наличие", this);
    showHistoryAction = new QAction("📋 История списаний", this);

    inventoryContextMenu->addAction(writeOffAction);
    inventoryContextMenu->addAction(returnAction);
    inventoryContextMenu->addSeparator();
    inventoryContextMenu->addAction(showHistoryAction);

    // Подключаем слоты
    connect(writeOffAction, &QAction::triggered, this, &MainWindow::onWriteOffItem);
    connect(returnAction, &QAction::triggered, this, &MainWindow::onReturnItem);
    connect(showHistoryAction, &QAction::triggered, this, &MainWindow::onShowWriteOffHistory);

    // Устанавливаем контекстное меню для таблицы
    ui->inventoryTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->inventoryTable, &QTableWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
                QTableWidgetItem *item = ui->inventoryTable->itemAt(pos);
                if (item) {
                    int row = item->row();
                    int itemId = ui->inventoryTable->item(row, 0)->text().toInt();
                    bool isWrittenOff = db->isItemWrittenOff(itemId);

                    // Настраиваем доступность действий
                    writeOffAction->setEnabled(!isWrittenOff);
                    returnAction->setEnabled(isWrittenOff);

                    inventoryContextMenu->exec(ui->inventoryTable->viewport()->mapToGlobal(pos));
                }
            });
}

void MainWindow::updateRowAppearance(int row, bool isWrittenOff)
{
    if (row < 0 || row >= ui->inventoryTable->rowCount()) {
        return;
    }

    for (int col = 0; col < ui->inventoryTable->columnCount(); ++col) {
        QTableWidgetItem *item = ui->inventoryTable->item(row, col);
        if (item) {
            if (isWrittenOff) {
                // Красный перечеркнутый текст для списанных
                QFont font = item->font();
                font.setStrikeOut(true);
                item->setFont(font);
                item->setForeground(QBrush(QColor(255, 100, 100))); // Красный

                // Добавляем эмодзи красного крестика в колонку типа материала
                if (col == 2) { // Колонка "Тип" (после скрытых ID и статуса)
                    QString currentText = item->text();
                    if (!currentText.startsWith("❌ ")) {
                        item->setText("❌ " + currentText);
                    }
                }
            } else {
                // Возвращаем обычный вид
                QFont font = item->font();
                font.setStrikeOut(false);
                item->setFont(font);
                item->setForeground(QBrush(QColor(0, 0, 0))); // Черный

                // Убираем эмодзи красного крестика
                if (col == 2) {
                    QString currentText = item->text();
                    if (currentText.startsWith("❌ ")) {
                        item->setText(currentText.mid(3)); // Убираем "❌ "
                    }
                }
            }
        }
    }
}



void MainWindow::setupConnections()
{
    connect(ui->addButton, &QPushButton::clicked, this, &MainWindow::onAddItem);
    connect(ui->updateButton, &QPushButton::clicked, this, &MainWindow::onUpdateItem);
    connect(ui->cancelButton, &QPushButton::clicked, this, &MainWindow::onCancelEdit);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::clearForm);
    connect(ui->editButton, &QPushButton::clicked, this, &MainWindow::onEditItem);
    connect(ui->deleteButton, &QPushButton::clicked, this, &MainWindow::onDeleteItem);
    connect(ui->generateReportButton, &QPushButton::clicked, this, &MainWindow::onGenerateReport);
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(ui->materialTypeCombo, &QComboBox::currentTextChanged, this, &MainWindow::onMaterialTypeChanged);
    connect(ui->manufacturerCombo, &QComboBox::currentTextChanged, this, &MainWindow::onManufacturerChanged);
    connect(ui->materialsTree, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);
    connect(ui->inventoryTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onTableSelectionChanged);



    // Контекстное меню дерева
    connect(ui->materialsTree, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::onTreeCustomContextMenu);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::onDeleteFromTree);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::onRefreshTree);
}

void MainWindow::setEditMode(bool editMode)
{
    currentEditId = editMode ? currentEditId : -1;

    ui->addButton->setEnabled(!editMode);
    ui->updateButton->setEnabled(editMode);
    ui->cancelButton->setEnabled(editMode);
    ui->editButton->setEnabled(false);
    ui->deleteButton->setEnabled(false);

    if (editMode) {
        ui->inputGroupBox->setTitle("Редактирование позиции");
        // Серийный номер теперь можно менять всегда
    } else {
        ui->inputGroupBox->setTitle("Добавление новой позиции");

    }

    if (dashboardWidget) {
        dashboardWidget->refreshStats();
    }


}

void MainWindow::updateInterfaceVisibility()
{
    QString materialType = ui->materialTypeCombo->currentText().trimmed();
    bool isStorage = (materialType == "Жесткий диск" || materialType == "SSD накопитель" ||
                     materialType == "Оперативная память");

    ui->capacityLabel->setVisible(isStorage);
    ui->capacityLineEdit->setVisible(isStorage);
    ui->interfaceLabel->setVisible(isStorage);
    ui->interfaceCombo->setVisible(isStorage);
}

void MainWindow::refreshCompleters()
{
    // Типы материалов
    QStringList materials = db->getMaterialTypes();
    materialModel->clear();
    for (const QString &material : materials) {
        materialModel->appendRow(new QStandardItem(material));
    }
    ui->materialTypeCombo->clear();
    ui->materialTypeCombo->addItems(materials);

    // Производители
    QStringList manufacturers = db->getManufacturers();
    manufacturerModel->clear();
    for (const QString &manufacturer : manufacturers) {
        manufacturerModel->appendRow(new QStandardItem(manufacturer));
    }
    ui->manufacturerCombo->clear();
    ui->manufacturerCombo->addItems(manufacturers);

    // Модели
    QString currentMaterial = ui->materialTypeCombo->currentText();
    QString currentManufacturer = ui->manufacturerCombo->currentText();
    if (!currentMaterial.isEmpty() && !currentManufacturer.isEmpty()) {
        QStringList models = db->getModelsByMaterialAndManufacturer(currentMaterial, currentManufacturer);
        modelModel->clear();
        for (const QString &model : models) {
            modelModel->appendRow(new QStandardItem(model));
        }
        ui->modelCombo->clear();
        ui->modelCombo->addItems(models);
    }
}

void MainWindow::loadMaterialsTree()
{
    ui->materialsTree->clear();

    QTreeWidgetItem *rootItem = new QTreeWidgetItem(ui->materialsTree);
    rootItem->setText(0, "📦 Все материалы");

    QStringList materials = db->getMaterialTypes();
    for (const QString &material : materials) {
        QTreeWidgetItem *materialItem = new QTreeWidgetItem(rootItem);
        materialItem->setText(0, "📁 " + material);

        QStringList manufacturers = db->getManufacturers();
        for (const QString &manufacturer : manufacturers) {
            QStringList models = db->getModelsByMaterialAndManufacturer(material, manufacturer);
            if (!models.isEmpty()) {
                QTreeWidgetItem *manufacturerItem = new QTreeWidgetItem(materialItem);
                manufacturerItem->setText(0, "🏭 " + manufacturer);

                for (const QString &model : models) {
                    QTreeWidgetItem *modelItem = new QTreeWidgetItem(manufacturerItem);
                    modelItem->setText(0, "📄 " + model);
                }
            }
        }
    }

    ui->materialsTree->expandAll();
}

void MainWindow::loadInventoryTable(const QList<QVariantMap> &items)
{
    // Временно отключаем сортировку
    ui->inventoryTable->setSortingEnabled(false);
    ui->inventoryTable->clearContents();

    QList<QVariantMap> inventoryItems = items.isEmpty() ? db->getInventoryItems() : items;

    ui->inventoryTable->setRowCount(inventoryItems.size());

    qDebug() << "Loading" << inventoryItems.size() << "items into table";

    for (int i = 0; i < inventoryItems.size(); ++i) {
        const QVariantMap &item = inventoryItems[i];

        // Статус
        QString status = item["status"].toString();
        if (status.isEmpty()) {
            status = "available";
        }

        qDebug() << "Row" << i << "ID:" << item["id"].toInt() << "Status:" << status;

        // Заполняем таблицу
        ui->inventoryTable->setItem(i, 0, new QTableWidgetItem(item["id"].toString()));
        ui->inventoryTable->setItem(i, 1, new QTableWidgetItem(status));

        // Для типа материала добавляем/убираем эмодзи в зависимости от статуса
        QString materialType = item["material_type"].toString();
        if (status == "written_off") {
            ui->inventoryTable->setItem(i, 2, new QTableWidgetItem("❌ " + materialType));
        } else {
            ui->inventoryTable->setItem(i, 2, new QTableWidgetItem(materialType));
        }

        ui->inventoryTable->setItem(i, 3, new QTableWidgetItem(item["manufacturer"].toString()));
        ui->inventoryTable->setItem(i, 4, new QTableWidgetItem(item["model"].toString()));
        ui->inventoryTable->setItem(i, 5, new QTableWidgetItem(item["part_number"].toString()));
        ui->inventoryTable->setItem(i, 6, new QTableWidgetItem(item["serial_number"].toString()));
        ui->inventoryTable->setItem(i, 7, new QTableWidgetItem(item["capacity"].toString()));
        ui->inventoryTable->setItem(i, 8, new QTableWidgetItem(item["interface_type"].toString()));

        // Дата
        QString dateStr = item["arrival_date"].toString();
        QTableWidgetItem *dateItem = new QTableWidgetItem(formatDateForDisplay(dateStr));
        dateItem->setData(Qt::UserRole, QDate::fromString(dateStr, "yyyy-MM-dd"));
        ui->inventoryTable->setItem(i, 9, dateItem);

        ui->inventoryTable->setItem(i, 10, new QTableWidgetItem(item["invoice_number"].toString()));

        // Обновляем внешний вид в зависимости от статуса
        bool isWrittenOff = (status == "written_off");
        updateRowAppearance(i, isWrittenOff);
    }

    // Включаем сортировку
    ui->inventoryTable->setSortingEnabled(true);

    // Устанавливаем сортировку по дате
    ui->inventoryTable->sortByColumn(9, Qt::DescendingOrder);
}


void MainWindow::onWriteOffItem()
{
    QList<QTableWidgetItem*> selectedItems = ui->inventoryTable->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Выберите запись для списания");
        return;
    }

    int row = ui->inventoryTable->currentRow();
    int itemId = ui->inventoryTable->item(row, 0)->text().toInt();

    showWriteOffDialog(itemId);
}

void MainWindow::showWriteOffDialog(int itemId)
{
    // Получаем информацию о позиции
    QVariantMap item = db->getInventoryItemById(itemId);
    if (item.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить информацию о позиции");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Списание позиции");
    dialog.setFixedSize(400, 300);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // Информация о позиции
    QGroupBox *infoGroup = new QGroupBox("Информация о позиции", &dialog);
    QFormLayout *infoLayout = new QFormLayout(infoGroup);

    QLabel *typeLabel = new QLabel(item["material_type"].toString(), infoGroup);
    QLabel *manufacturerLabel = new QLabel(item["manufacturer"].toString(), infoGroup);
    QLabel *modelLabel = new QLabel(item["model"].toString(), infoGroup);
    QLabel *serialLabel = new QLabel(item["serial_number"].toString(), infoGroup);
    QLabel *partLabel = new QLabel(item["part_number"].toString(), infoGroup);

    infoLayout->addRow("Тип:", typeLabel);
    infoLayout->addRow("Производитель:", manufacturerLabel);
    infoLayout->addRow("Модель:", modelLabel);
    infoLayout->addRow("Серийный номер:", serialLabel);
    infoLayout->addRow("Part Number:", partLabel);

    // Поля для списания
    QGroupBox *writeOffGroup = new QGroupBox("Данные списания", &dialog);
    QFormLayout *writeOffLayout = new QFormLayout(writeOffGroup);

    QLineEdit *issuedToEdit = new QLineEdit(&dialog);
    issuedToEdit->setPlaceholderText("ФИО или отдел получателя");
    QDateEdit *issueDateEdit = new QDateEdit(QDate::currentDate(), &dialog);
    issueDateEdit->setDisplayFormat("dd.MM.yyyy");
    issueDateEdit->setCalendarPopup(true);
    QTextEdit *commentsEdit = new QTextEdit(&dialog);
    commentsEdit->setMaximumHeight(60);
    commentsEdit->setPlaceholderText("Причина списания, комментарии...");

    writeOffLayout->addRow("Кому выдано:", issuedToEdit);
    writeOffLayout->addRow("Дата выдачи:", issueDateEdit);
    writeOffLayout->addRow("Комментарий:", commentsEdit);

    // Кнопки
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

    layout->addWidget(infoGroup);
    layout->addWidget(writeOffGroup);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString issuedTo = issuedToEdit->text().trimmed();
        QDate issueDate = issueDateEdit->date();
        QString comments = commentsEdit->toPlainText().trimmed();

        if (issuedTo.isEmpty()) {
            QMessageBox::warning(this, "Внимание", "Укажите получателя");
            return;
        }

        if (db->markItemAsWrittenOff(itemId, issuedTo, issueDate, comments)) {
            QMessageBox::information(this, "Успех", "Позиция успешно списана");

            // Обновляем отображение
            int row = findRowByItemId(itemId);
            if (row >= 0) {
                updateRowAppearance(row, true);
            }

            // Обновляем данные в таблице
            loadInventoryTable();
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось списать позицию");
        }
    }
}

int MainWindow::findRowByItemId(int itemId)
{
    for (int row = 0; row < ui->inventoryTable->rowCount(); ++row) {
        if (ui->inventoryTable->item(row, 0)->text().toInt() == itemId) {
            return row;
        }
    }
    return -1;
}

void MainWindow::onReturnItem()
{
    QList<QTableWidgetItem*> selectedItems = ui->inventoryTable->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Выберите списанную запись для возврата");
        return;
    }

    int row = ui->inventoryTable->currentRow();
    int itemId = ui->inventoryTable->item(row, 0)->text().toInt();

    QString serialNumber = ui->inventoryTable->item(row, 5)->text();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Подтверждение возврата",
        QString("Вернуть позицию в наличие?\nСерийный номер: %1").arg(serialNumber),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (db->markItemAsAvailable(itemId)) {
            QMessageBox::information(this, "Успех", "Позиция возвращена в наличие");

            // Обновляем отображение
            updateRowAppearance(row, false);

            // Обновляем данные в таблице
            loadInventoryTable();
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось вернуть позицию");
        }
    }
}

void MainWindow::onShowWriteOffHistory()
{
    QDialog historyDialog(this);
    historyDialog.setWindowTitle("История списаний");
    historyDialog.resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(&historyDialog);

    // Таблица истории
    QTableWidget *historyTable = new QTableWidget(&historyDialog);
    historyTable->setColumnCount(9);
    QStringList headers = {"ID", "Тип", "Производитель", "Модель", "Part Number",
                          "Серийный номер", "Кому выдано", "Дата выдачи", "Комментарий"};
    historyTable->setHorizontalHeaderLabels(headers);
    historyTable->setColumnHidden(0, true);
    historyTable->setAlternatingRowColors(true);
    historyTable->setSortingEnabled(true);

    // Загружаем историю
    QList<QVariantMap> history = db->getWriteOffHistory();
    historyTable->setRowCount(history.size());

    for (int i = 0; i < history.size(); ++i) {
        const QVariantMap &record = history[i];

        historyTable->setItem(i, 0, new QTableWidgetItem(record["id"].toString()));
        historyTable->setItem(i, 1, new QTableWidgetItem(record["material_type"].toString()));
        historyTable->setItem(i, 2, new QTableWidgetItem(record["manufacturer"].toString()));
        historyTable->setItem(i, 3, new QTableWidgetItem(record["model"].toString()));
        historyTable->setItem(i, 4, new QTableWidgetItem(record["part_number"].toString()));
        historyTable->setItem(i, 5, new QTableWidgetItem(record["serial_number"].toString()));
        historyTable->setItem(i, 6, new QTableWidgetItem(record["issued_to"].toString()));
        historyTable->setItem(i, 7, new QTableWidgetItem(formatDateForDisplay(record["issue_date"].toString())));
        historyTable->setItem(i, 8, new QTableWidgetItem(record["comments"].toString()));
    }

    historyTable->resizeColumnsToContents();

    layout->addWidget(historyTable);

    // Кнопки
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &historyDialog);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected, &historyDialog, &QDialog::reject);

    historyDialog.exec();
}


QString MainWindow::formatDateForDisplay(const QString &dbDate)
{
    if (dbDate.isEmpty()) {
        return "";
    }

    QDate date = QDate::fromString(dbDate, "yyyy-MM-dd");
    if (date.isValid()) {
        return date.toString("dd.MM.yyyy");
    }

    return dbDate;
}

void MainWindow::loadItemForEdit(int itemId)
{
    qDebug() << "=== loadItemForEdit called ===";
    qDebug() << "Item ID to edit:" << itemId;

    QVariantMap item = db->getInventoryItemById(itemId);

    if (!db->checkInventoryItemExists(itemId)) {
        QMessageBox::warning(this, "Ошибка", "Запись не найдена");
        return;
    }

    if (item.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить данные для редактирования");
        qDebug() << "Failed to load item for editing";
        return;
    }

    currentEditId = itemId;

    ui->materialTypeCombo->setCurrentText(item["material_type"].toString());
    ui->manufacturerCombo->setCurrentText(item["manufacturer"].toString());
    ui->modelCombo->setCurrentText(item["model"].toString());
    ui->partNumberLineEdit->setText(item["part_number"].toString());

    // Просто устанавливаем значение серийного номера (может быть пустым)
    ui->serialLineEdit->setText(item["serial_number"].toString());

    ui->capacityLineEdit->setText(item["capacity"].toString());
    ui->interfaceCombo->setCurrentText(item["interface_type"].toString());
    ui->notesTextEdit->setText(item["notes"].toString());

    QString arrivalDateStr = item["arrival_date"].toString();
    if (!arrivalDateStr.isEmpty()) {
        ui->arrivalDateEdit->setDate(QDate::fromString(arrivalDateStr, "yyyy-MM-dd"));
    } else {
        ui->arrivalDateEdit->setDate(QDate::currentDate());
    }

    ui->invoiceLineEdit->setText(item["invoice_number"].toString());

    setEditMode(true);
    ui->tabWidget->setCurrentIndex(0);

    qDebug() << "Edit form populated successfully";
}

// Обновим clearForm для сброса серийного номера
void MainWindow::clearForm()
{
    ui->materialTypeCombo->setCurrentIndex(0);
    ui->manufacturerCombo->setCurrentIndex(0);
    ui->modelCombo->clear();
    ui->partNumberLineEdit->clear();
    ui->serialLineEdit->clear();
    ui->capacityLineEdit->clear();
    ui->interfaceCombo->setCurrentIndex(0);
    ui->notesTextEdit->clear();
    ui->arrivalDateEdit->setDate(QDate::currentDate());
    ui->invoiceLineEdit->clear();
    ui->materialTypeCombo->setFocus();

    // Сбрасываем режим редактирования если был
    if (currentEditId > 0) {
        setEditMode(false);
        ui->serialLineEdit->setReadOnly(false);
        ui->serialLineEdit->setStyleSheet("");
    }
}

void MainWindow::onAddItem()
{
    QString materialType = ui->materialTypeCombo->currentText().trimmed();
    QString manufacturer = ui->manufacturerCombo->currentText().trimmed();
    QString modelName = ui->modelCombo->currentText().trimmed();
    QString partNumber = ui->partNumberLineEdit->text().trimmed();
    QString serialNumber = ui->serialLineEdit->text().trimmed();
    QString capacity = ui->capacityLineEdit->text().trimmed();
    QString interfaceType = ui->interfaceCombo->currentText().trimmed();
    QString notes = ui->notesTextEdit->toPlainText().trimmed();
    QDate arrivalDate = ui->arrivalDateEdit->date();
    QString invoiceNumber = ui->invoiceLineEdit->text().trimmed();

    // Валидация (серийный номер теперь не обязателен)
    if (materialType.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Введите тип материала");
        ui->materialTypeCombo->setFocus();
        return;
    }

    if (manufacturer.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Введите производителя");
        ui->manufacturerCombo->setFocus();
        return;
    }

    if (modelName.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Введите модель");
        ui->modelCombo->setFocus();
        return;
    }

    // Для накопителей проверяем объем
    bool isStorage = (materialType == "Жесткий диск" || materialType == "SSD накопитель");
    if (isStorage && capacity.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Для накопителя укажите объем");
        ui->capacityLineEdit->setFocus();
        return;
    }

    // Добавляем в справочники
    db->addMaterialType(materialType);
    db->addManufacturer(manufacturer);
    db->addModel(materialType, manufacturer, modelName);

    // Добавляем запись (теперь серийный номер может быть пустым)
    if (db->addInventoryItem(materialType, manufacturer, modelName, partNumber,
                             serialNumber, capacity, interfaceType, notes,
                             arrivalDate, invoiceNumber)) {
        QMessageBox::information(this, "Успех", "Позиция успешно добавлена");

        refreshCompleters();
        loadMaterialsTree();
        loadInventoryTable();
        clearForm();
    } else {
        QMessageBox::critical(this, "Ошибка",
            "Не удалось добавить позицию. Возможно, серийный номер уже существует.");
    }
}

void MainWindow::onUpdateItem()
{
    qDebug() << "=== onUpdateItem called ===";

    if (currentEditId <= 0) {
        QMessageBox::warning(this, "Внимание", "Нет выбранной записи для редактирования");
        qDebug() << "No currentEditId:" << currentEditId;
        return;
    }

    QString materialType = ui->materialTypeCombo->currentText().trimmed();
    QString manufacturer = ui->manufacturerCombo->currentText().trimmed();
    QString modelName = ui->modelCombo->currentText().trimmed();
    QString partNumber = ui->partNumberLineEdit->text().trimmed();
    QString serialNumber = ui->serialLineEdit->text().trimmed(); // Может быть пустым
    QString capacity = ui->capacityLineEdit->text().trimmed();
    QString interfaceType = ui->interfaceCombo->currentText().trimmed();
    QString notes = ui->notesTextEdit->toPlainText().trimmed();
    QDate arrivalDate = ui->arrivalDateEdit->date();
    QString invoiceNumber = ui->invoiceLineEdit->text().trimmed();

    qDebug() << "Form data:";
    qDebug() << "  materialType:" << materialType;
    qDebug() << "  manufacturer:" << manufacturer;
    qDebug() << "  modelName:" << modelName;
    qDebug() << "  partNumber:" << partNumber;
    qDebug() << "  serialNumber:" << serialNumber;
    qDebug() << "  capacity:" << capacity;
    qDebug() << "  interfaceType:" << interfaceType;
    qDebug() << "  arrivalDate:" << arrivalDate.toString("yyyy-MM-dd");
    qDebug() << "  invoiceNumber:" << invoiceNumber;

    // Валидация (серийный номер теперь не обязателен)
    if (materialType.isEmpty() || manufacturer.isEmpty() || modelName.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Заполните обязательные поля");
        qDebug() << "Validation failed - empty required fields";
        return;
    }

    // Обновляем запись (serialNumber может быть пустым)
    if (db->updateInventoryItem(currentEditId, materialType, manufacturer, modelName,
                               partNumber, serialNumber, capacity, interfaceType,
                               notes, arrivalDate, invoiceNumber)) {
        QMessageBox::information(this, "Успех", "Позиция успешно обновлена");
        qDebug() << "Update successful!";

        refreshCompleters();
        loadMaterialsTree();
        loadInventoryTable();
        clearForm();
        setEditMode(false);
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось обновить позицию");
        qDebug() << "Update failed!";
    }
}

void MainWindow::onDeleteItem()
{
    QList<QTableWidgetItem*> selectedItems = ui->inventoryTable->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Выберите запись для удаления");
        return;
    }

    int row = ui->inventoryTable->currentRow();
    if (row < 0) return;

    int itemId = ui->inventoryTable->item(row, 0)->text().toInt();
    QString serialNumber = ui->inventoryTable->item(row, 5)->text();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Подтверждение удаления",
        QString("Вы уверены, что хотите удалить запись?\nСерийный номер: %1").arg(serialNumber),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (db->deleteInventoryItem(itemId)) {
            QMessageBox::information(this, "Успех", "Запись успешно удалена");
            loadInventoryTable();
            ui->editButton->setEnabled(false);
            ui->deleteButton->setEnabled(false);
            dashboardWidget->refreshStats();
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось удалить запись");
        }
    }
}

void MainWindow::onCancelEdit()
{
    clearForm();
    setEditMode(false);
}

void MainWindow::onEditItem()
{
    QList<QTableWidgetItem*> selectedItems = ui->inventoryTable->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Выберите запись для редактирования");
        return;
    }

    int row = ui->inventoryTable->currentRow();
    if (row < 0) return;

    int itemId = ui->inventoryTable->item(row, 0)->text().toInt();
    loadItemForEdit(itemId);
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    if (text.isEmpty()) {
        loadInventoryTable();
    } else {
        QList<QVariantMap> searchResults = db->searchInventory(text);
        loadInventoryTable(searchResults);
    }
}

void MainWindow::onMaterialTypeChanged(const QString &text)
{
    Q_UNUSED(text);

    QString currentMaterial = ui->materialTypeCombo->currentText();
    QString currentManufacturer = ui->manufacturerCombo->currentText();

    if (!currentMaterial.isEmpty() && !currentManufacturer.isEmpty()) {
        QStringList models = db->getModelsByMaterialAndManufacturer(currentMaterial, currentManufacturer);
        modelModel->clear();
        for (const QString &model : models) {
            modelModel->appendRow(new QStandardItem(model));
        }
        ui->modelCombo->clear();
        ui->modelCombo->addItems(models);
    }

    updateInterfaceVisibility();
}

void MainWindow::onManufacturerChanged(const QString &text)
{
    Q_UNUSED(text);

    QString currentMaterial = ui->materialTypeCombo->currentText();
    QString currentManufacturer = ui->manufacturerCombo->currentText();

    if (!currentMaterial.isEmpty() && !currentManufacturer.isEmpty()) {
        QStringList models = db->getModelsByMaterialAndManufacturer(currentMaterial, currentManufacturer);
        modelModel->clear();
        for (const QString &model : models) {
            modelModel->appendRow(new QStandardItem(model));
        }
        ui->modelCombo->clear();
        ui->modelCombo->addItems(models);
    }
}

void MainWindow::onTableSelectionChanged()
{
    bool hasSelection = !ui->inventoryTable->selectedItems().isEmpty();
    ui->editButton->setEnabled(hasSelection);
    ui->deleteButton->setEnabled(hasSelection);
}

void MainWindow::onGenerateReport()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить отчет",
                                                   "Отчет_ЗИП_" + QDate::currentDate().toString("yyyy-MM-dd") + ".csv",
                                                   "CSV Files (*.csv);;Text Files (*.txt)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл");
        return;
    }

    QStringList reportTypes;
        reportTypes << "Текущий инвентарь" << "История списаний";

        bool ok;
        QString reportType = QInputDialog::getItem(this, "Тип отчета",
                                                   "Выберите тип отчета:",
                                                   reportTypes, 0, false, &ok);
        if (!ok) return;

        if (reportType == "История списаний") {
            exportWriteOffHistory(fileName);
            return;
        }

    QTextStream stream(&file);
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
    #else
        stream.setCodec("UTF-8");
    #endif

    // Базовый заголовок
    stream << "ID;Тип;Производитель;Модель;Part Number;Серийный номер;Объем;Интерфейс;Дата прихода;Накладная;Примечание";

    // Добавляем дополнительные поля если они есть
    QList<QVariantMap> items = db->getInventoryItems();
    if (!items.isEmpty() && items.first().contains("created_at")) {
        stream << ";Создано";
    }
    if (!items.isEmpty() && items.first().contains("updated_at")) {
        stream << ";Обновлено";
    }

    stream << "\n";

    // Данные
    for (const QVariantMap &item : items) {
        stream << item["id"].toString() << ";"
               << item["material_type"].toString() << ";"
               << item["manufacturer"].toString() << ";"
               << item["model"].toString() << ";"
               << item["part_number"].toString() << ";"
               << item["serial_number"].toString() << ";"
               << item["capacity"].toString() << ";"
               << item["interface_type"].toString() << ";"
               << item["arrival_date"].toString() << ";"
               << item["invoice_number"].toString() << ";"
               << item["notes"].toString().replace("\n", " ");

        if (item.contains("created_at") && !item["created_at"].toString().isEmpty()) {
            stream << ";" << item["created_at"].toString();
        }

        if (item.contains("updated_at") && !item["updated_at"].toString().isEmpty()) {
            stream << ";" << item["updated_at"].toString();
        }

        stream << "\n";
    }

    file.close();
    QMessageBox::information(this, "Успех", QString("Отчет успешно сформирован\nФайл: %1").arg(fileName));
}

void MainWindow::exportWriteOffHistory(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл");
        return;
    }

    QTextStream stream(&file);
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
    #else
        stream.setCodec("UTF-8");
    #endif

    stream << "ID;Тип;Производитель;Модель;Part Number;Серийный номер;"
           << "Кому выдано;Дата выдачи;Комментарий;Дата списания\n";

    QList<QVariantMap> history = db->getWriteOffHistory();
    for (const QVariantMap &record : history) {
        stream << record["id"].toString() << ";"
               << record["material_type"].toString() << ";"
               << record["manufacturer"].toString() << ";"
               << record["model"].toString() << ";"
               << record["part_number"].toString() << ";"
               << record["serial_number"].toString() << ";"
               << record["issued_to"].toString() << ";"
               << record["issue_date"].toString() << ";"
               << record["comments"].toString().replace("\n", " ") << ";"
               << record["created_at"].toString() << "\n";
    }

    file.close();
    QMessageBox::information(this, "Успех",
        QString("Отчет истории списаний успешно сформирован\nФайл: %1").arg(fileName));
}

void MainWindow::onTreeCustomContextMenu(const QPoint &pos)
{
    QTreeWidgetItem* item = ui->materialsTree->itemAt(pos);

    if (item) {
        // Не разрешаем удаление корневого элемента "Все материалы"
        if (item == ui->materialsTree->topLevelItem(0)) {
            deleteAction->setEnabled(false);
        } else {
            deleteAction->setEnabled(true);
        }

        // Сохраняем выбранный элемент для использования в слотах
        contextMenuItem = item;

        // Показываем меню
        treeContextMenu->exec(ui->materialsTree->viewport()->mapToGlobal(pos));
    }
}

void MainWindow::onDeleteFromTree()
{
    if (!contextMenuItem) return;

    deleteSelectedTreeItem();
}

void MainWindow::deleteSelectedTreeItem()
{
    if (!contextMenuItem) return;

    QString itemText = contextMenuItem->text(0);
    QString cleanText = getItemTextWithoutEmoji(itemText);

    // Определяем уровень элемента более точно
    QTreeWidgetItem* parent = contextMenuItem->parent();
    QTreeWidgetItem* grandParent = parent ? parent->parent() : nullptr;
    QTreeWidgetItem* greatGrandParent = grandParent ? grandParent->parent() : nullptr;

    bool isModel = (parent && grandParent && greatGrandParent); // Уровень 3 (модель)
    bool isManufacturer = (parent && grandParent && !greatGrandParent); // Уровень 2 (производитель)
    bool isMaterialType = (parent && !grandParent); // Уровень 1 (тип материала)

    qDebug() << "=== deleteSelectedTreeItem ===";
    qDebug() << "Item text:" << itemText;
    qDebug() << "Clean text:" << cleanText;
    qDebug() << "Levels: parent=" << parent << " grandParent=" << grandParent << " greatGrandParent=" << greatGrandParent;
    qDebug() << "isModel:" << isModel << " isManufacturer:" << isManufacturer << " isMaterialType:" << isMaterialType;

    QString message;
    QString details;
    bool canDelete = false;

    if (isModel) {
        // Это модель (3 уровень)
        QString modelName = cleanText;
        QString manufacturer = getItemTextWithoutEmoji(parent->text(0));
        QString materialType = getItemTextWithoutEmoji(grandParent->text(0));

        qDebug() << "Model details:";
        qDebug() << "  Model:" << modelName;
        qDebug() << "  Manufacturer:" << manufacturer;
        qDebug() << "  Material type:" << materialType;

        int usageCount = db->getUsageCountForModel(materialType, manufacturer, modelName);
        qDebug() << "Usage count:" << usageCount;

        message = QString("Удалить модель '%1'?").arg(modelName);
        details = QString("Производитель: %1\nТип материала: %2\n\n")
                         .arg(manufacturer)
                         .arg(materialType);

        if (usageCount > 0) {
            details += QString("⚠️ Невозможно удалить!\n"
                              "Эта модель используется в %1 записях инвентаря.\n"
                              "Сначала удалите или измените эти записи.")
                              .arg(usageCount);
            canDelete = false;
        } else {
            details += "✅ Эта модель не используется в записях инвентаря.\nМожно удалить.";
            canDelete = true;
        }

        if (canDelete) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Подтверждение удаления",
                message + "\n\n" + details,
                QMessageBox::Yes | QMessageBox::No
            );

            if (reply == QMessageBox::Yes) {
                if (db->deleteModel(materialType, manufacturer, modelName)) {
                    QMessageBox::information(this, "Успех", "Модель успешно удалена");
                    loadMaterialsTree();
                    refreshCompleters();
                } else {
                    QMessageBox::warning(this, "Ошибка", "Не удалось удалить модель");
                }
            }
        } else {
            QMessageBox::information(this, "Невозможно удалить", details);
        }

    } else if (isManufacturer) {
        // Это производитель (2 уровень)
        QString manufacturer = cleanText;
        QString materialType = getItemTextWithoutEmoji(parent->text(0));

        qDebug() << "Manufacturer details:";
        qDebug() << "  Manufacturer:" << manufacturer;
        qDebug() << "  Material type:" << materialType;

        int usageCount = db->getUsageCountForManufacturer(manufacturer);
        qDebug() << "Usage count for manufacturer:" << usageCount;

        message = QString("Удалить производителя '%1'?").arg(manufacturer);
        details = QString("Тип материала: %1\n\n").arg(materialType);

        // Проверяем, есть ли у этого производителя модели
        QStringList models = db->getModelsByMaterialAndManufacturer(materialType, manufacturer);
        qDebug() << "Models for this manufacturer:" << models;

        if (usageCount > 0) {
            details += QString("⚠️ Невозможно удалить!\n"
                              "Этот производитель используется в %1 записях инвентаря.\n"
                              "Сначала удалите или измените эти записи.")
                              .arg(usageCount);
            canDelete = false;
        } else if (!models.isEmpty()) {
            details += QString("⚠️ Сначала удалите все модели этого производителя!\n"
                              "Количество моделей: %1").arg(models.size());
            canDelete = false;
        } else {
            details += "✅ Этот производитель не используется и не имеет моделей.\nМожно удалить.";
            canDelete = true;
        }

        if (canDelete) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Подтверждение удаления",
                message + "\n\n" + details,
                QMessageBox::Yes | QMessageBox::No
            );

            if (reply == QMessageBox::Yes) {
                if (db->deleteManufacturer(manufacturer)) {
                    QMessageBox::information(this, "Успех", "Производитель успешно удален");
                    loadMaterialsTree();
                    refreshCompleters();
                } else {
                    QMessageBox::warning(this, "Ошибка", "Не удалось удалить производителя");
                }
            }
        } else {
            QMessageBox::information(this, "Невозможно удалить", details);
        }

    } else if (isMaterialType) {
        // Это тип материала (1 уровень)
        QString materialType = cleanText;

        qDebug() << "Material type details:";
        qDebug() << "  Material type:" << materialType;

        int usageCount = db->getUsageCountForMaterialType(materialType);
        qDebug() << "Usage count for material type:" << usageCount;

        message = QString("Удалить тип материала '%1'?").arg(materialType);

        // Проверяем, есть ли модели этого типа
        QStringList models = db->getModelsByMaterial(materialType);
        qDebug() << "Models for this material type:" << models;

        if (usageCount > 0) {
            details = QString("⚠️ Невозможно удалить!\n"
                             "Этот тип материала используется в %1 записях инвентаря.\n"
                             "Сначала удалите или измените эти записи.")
                             .arg(usageCount);
            canDelete = false;
        } else if (!models.isEmpty()) {
            details = QString("⚠️ Сначала удалите все модели этого типа!\n"
                             "Количество моделей: %1").arg(models.size());
            canDelete = false;
        } else {
            details = "✅ Этот тип материала не используется и не имеет моделей.\nМожно удалить.";
            canDelete = true;
        }

        if (canDelete) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Подтверждение удаления",
                message + "\n\n" + details,
                QMessageBox::Yes | QMessageBox::No
            );

            if (reply == QMessageBox::Yes) {
                if (db->deleteMaterialType(materialType)) {
                    QMessageBox::information(this, "Успех", "Тип материала успешно удален");
                    loadMaterialsTree();
                    refreshCompleters();
                } else {
                    QMessageBox::warning(this, "Ошибка", "Не удалось удалить тип материала");
                }
            }
        } else {
            QMessageBox::information(this, "Невозможно удалить", details);
        }
    } else {
        qDebug() << "Unknown item level or root item selected";
        QMessageBox::information(this, "Информация", "Нельзя удалить корневой элемент или элемент неизвестного уровня");
    }

    contextMenuItem = nullptr;
}

void MainWindow::onRefreshTree()
{
    loadMaterialsTree();
    QMessageBox::information(this, "Обновление", "Дерево материалов обновлено");
}

QString MainWindow::getItemTextWithoutEmoji(const QString &textWithEmoji)
{
    // Удаляем эмодзи и пробел в начале
    // Эмодзи могут занимать 2 символа (некоторые 4)
    QString result = textWithEmoji;

    // Удаляем первый символ (эмодзи) и пробел, если они есть
    if (!result.isEmpty()) {
        // Проверяем, есть ли пробел после эмодзи
        for (int i = 0; i < result.length() - 1; i++) {
            if (result[i] == ' ') {
                // Если нашли пробел, возвращаем все после него
                return result.mid(i + 1);
            }
        }
    }

    return result;
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    if (!item) return;

    if (item == ui->materialsTree->topLevelItem(0)) {
        return;
    }

    if (item->parent() && item->parent()->parent() && item->parent()->parent()->parent()) {
        QString modelName = getItemTextWithoutEmoji(item->text(0));
        QString manufacturer = getItemTextWithoutEmoji(item->parent()->text(0));
        QString materialType = getItemTextWithoutEmoji(item->parent()->parent()->text(0));

        ui->materialTypeCombo->setCurrentText(materialType);
        ui->manufacturerCombo->setCurrentText(manufacturer);
        ui->modelCombo->setCurrentText(modelName);
    }
    else if (item->parent() && item->parent()->parent()) {
        QString manufacturer = getItemTextWithoutEmoji(item->text(0));
        QString materialType = getItemTextWithoutEmoji(item->parent()->text(0));

        ui->materialTypeCombo->setCurrentText(materialType);
        ui->manufacturerCombo->setCurrentText(manufacturer);
    }
    else if (item->parent()) {
        QString materialType = getItemTextWithoutEmoji(item->text(0));
        ui->materialTypeCombo->setCurrentText(materialType);
    }
}

void MainWindow::setupSortMenu()
{
    // Создаем меню сортировки
    sortMenu = new QMenu(this);

    // Создаем действия для меню
    sortByDateDescAction = new QAction("📅 По дате прихода (сначала новые)", this);
    sortByDateAscAction = new QAction("📅 По дате прихода (сначала старые)", this);
    sortByTypeAction = new QAction("📦 По типу материала", this);
    sortByManufacturerAction = new QAction("🏭 По производителю", this);
    sortByModelAction = new QAction("📝 По модели", this);
    sortBySerialAction = new QAction("🏷️ По серийному номеру", this);

    // Добавляем действия в меню
    sortMenu->addAction(sortByDateDescAction);
    sortMenu->addAction(sortByDateAscAction);
    sortMenu->addSeparator();
    sortMenu->addAction(sortByTypeAction);
    sortMenu->addAction(sortByManufacturerAction);
    sortMenu->addAction(sortByModelAction);
    sortMenu->addAction(sortBySerialAction);

    // Устанавливаем меню для кнопки
    ui->sortButton->setMenu(sortMenu);

    // Подключаем слоты
    connect(sortByDateDescAction, &QAction::triggered, this, &MainWindow::onSortByDateDesc);
    connect(sortByDateAscAction, &QAction::triggered, this, &MainWindow::onSortByDateAsc);
    connect(sortByTypeAction, &QAction::triggered, this, &MainWindow::onSortByType);
    connect(sortByManufacturerAction, &QAction::triggered, this, &MainWindow::onSortByManufacturer);
    connect(sortByModelAction, &QAction::triggered, this, &MainWindow::onSortByModel);
    connect(sortBySerialAction, &QAction::triggered, this, &MainWindow::onSortBySerial);
}

void MainWindow::onSortByDateDesc()
{
    ui->inventoryTable->sortItems(8, Qt::DescendingOrder); // Колонка 8 - дата прихода
    ui->sortButton->setText("📊 Сортировка: по дате ▼");
}

void MainWindow::onSortByDateAsc()
{
    ui->inventoryTable->sortItems(8, Qt::AscendingOrder);
    ui->sortButton->setText("📊 Сортировка: по дате ▲");
}

void MainWindow::onSortByType()
{
    ui->inventoryTable->sortItems(1, Qt::AscendingOrder); // Колонка 1 - тип
    ui->sortButton->setText("📊 Сортировка: по типу");
}

void MainWindow::onSortByManufacturer()
{
    ui->inventoryTable->sortItems(2, Qt::AscendingOrder); // Колонка 2 - производитель
    ui->sortButton->setText("📊 Сортировка: по производителю");
}

void MainWindow::onSortByModel()
{
    ui->inventoryTable->sortItems(3, Qt::AscendingOrder); // Колонка 3 - модель
    ui->sortButton->setText("📊 Сортировка: по модели");
}

void MainWindow::onSortBySerial()
{
    ui->inventoryTable->sortItems(5, Qt::AscendingOrder); // Колонка 5 - серийный номер
    ui->sortButton->setText("📊 Сортировка: по серийному номеру");
}

void MainWindow::onPrintLabels()
{
    // Получаем выбранные элементы
    QList<int> selectedIds;
    QList<QTableWidgetItem*> selectedItems = ui->inventoryTable->selectedItems();

    if (selectedItems.isEmpty()) {
        // Если ничего не выбрано, печатаем все видимые
        for (int row = 0; row < ui->inventoryTable->rowCount(); ++row) {
            bool ok;
            int id = ui->inventoryTable->item(row, 0)->text().toInt(&ok);
            if (ok) {
                selectedIds.append(id);
            }
        }
    } else {
        // Печатаем только выбранные
        QSet<int> rows;
        for (QTableWidgetItem *item : selectedItems) {
            rows.insert(item->row());
        }
        for (int row : rows) {
            bool ok;
            int id = ui->inventoryTable->item(row, 0)->text().toInt(&ok);
            if (ok) {
                selectedIds.append(id);
            }
        }
    }

    if (selectedIds.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Нет позиций для печати");
        return;
    }

    // Получаем данные из БД
    QList<QVariantMap> items = db->getItemsForLabels(selectedIds);

    if (items.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить данные для печати");
        return;
    }

    // Показываем диалог печати
    LabelPrintDialog dialog(items, this);
    dialog.exec();
}

void MainWindow::onAdvancedFilter()
{
    AdvancedFilterDialog dialog(this);

    // Заполняем списки данными из БД
    QComboBox *materialCombo = dialog.findChild<QComboBox*>("materialTypeCombo");
    if (materialCombo) {
        materialCombo->clear();
        materialCombo->addItem("Все типы", "");  // Добавляем с пустыми данными
        QStringList materials = db->getMaterialTypes();
        for (const QString &material : materials) {
            materialCombo->addItem(material, material);  // Добавляем с данными = тексту
        }
        materialCombo->setEditable(true);
        materialCombo->setCurrentIndex(0);
    }

    QComboBox *manufacturerCombo = dialog.findChild<QComboBox*>("manufacturerCombo");
    if (manufacturerCombo) {
        manufacturerCombo->clear();
        manufacturerCombo->addItem("Все производители", "");
        QStringList manufacturers = db->getManufacturers();
        for (const QString &manufacturer : manufacturers) {
            manufacturerCombo->addItem(manufacturer, manufacturer);
        }
        manufacturerCombo->setEditable(true);
        manufacturerCombo->setCurrentIndex(0);
    }

    QComboBox *modelCombo = dialog.findChild<QComboBox*>("modelCombo");
    if (modelCombo) {
        modelCombo->clear();
        modelCombo->addItem("Все модели", "");
        modelCombo->setEditable(true);
        modelCombo->setCurrentIndex(0);
    }

    // Подключаем обновление моделей
    if (materialCombo && manufacturerCombo && modelCombo) {
        auto updateModels = [materialCombo, manufacturerCombo, modelCombo, this]() {
            QString material = materialCombo->currentText();
            QString manufacturer = manufacturerCombo->currentText();

            qDebug() << "Updating models for material:" << material << "manufacturer:" << manufacturer;

            modelCombo->clear();
            modelCombo->addItem("Все модели", "");

            if (!material.isEmpty() && material != "Все типы" &&
                !manufacturer.isEmpty() && manufacturer != "Все производители") {
                QStringList models = db->getModelsByMaterialAndManufacturer(material, manufacturer);
                for (const QString &model : models) {
                    modelCombo->addItem(model, model);
                }
                qDebug() << "Found" << models.size() << "models";
            }
            modelCombo->setCurrentIndex(0);
        };

        connect(materialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                [updateModels](int) { updateModels(); });
        connect(manufacturerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                [updateModels](int) { updateModels(); });
    }

    if (dialog.exec() == QDialog::Accepted) {
        AdvancedFilterDialog::FilterParams params = dialog.getFilterParams();

        qDebug() << "=== Applying filter in MainWindow ===";
        qDebug() << "material:" << params.materialType;
        qDebug() << "manufacturer:" << params.manufacturer;
        qDebug() << "model:" << params.model;
        qDebug() << "partNumber:" << params.partNumber;
        qDebug() << "serialNumber:" << params.serialNumber;
        qDebug() << "status:" << params.status;
        qDebug() << "useDateRange:" << params.useDateRange;
        qDebug() << "dateFrom:" << params.dateFrom.toString("dd.MM.yyyy");
        qDebug() << "dateTo:" << params.dateTo.toString("dd.MM.yyyy");

        // Применяем фильтр
        QList<QVariantMap> filteredItems = db->getFilteredInventory(
            params.materialType,
            params.manufacturer,
            params.model,
            params.partNumber,
            params.serialNumber,
            params.status,
            params.useDateRange ? params.dateFrom : QDate(),
            params.useDateRange ? params.dateTo : QDate()
        );

        qDebug() << "Filtered items count:" << filteredItems.size();
        loadInventoryTable(filteredItems);

        // Показываем индикатор активного фильтра
        if (!params.materialType.isEmpty() || !params.manufacturer.isEmpty() ||
            !params.model.isEmpty() || !params.partNumber.isEmpty() ||
            !params.serialNumber.isEmpty() || params.status != "all" ||
            params.useDateRange) {
            ui->advancedFilterButton->setText("🔍 Фильтр активен*");
            ui->advancedFilterButton->setStyleSheet("font-weight: bold; color: blue;");
        } else {
            ui->advancedFilterButton->setText("🔍 Расширенный фильтр");
            ui->advancedFilterButton->setStyleSheet("");
        }
    }
}


void MainWindow::refreshStats()
{
    if (dashboardWidget) {
        dashboardWidget->refreshStats();
    }
}


void MainWindow::about()
{
    QMessageBox::about(this, "О программе",
                       "<h3>Программа для учёта ЗИП v0.13</h3>"
                       "<p>Приложение для каталогизации материалов и запасных частей</p>"
                       "<p><b>Возможности:</b><br>"
                       "✅ Добавление и редактирование записей о комплектующих:<br>"
                       "• Тип материала (HDD, SSD, ОЗУ, видеокарта и т.д.)<br>"
                       "• Производитель<br>"
                       "• Модель<br>"
                       "• Part Number и серийный номер (необязательный)<br>"
                       "• Объём и интерфейс (для накопителей и памяти)<br>"
                       "• Дата поступления и номер накладной<br>"
                       "• Примечания<br>"
                       "<p><b>🗑️ Списание и возврат</b><br>"
                       "<p><b>🔍 Поиск и фильтрация </b><br>"
                       "<p><b>📊 Отчёты</b><br>"
                       "<p><b>🌲 Иерархическое дерево материалов</b><br>"
                       "<p><b>🧩 Гибкая работа со справочниками:  </b><br>"
                       "• Автоматическое добавление новых типов, производителей и моделей<br>"
                       "• Поддержка редактируемых выпадающих списков с автодополнением<br>"
                       "<p><b>Для печати этикеток используется - Qt QR Code Generator Library</b><br>"
                       "<p><b>Автор:</b><br>"
                       "• LostDragon (ldragon24@gmail.com)</b></p>");
}
