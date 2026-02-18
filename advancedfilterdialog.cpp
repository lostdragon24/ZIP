#include "advancedfilterdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QDateEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>

AdvancedFilterDialog::AdvancedFilterDialog(QWidget *parent)
    : QDialog(parent),
      materialTypeCombo(nullptr),
      manufacturerCombo(nullptr),
      modelCombo(nullptr),
      statusCombo(nullptr),
      partNumberEdit(nullptr),
      serialNumberEdit(nullptr),
      dateFromEdit(nullptr),
      dateToEdit(nullptr),
      useDateRangeCheck(nullptr)
{
    qDebug() << "AdvancedFilterDialog constructor";
    setupUI();
}

void AdvancedFilterDialog::setupUI()
{
    setWindowTitle("Расширенный фильтр");
    setFixedSize(500, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Основные параметры
    QGroupBox *mainGroup = new QGroupBox("Основные параметры", this);
    QGridLayout *mainLayout_grid = new QGridLayout(mainGroup);

    mainLayout_grid->addWidget(new QLabel("Тип материала:"), 0, 0);
    materialTypeCombo = new QComboBox(this);
    materialTypeCombo->setObjectName("materialTypeCombo");  // Важно!
    materialTypeCombo->setEditable(true);
    materialTypeCombo->addItem("Все типы", "");
    mainLayout_grid->addWidget(materialTypeCombo, 0, 1);

    mainLayout_grid->addWidget(new QLabel("Производитель:"), 1, 0);
    manufacturerCombo = new QComboBox(this);
    manufacturerCombo->setObjectName("manufacturerCombo");  // Важно!
    manufacturerCombo->setEditable(true);
    manufacturerCombo->addItem("Все производители", "");
    mainLayout_grid->addWidget(manufacturerCombo, 1, 1);

    mainLayout_grid->addWidget(new QLabel("Модель:"), 2, 0);
    modelCombo = new QComboBox(this);
    modelCombo->setObjectName("modelCombo");  // Важно!
    modelCombo->setEditable(true);
    modelCombo->addItem("Все модели", "");
    mainLayout_grid->addWidget(modelCombo, 2, 1);

    mainLayout->addWidget(mainGroup);

    // Диапазон дат
    QGroupBox *dateGroup = new QGroupBox("Диапазон дат", this);
    QVBoxLayout *dateLayout = new QVBoxLayout(dateGroup);

    useDateRangeCheck = new QCheckBox("Использовать диапазон дат", this);
    dateLayout->addWidget(useDateRangeCheck);

    QHBoxLayout *dateRangeLayout = new QHBoxLayout();
    dateRangeLayout->addWidget(new QLabel("С:", dateGroup));
    dateFromEdit = new QDateEdit(QDate::currentDate().addMonths(-1), dateGroup);
    dateFromEdit->setCalendarPopup(true);
    dateFromEdit->setEnabled(false);
    dateRangeLayout->addWidget(dateFromEdit);

    dateRangeLayout->addWidget(new QLabel("по:", dateGroup));
    dateToEdit = new QDateEdit(QDate::currentDate(), dateGroup);
    dateToEdit->setCalendarPopup(true);
    dateToEdit->setEnabled(false);
    dateRangeLayout->addWidget(dateToEdit);

    dateLayout->addLayout(dateRangeLayout);

    connect(useDateRangeCheck, &QCheckBox::toggled, dateFromEdit, &QDateEdit::setEnabled);
    connect(useDateRangeCheck, &QCheckBox::toggled, dateToEdit, &QDateEdit::setEnabled);

    mainLayout->addWidget(dateGroup);

    // Кнопки
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QPushButton *clearBtn = new QPushButton("Сбросить фильтр", this);
    buttonBox->addButton(clearBtn, QDialogButtonBox::ActionRole);

    connect(clearBtn, &QPushButton::clicked, [this]() {
        qDebug() << "Clear filter button clicked";

        // Проверяем каждый указатель перед использованием
        if (materialTypeCombo) {
            materialTypeCombo->setCurrentIndex(0);
            qDebug() << "Material combo reset";
        }

        if (manufacturerCombo) {
            manufacturerCombo->setCurrentIndex(0);
            qDebug() << "Manufacturer combo reset";
        }

        if (modelCombo) {
            modelCombo->clear();
            modelCombo->addItem("Все модели", "");
            modelCombo->setCurrentIndex(0);
            qDebug() << "Model combo reset";
        }

        if (partNumberEdit) {
            partNumberEdit->clear();
            qDebug() << "Part number edit cleared";
        }

        if (serialNumberEdit) {
            serialNumberEdit->clear();
            qDebug() << "Serial number edit cleared";
        }

        if (statusCombo) {
            statusCombo->setCurrentIndex(0);
            qDebug() << "Status combo reset";
        }

        if (useDateRangeCheck) {
            useDateRangeCheck->setChecked(false);
            qDebug() << "Date range check reset";
        }
        });

    mainLayout->addWidget(buttonBox);
}

AdvancedFilterDialog::FilterParams AdvancedFilterDialog::getFilterParams() const
{
    FilterParams params;

    // Инициализируем значения по умолчанию
    params.materialType = "";
    params.manufacturer = "";
    params.model = "";
    params.partNumber = "";
    params.serialNumber = "";
    params.status = "all";
    params.useDateRange = false;

    qDebug() << "=== getFilterParams ===";

    // Проверяем каждый указатель перед использованием
    if (materialTypeCombo) {
        if (materialTypeCombo->currentIndex() == 0) {
            params.materialType = "";
            qDebug() << "Material: All types";
        } else {
            params.materialType = materialTypeCombo->currentText();
            qDebug() << "Material:" << params.materialType;
        }
    } else {
        qDebug() << "ERROR: materialTypeCombo is null!";
    }

    if (manufacturerCombo) {
        if (manufacturerCombo->currentIndex() == 0) {
            params.manufacturer = "";
            qDebug() << "Manufacturer: All manufacturers";
        } else {
            params.manufacturer = manufacturerCombo->currentText();
            qDebug() << "Manufacturer:" << params.manufacturer;
        }
    } else {
        qDebug() << "ERROR: manufacturerCombo is null!";
    }

    if (modelCombo) {
        if (modelCombo->currentIndex() == 0) {
            params.model = "";
            qDebug() << "Model: All models";
        } else {
            params.model = modelCombo->currentText();
            qDebug() << "Model:" << params.model;
        }
    } else {
        qDebug() << "ERROR: modelCombo is null!";
    }

    if (partNumberEdit) {
        params.partNumber = partNumberEdit->text();
        qDebug() << "Part number:" << params.partNumber;
    } else {
        qDebug() << "ERROR: partNumberEdit is null!";
    }

    if (serialNumberEdit) {
        params.serialNumber = serialNumberEdit->text();
        qDebug() << "Serial number:" << params.serialNumber;
    } else {
        qDebug() << "ERROR: serialNumberEdit is null!";
    }

    if (statusCombo) {
        params.status = statusCombo->currentData().toString();
        qDebug() << "Status:" << params.status;
    } else {
        qDebug() << "ERROR: statusCombo is null!";
    }

    if (useDateRangeCheck) {
        params.useDateRange = useDateRangeCheck->isChecked();
        qDebug() << "Use date range:" << params.useDateRange;
    } else {
        qDebug() << "ERROR: useDateRangeCheck is null!";
    }

    if (dateFromEdit) {
        params.dateFrom = dateFromEdit->date();
        qDebug() << "Date from:" << params.dateFrom.toString("dd.MM.yyyy");
    } else {
        qDebug() << "ERROR: dateFromEdit is null!";
    }

    if (dateToEdit) {
        params.dateTo = dateToEdit->date();
        qDebug() << "Date to:" << params.dateTo.toString("dd.MM.yyyy");
    } else {
        qDebug() << "ERROR: dateToEdit is null!";
    }

    return params;
}
