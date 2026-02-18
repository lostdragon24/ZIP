#include "labelprintdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPrinter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QTextDocument>
#include <QFileDialog>
#include <QMessageBox>
#include <QBuffer>


LabelPrintDialog::LabelPrintDialog(const QList<QVariantMap> &items, QWidget *parent)
    : QDialog(parent),
      allItems(items),
      qrGenerator(new QrCodeGenerator(this))  // Инициализируем генератор
{
    setupUI();
}

LabelPrintDialog::~LabelPrintDialog()
{
    // QScopedPointer автоматически удалит qrGenerator
}


void LabelPrintDialog::setupUI()
{
    setWindowTitle("Печать этикеток");
    resize(600, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Таблица с позициями
    QGroupBox *itemsGroup = new QGroupBox("Выберите позиции для печати", this);
    QVBoxLayout *itemsLayout = new QVBoxLayout(itemsGroup);

    itemsTable = new QTableWidget(this);
    itemsTable->setColumnCount(6);
    QStringList headers = {"", "ID", "Тип", "Производитель", "Модель", "Серийный номер"};
    itemsTable->setHorizontalHeaderLabels(headers);
    itemsTable->setColumnHidden(1, true); // Скрываем ID
    itemsTable->horizontalHeader()->setStretchLastSection(true);
    itemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Заполняем таблицу
    itemsTable->setRowCount(allItems.size());
    for (int i = 0; i < allItems.size(); ++i) {
        const QVariantMap &item = allItems[i];

        QCheckBox *checkBox = new QCheckBox(this);
        checkBox->setChecked(true);
        itemsTable->setCellWidget(i, 0, checkBox);

        itemsTable->setItem(i, 1, new QTableWidgetItem(item["id"].toString()));
        itemsTable->setItem(i, 2, new QTableWidgetItem(item["material_type"].toString()));
        itemsTable->setItem(i, 3, new QTableWidgetItem(item["manufacturer"].toString()));
        itemsTable->setItem(i, 4, new QTableWidgetItem(item["model"].toString()));
        itemsTable->setItem(i, 5, new QTableWidgetItem(item["serial_number"].toString()));
    }

    itemsTable->resizeColumnsToContents();
    itemsLayout->addWidget(itemsTable);

    // Кнопки выделения
    QHBoxLayout *selectLayout = new QHBoxLayout();
    QPushButton *selectAllBtn = new QPushButton("Выделить все", this);
    QPushButton *clearAllBtn = new QPushButton("Снять выделение", this);
    selectLayout->addWidget(selectAllBtn);
    selectLayout->addWidget(clearAllBtn);
    selectLayout->addStretch();
    itemsLayout->addLayout(selectLayout);

    connect(selectAllBtn, &QPushButton::clicked, this, &LabelPrintDialog::onSelectAll);
    connect(clearAllBtn, &QPushButton::clicked, this, &LabelPrintDialog::onClearAll);

    mainLayout->addWidget(itemsGroup);

    // Настройки печати
    QGroupBox *settingsGroup = new QGroupBox("Настройки", this);
    QGridLayout *settingsLayout = new QGridLayout(settingsGroup);

    settingsLayout->addWidget(new QLabel("Количество копий:", this), 0, 0);
    copiesSpinBox = new QSpinBox(this);
    copiesSpinBox->setRange(1, 10);
    copiesSpinBox->setValue(1);
    settingsLayout->addWidget(copiesSpinBox, 0, 1);

    settingsLayout->addWidget(new QLabel("Размер этикетки:", this), 1, 0);
    labelSizeCombo = new QComboBox(this);
    labelSizeCombo->addItem("Малая (50x30 мм)", "50x30");
    labelSizeCombo->addItem("Средняя (70x50 мм)", "70x50");
    labelSizeCombo->addItem("Большая (100x70 мм)", "100x70");
    settingsLayout->addWidget(labelSizeCombo, 1, 1);

    includeQRCheckBox = new QCheckBox("Добавить QR-код", this);
    includeQRCheckBox->setChecked(true);

    // В секции настроек, после includeQRCheckBox добавьте:
        QLabel *qrSizeLabel = new QLabel("Размер QR-кода:", this);
        QComboBox *qrSizeCombo = new QComboBox(this);
        qrSizeCombo->addItem("Малый (50x50)", 50);
        qrSizeCombo->addItem("Средний (80x80)", 80);
        qrSizeCombo->addItem("Большой (100x100)", 100);
        qrSizeCombo->setCurrentIndex(1); // Средний по умолчанию

        settingsLayout->addWidget(qrSizeLabel, 3, 0);
        settingsLayout->addWidget(qrSizeCombo, 3, 1);


    settingsLayout->addWidget(includeQRCheckBox, 2, 0, 1, 2);

    mainLayout->addWidget(settingsGroup);

    // Кнопки
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this);
    QPushButton *printBtn = buttonBox->button(QDialogButtonBox::Ok);
    printBtn->setText("Печать");
    // printBtn->setIcon(QIcon(":/icons/print.png")); // Закомментировано, если нет иконки

    QPushButton *previewBtn = new QPushButton("Предпросмотр", this);
    buttonBox->addButton(previewBtn, QDialogButtonBox::ActionRole);

    connect(printBtn, &QPushButton::clicked, this, &LabelPrintDialog::onPrint);
    connect(previewBtn, &QPushButton::clicked, this, &LabelPrintDialog::onPreview);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

void LabelPrintDialog::onSelectAll()
{
    for (int i = 0; i < itemsTable->rowCount(); ++i) {
        QCheckBox *checkBox = qobject_cast<QCheckBox*>(itemsTable->cellWidget(i, 0));
        if (checkBox) {
            checkBox->setChecked(true);
        }
    }
}

void LabelPrintDialog::onClearAll()
{
    for (int i = 0; i < itemsTable->rowCount(); ++i) {
        QCheckBox *checkBox = qobject_cast<QCheckBox*>(itemsTable->cellWidget(i, 0));
        if (checkBox) {
            checkBox->setChecked(false);
        }
    }
}

void LabelPrintDialog::onPrint()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() == QDialog::Accepted) {
        printLabels(&printer);
    }
}

