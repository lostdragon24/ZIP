#include "dashboardwidget.h"
#include <QGridLayout>
#include <QGroupBox>
#include <QFont>
#include <QFrame>

DashboardWidget::DashboardWidget(QWidget *parent, Database *db)
    : QWidget(parent), database(db)
{
    setupUI();
    if (database) {
      //  refreshStats();
    }
}

void DashboardWidget::setDatabase(Database *db)
{
    database = db;
    refreshStats();
}


void DashboardWidget::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    // Заголовок
    QLabel *title = new QLabel("📊 Статистика склада", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

    // Карточки с основными показателями
    QHBoxLayout *cardsLayout = new QHBoxLayout();

    QFrame *totalCard = new QFrame(this);
    totalCard->setFrameStyle(QFrame::StyledPanel);
    totalCard->setStyleSheet("QFrame { background-color: #f0f0f0; border-radius: 5px; padding: 10px; }");
    QVBoxLayout *totalCardLayout = new QVBoxLayout(totalCard);

    totalLabel = new QLabel("0", totalCard);
    QFont totalFont = totalLabel->font();
    totalFont.setPointSize(24);
    totalFont.setBold(true);
    totalLabel->setFont(totalFont);
    totalLabel->setAlignment(Qt::AlignCenter);
    totalCardLayout->addWidget(totalLabel);

    QLabel *totalTextLabel = new QLabel("Всего позиций", totalCard);
    totalTextLabel->setAlignment(Qt::AlignCenter);
    totalCardLayout->addWidget(totalTextLabel);

    cardsLayout->addWidget(totalCard);

    QFrame *availableCard = new QFrame(this);
    availableCard->setFrameStyle(QFrame::StyledPanel);
    availableCard->setStyleSheet("QFrame { background-color: #e8f5e8; border-radius: 5px; padding: 10px; }");
    QVBoxLayout *availableCardLayout = new QVBoxLayout(availableCard);

    availableLabel = new QLabel("0", availableCard);
    QFont availableFont = availableLabel->font();
    availableFont.setPointSize(24);
    availableFont.setBold(true);
    availableLabel->setFont(availableFont);
    availableLabel->setAlignment(Qt::AlignCenter);
    availableCardLayout->addWidget(availableLabel);

    QLabel *availableTextLabel = new QLabel("В наличии", availableCard);
    availableTextLabel->setAlignment(Qt::AlignCenter);
    availableCardLayout->addWidget(availableTextLabel);

    cardsLayout->addWidget(availableCard);

    QFrame *writtenOffCard = new QFrame(this);
    writtenOffCard->setFrameStyle(QFrame::StyledPanel);
    writtenOffCard->setStyleSheet("QFrame { background-color: #ffe8e8; border-radius: 5px; padding: 10px; }");
    QVBoxLayout *writtenOffCardLayout = new QVBoxLayout(writtenOffCard);

    writtenOffLabel = new QLabel("0", writtenOffCard);
    QFont writtenOffFont = writtenOffLabel->font();
    writtenOffFont.setPointSize(24);
    writtenOffFont.setBold(true);
    writtenOffLabel->setFont(writtenOffFont);
    writtenOffLabel->setAlignment(Qt::AlignCenter);
    writtenOffCardLayout->addWidget(writtenOffLabel);

    QLabel *writtenOffTextLabel = new QLabel("Списано", writtenOffCard);
    writtenOffTextLabel->setAlignment(Qt::AlignCenter);
    writtenOffCardLayout->addWidget(writtenOffTextLabel);

    cardsLayout->addWidget(writtenOffCard);

    mainLayout->addLayout(cardsLayout);

    // Прогресс-бар наличия
    QGroupBox *progressGroup = new QGroupBox("Заполненность склада", this);
    QVBoxLayout *progressLayout = new QVBoxLayout(progressGroup);
    availableProgress = new QProgressBar(this);
    availableProgress->setRange(0, 100);
    availableProgress->setTextVisible(true);
    availableProgress->setStyleSheet(
        "QProgressBar { height: 25px; text-align: center; } "
        "QProgressBar::chunk { background-color: #4CAF50; }"
    );
    progressLayout->addWidget(availableProgress);
    mainLayout->addWidget(progressGroup);

    // Детальная статистика
    QGroupBox *detailsGroup = new QGroupBox("Детальная информация", this);
    QVBoxLayout *detailsLayout = new QVBoxLayout(detailsGroup);
    statsLabel = new QLabel(this);
    statsLabel->setWordWrap(true);
    detailsLayout->addWidget(statsLabel);
    mainLayout->addWidget(detailsGroup);

    mainLayout->addStretch();
}

void DashboardWidget::refreshStats()
{
    updateStats();
}

void DashboardWidget::updateStats()
{
    // Проверяем, что все виджеты существуют
    if (!totalLabel || !availableLabel || !writtenOffLabel ||
        !availableProgress || !statsLabel) {
        qDebug() << "Dashboard widgets not initialized yet";
        return;
    }

    if (!database) {
        qDebug() << "Database not set for DashboardWidget";
        return;
    }

    Database::DashboardStats stats = database->getDashboardStats();

    // Обновляем карточки
    totalLabel->setText(QString::number(stats.totalItems));
    availableLabel->setText(QString::number(stats.availableItems));
    writtenOffLabel->setText(QString::number(stats.writtenOffItems));

    // Обновляем прогресс-бар
    if (stats.totalItems > 0) {
        int percent = (stats.availableItems * 100) / stats.totalItems;
        availableProgress->setValue(percent);
        availableProgress->setFormat(QString("%1% в наличии (%2 из %3)")
                                     .arg(percent)
                                     .arg(stats.availableItems)
                                     .arg(stats.totalItems));
    }

    // Формируем детальную статистику
    QString details;

    if (!stats.itemsByType.isEmpty()) {
        details += "📦 По типам материалов:\n";
        for (auto it = stats.itemsByType.begin(); it != stats.itemsByType.end(); ++it) {
            details += QString("  • %1: %2\n").arg(it.key()).arg(it.value());
        }
        details += "\n";
    }

    if (!stats.itemsByManufacturer.isEmpty()) {
        details += "🏭 По производителям:\n";
        for (auto it = stats.itemsByManufacturer.begin(); it != stats.itemsByManufacturer.end(); ++it) {
            details += QString("  • %1: %2\n").arg(it.key()).arg(it.value());
        }
        details += "\n";
    }

    if (!stats.recentActivity.isEmpty()) {
        details += "🔄 Последние действия:\n";
        for (const auto &activity : stats.recentActivity) {
            details += QString("  • %1 (%2)\n").arg(activity.first).arg(activity.second);
        }
    }

    statsLabel->setText(details);
}

