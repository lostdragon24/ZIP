#ifndef DASHBOARDWIDGET_H
#define DASHBOARDWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include "database.h"

class DashboardWidget : public QWidget
{
    Q_OBJECT

public:
    // Изменяем конструктор: делаем db параметром по умолчанию nullptr
    explicit DashboardWidget(QWidget *parent = nullptr, Database *db = nullptr);

    // Добавляем метод для установки database после создания
    void setDatabase(Database *db);

    void refreshStats();

private:
    Database *database;
    QVBoxLayout *mainLayout;
    QLabel *totalLabel;
    QLabel *availableLabel;
    QLabel *writtenOffLabel;
    QProgressBar *availableProgress;
    QLabel *statsLabel;

    void setupUI();
    void updateStats();
};

#endif // DASHBOARDWIDGET_H
