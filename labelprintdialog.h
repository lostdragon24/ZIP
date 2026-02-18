#ifndef LABELPRINTDIALOG_H
#define LABELPRINTDIALOG_H

#include <QDialog>
#include <QPrinter>
#include <QList>
#include <QVariantMap>
#include <QCheckBox>
#include <QScopedPointer>  // для умного указателя

// QR-генератор
#include "QrCodeGenerator.h"

class QTableWidget;
class QSpinBox;
class QComboBox;

class LabelPrintDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LabelPrintDialog(const QList<QVariantMap> &items, QWidget *parent = nullptr);
    ~LabelPrintDialog();

private slots:
    void onPrint();
    void onPreview();
    void onSelectAll();
    void onClearAll();

private:
    void setupUI();
    void printLabels(QPrinter *printer);
    QString generateLabelHtml(const QVariantMap &item, int copyNumber = 1);
    QString generateLabelsHtml();

    // Новый метод для генерации QR-кода в формате Base64 для HTML
    QString generateQrBase64(const QString &data, int size = 100);

    QList<QVariantMap> allItems;
    QTableWidget *itemsTable;
    QSpinBox *copiesSpinBox;
    QComboBox *labelSizeCombo;
    QCheckBox *includeQRCheckBox;

    // Генератор QR-кодов
    QScopedPointer<QrCodeGenerator> qrGenerator;
};

#endif // LABELPRINTDIALOG_H