void LabelPrintDialog::onPreview()
{
    QPrintPreviewDialog preview(this);
    connect(&preview, &QPrintPreviewDialog::paintRequested,
            [this](QPrinter *printer) { this->printLabels(printer); });
    preview.exec();
}

void LabelPrintDialog::printLabels(QPrinter *printer)
{
    // Собираем выбранные элементы
    QList<QVariantMap> selectedItems;
    for (int i = 0; i < itemsTable->rowCount(); ++i) {
        QCheckBox *checkBox = qobject_cast<QCheckBox*>(itemsTable->cellWidget(i, 0));
        if (checkBox && checkBox->isChecked()) {
            QVariantMap item;
            item["id"] = itemsTable->item(i, 1)->text();
            item["material_type"] = itemsTable->item(i, 2)->text();
            item["manufacturer"] = itemsTable->item(i, 3)->text();
            item["model"] = itemsTable->item(i, 4)->text();
            item["serial_number"] = itemsTable->item(i, 5)->text();

            // Добавляем дополнительные поля
            if (i < allItems.size() && allItems[i].contains("part_number")) {
                item["part_number"] = allItems[i]["part_number"];
            }
            if (i < allItems.size() && allItems[i].contains("arrival_date")) {
                QDate date = QDate::fromString(allItems[i]["arrival_date"].toString(), "yyyy-MM-dd");
                if (date.isValid()) {
                    item["date"] = date.toString("dd.MM.yyyy");
                }
            }

            selectedItems.append(item);
        }
    }

    if (selectedItems.isEmpty()) {
        return;
    }

    int copies = copiesSpinBox->value();
    QString labelSize = labelSizeCombo->currentData().toString();

    // Определяем размеры
    int labelsPerRow = 2; // По умолчанию 2 этикетки в ряд
    if (labelSize == "50x30") {
        labelsPerRow = 3;
    } else if (labelSize == "100x70") {
        labelsPerRow = 1;
    }

    // Генерируем HTML
    QString html;
    html += "<!DOCTYPE html>\n";
    html += "<html>\n";
    html += "<head>\n";
    html += "<meta http-equiv='content-type' content='text/html; charset=utf-8'/>\n";
    html += "<style type='text/css'>\n";
    html += "@page { size: 21cm 29.7cm; margin: 0.5cm }\n";
    html += "body { font-family: 'Arial', sans-serif; margin: 0; padding: 0; }\n";
    html += "table { width: 100%; border-collapse: collapse; }\n";
    html += "td { border: 1px solid #000000; padding: 2mm; vertical-align: top; }\n";
    html += ".label-table { width: 100%; height: 100%; border: none; }\n";
    html += ".label-table td { border: none; padding: 0.5mm; }\n";
    html += ".label-cell { font-size: 6pt; }\n";
    html += ".label-cell p { margin: 0; padding: 0; line-height: 1.2; }\n";
    html += ".qr-cell { text-align: center; vertical-align: middle; }\n";
    html += ".qr-img { width: 15mm; height: 15mm; }\n";
    html += ".copy-number { font-size: 6pt; text-align: right; margin-top: 1mm; }\n";
    html += "</style>\n";
    html += "</head>\n";
    html += "<body lang='ru-RU'>\n";

    // Создаем основную таблицу для размещения этикеток
    html += "<table>\n";

    int itemIndex = 0;
    while (itemIndex < selectedItems.size()) {
        html += "<tr>\n";

        for (int col = 0; col < labelsPerRow && itemIndex < selectedItems.size(); ++col) {
            const QVariantMap &item = selectedItems[itemIndex];

            html += "<td style='width: " + QString::number(100 / labelsPerRow) + "%'>\n";

            // Для каждой копии создаем отдельную этикетку
            for (int copy = 0; copy < copies; ++copy) {
                // Внутренняя таблица этикетки
                html += "<table class='label-table' style='margin-bottom: 2mm;'>\n";

                // Строка с типом материала и QR-кодом
                html += "<tr>\n";
                html += "<td class='label-cell' style='width: 60%;'><p><b>" +
                        item["material_type"].toString() + "</b></p></td>\n";

                // QR-код в первой строке, объединяя строки
                if (includeQRCheckBox->isChecked()) {
                    QString qrData = QString("ID:%1\nSN:%2\n%3 %4")
                                        .arg(item["id"].toString())
                                        .arg(item["serial_number"].toString())
                                        .arg(item["manufacturer"].toString())
                                        .arg(item["model"].toString());

                    QString qrBase64 = generateQrBase64(qrData, 80);

                    if (!qrBase64.isEmpty()) {
                        html += "<td class='qr-cell' rowspan='6' style='width: 40%;'>";
                        html += "<img class='qr-img' src='" + qrBase64 + "'/>";
                        html += "</td>\n";
                    }
                }
                html += "</tr>\n";

                // Производитель
                html += "<tr>\n";
                html += "<td class='label-cell'><p>" + item["manufacturer"].toString() + "</p></td>\n";
                html += "</tr>\n";

                // Модель
                html += "<tr>\n";
                html += "<td class='label-cell'><p>" + item["model"].toString() + "</p></td>\n";
                html += "</tr>\n";

                // Серийный номер
                html += "<tr>\n";
                html += "<td class='label-cell'><p>" + item["serial_number"].toString() + "</p></td>\n";
                html += "</tr>\n";

                // Part Number
                html += "<tr>\n";
                html += "<td class='label-cell'><p>" +
                        (item.contains("part_number") ? item["part_number"].toString() : "") +
                        "</p></td>\n";
                html += "</tr>\n";

                // Дата
                html += "<tr>\n";
                html += "<td class='label-cell'><p>" +
                        (item.contains("date") ? item["date"].toString() : "") +
                        "</p></td>\n";
                html += "</tr>\n";

                // Копия
                if (copies > 1) {
                    html += "<tr>\n";
                    html += "<td colspan='2' class='copy-number'><p>Копия №" +
                            QString::number(copy + 1) + "</p></td>\n";
                    html += "</tr>\n";
                }

                html += "</table>\n"; // Закрываем внутреннюю таблицу

                // Добавляем разделитель между копиями, если это не последняя копия
                if (copy < copies - 1) {
                    html += "<hr style='border: 1px dashed #ccc; margin: 1mm 0;'>\n";
                }
            }

            html += "</td>\n"; // Закрываем ячейку основной таблицы
            itemIndex++;
        }

        // Заполняем пустые ячейки, если строка неполная
        for (int col = (itemIndex % labelsPerRow); col < labelsPerRow && itemIndex >= selectedItems.size(); ++col) {
            html += "<td style='width: " + QString::number(100 / labelsPerRow) + "%'>&nbsp;</td>\n";
        }

        html += "</tr>\n";

        // Добавляем пустую строку между рядами этикеток
        if (itemIndex < selectedItems.size()) {
            html += "<tr><td colspan='" + QString::number(labelsPerRow) + "' style='border: none; height: 2mm;'>&nbsp;</td></tr>\n";
        }
    }

    html += "</table>\n";
    html += "</body>\n";
    html += "</html>";

    // Настраиваем принтер
    printer->setFullPage(true);
    printer->setPageMargins(QMarginsF(5, 5, 5, 5), QPageLayout::Millimeter);

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(printer);
}

