#ifndef ADVANCEDFILTERDIALOG_H
#define ADVANCEDFILTERDIALOG_H

#include <QDialog>
#include <QDate>

class QComboBox;
class QLineEdit;
class QDateEdit;
class QCheckBox;

class AdvancedFilterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdvancedFilterDialog(QWidget *parent = nullptr);

    struct FilterParams {
        QString materialType;
        QString manufacturer;
        QString model;
        QString partNumber;
        QString serialNumber;
        QString status;
        QDate dateFrom;
        QDate dateTo;
        bool useDateRange;
    };

    FilterParams getFilterParams() const;

private:
    QComboBox *materialTypeCombo;
    QComboBox *manufacturerCombo;
    QComboBox *modelCombo;
    QComboBox *statusCombo;
    QLineEdit *partNumberEdit;
    QLineEdit *serialNumberEdit;
    QDateEdit *dateFromEdit;
    QDateEdit *dateToEdit;
    QCheckBox *useDateRangeCheck;



    void setupUI();
};

#endif // ADVANCEDFILTERDIALOG_H
