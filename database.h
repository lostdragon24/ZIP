#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDate>
#include <QVariantMap>
#include <QPair>

class Database : public QObject
{
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);
    ~Database();

    // Структура для статистики
    struct DashboardStats {
        int totalItems;
        int availableItems;
        int writtenOffItems;
        QMap<QString, int> itemsByType;
        QMap<QString, int> itemsByManufacturer;
        QList<QPair<QString, QString>> recentActivity; // Изменено: QString вместо int для даты
    };

    bool initDatabase();
    bool createTables();

    // Методы для работы с материалами
    bool addMaterialType(const QString &type);
    QStringList getMaterialTypes();
    bool deleteMaterialType(const QString &type);
    bool isMaterialTypeUsed(const QString &type);

    // Методы для работы с производителями
    bool addManufacturer(const QString &manufacturer);
    QStringList getManufacturers();
    bool deleteManufacturer(const QString &manufacturer);
    bool isManufacturerUsed(const QString &manufacturer);

    // Методы для работы с моделями
    bool addModel(const QString &materialType, const QString &manufacturer, const QString &modelName);
    QStringList getModelsByMaterialAndManufacturer(const QString &materialType, const QString &manufacturer);
    QStringList getModelsByMaterial(const QString &materialType);
    bool deleteModel(const QString &materialType, const QString &manufacturer, const QString &modelName);
    bool isModelUsed(const QString &materialType, const QString &manufacturer, const QString &modelName);

    // Методы для работы с записями ЗИП
    bool addInventoryItem(const QString &materialType, const QString &manufacturer, const QString &modelName,
                          const QString &partNumber, const QString &serialNumber, const QString &capacity,
                          const QString &interfaceType, const QString &notes, const QDate &arrivalDate,
                          const QString &invoiceNumber);

    bool updateInventoryItem(int itemId, const QString &materialType, const QString &manufacturer,
                            const QString &modelName, const QString &partNumber, const QString &serialNumber,
                            const QString &capacity, const QString &interfaceType, const QString &notes,
                            const QDate &arrivalDate, const QString &invoiceNumber);

    bool deleteInventoryItem(int itemId);

    QList<QVariantMap> getInventoryItems();
    QVariantMap getInventoryItemById(int itemId);
    QList<QVariantMap> searchInventory(const QString &searchText);

    // Новые методы для фильтрации
    QList<QVariantMap> getFilteredInventory(const QString &materialType = "",
                                            const QString &manufacturer = "",
                                            const QString &model = "",
                                            const QString &partNumber = "",
                                            const QString &serialNumber = "",
                                            const QString &status = "",
                                            const QDate &dateFrom = QDate(),
                                            const QDate &dateTo = QDate());

    // Вспомогательные методы для проверки использования
    int getUsageCountForMaterialType(const QString &materialType);
    int getUsageCountForManufacturer(const QString &manufacturer);
    int getUsageCountForModel(const QString &materialType, const QString &manufacturer, const QString &modelName);
    bool checkInventoryItemExists(int itemId);

    // Методы для списания
    bool markItemAsWrittenOff(int itemId, const QString &issuedTo,
                             const QDate &issueDate, const QString &comments);
    bool markItemAsAvailable(int itemId);
    bool isItemWrittenOff(int itemId);
    QVariantMap getItemStatus(int itemId);
    QList<QVariantMap> getWriteOffHistory(int itemId = -1);

    // Методы для статистики
    DashboardStats getDashboardStats();
    QList<QPair<QDate, int>> getMonthlyStats(int months = 6);

    // Методы для печати этикеток
    QList<QVariantMap> getItemsForLabels(const QList<int> &itemIds);

private:
    QSqlDatabase db;
    QString databasePath;

    // Вспомогательные методы
    int getMaterialTypeId(const QString &materialType);
    int getManufacturerId(const QString &manufacturer);
    int getModelId(const QString &materialType, const QString &manufacturer, const QString &modelName);
    bool recreateTableWithStatus();

    // Методы для работы со структурой БД
    bool updateDatabaseStructure();
    QStringList getTableColumns(const QString &tableName);

};

#endif // DATABASE_H