QString LabelPrintDialog::generateLabelHtml(const QVariantMap &item, int copyNumber)
{
    QString label;

    // Заголовок
    label += "<div class='header'>" + item["material_type"].toString() + "</div>";

    // Производитель и модель
    label += "<div class='info'><b>" + item["manufacturer"].toString() + " " +
             item["model"].toString() + "</b></div>";

    // Серийный номер
    if (!item["serial_number"].toString().isEmpty()) {
        label += "<div class='info'>SN: " + item["serial_number"].toString() + "</div>";
    }

    // Part Number
    if (!item["part_number"].toString().isEmpty()) {
        label += "<div class='info'>PN: " + item["part_number"].toString() + "</div>";
    }

    // ID
    label += "<div class='info'>ID: " + item["id"].toString() + "</div>";

    // Дата поступления
    if (item.contains("arrival_date") && !item["arrival_date"].toString().isEmpty()) {
        QDate date = QDate::fromString(item["arrival_date"].toString(), "yyyy-MM-dd");
        if (date.isValid()) {
            label += "<div class='info'>" + date.toString("dd.MM.yyyy") + "</div>";
        }
    }

    // QR-код
    if (includeQRCheckBox->isChecked()) {
        QString qrData = QString("ID: %1\nSN: %2")
                            .arg(item["id"].toString())
                            .arg(item["serial_number"].toString());

        QString qrBase64 = generateQrBase64(qrData, 80);

        if (!qrBase64.isEmpty()) {
            label += "<div class='qr'>";
            label += "<img src='" + qrBase64 + "'/>";
            label += "</div>";
        }
    }

    if (copyNumber > 1) {
        label += "<div class='info' style='text-align: right;'>#" +
                 QString::number(copyNumber) + "</div>";
    }

    return label;
}

QString LabelPrintDialog::generateLabelsHtml()
{
    // Вспомогательный метод для генерации всего HTML
    QString html;
    // ... аналогично коду из printLabels
    return html;
}

QString LabelPrintDialog::generateQrBase64(const QString &data, int size)
{
    if (!qrGenerator) {
        return "";
    }

    // Используем размер из комбобокса, если он есть
    QComboBox *qrSizeCombo = findChild<QComboBox*>("qrSizeCombo");
    if (qrSizeCombo) {
        size = qrSizeCombo->currentData().toInt();
    }

    // Генерируем QR-код с высоким разрешением
    QImage qrImage = qrGenerator->generateQr(data, size * 2, 2, qrcodegen::QrCode::Ecc::HIGH);

    if (qrImage.isNull()) {
        return "";
    }

    // Масштабируем до нужного размера с высоким качеством
    qrImage = qrImage.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Конвертируем в Base64
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    qrImage.save(&buffer, "PNG", 100); // Максимальное качество

    return QString("data:image/png;base64,%1").arg(QString(byteArray.toBase64()));
}


