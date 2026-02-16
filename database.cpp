#include "database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDate>
#include <QSqlRecord>

Database::Database(QObject *parent) : QObject(parent)
{
    databasePath = QDir::currentPath() + "/zip_inventory.db";
}

Database::~Database()
{
    if (db.isOpen()) {
        db.close();
    }
}

bool Database::initDatabase()
{
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        db = QSqlDatabase::addDatabase("QSQLITE");
    #else
        db = QSqlDatabase::addDatabase("QSQLITE", "zip_connection");
    #endif

    db.setDatabaseName(databasePath);

    if (!db.open()) {
        qDebug() << "Error opening database:" << db.lastError().text();
        return false;
    }

    // Проверяем существование таблиц
    QStringList tables = db.tables();
    qDebug() << "Database tables:" << tables;

    // Проверяем версию базы данных или создаем таблицы
    if (!tables.contains("inventory")) {
        // Новая база данных
        return createTables();
    } else {
        // Существующая база данных
        qDebug() << "Using existing database, checking structure...";

        // Проверяем и обновляем структуру таблиц
        return updateDatabaseStructure();
    }
}

bool Database::updateDatabaseStructure()
{
    QSqlQuery query;

    qDebug() << "=== Updating database structure ===";

    // Проверяем наличие таблицы write_off_history
    query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='write_off_history'");
    if (!query.next()) {
        qDebug() << "Creating write_off_history table...";
        QString createWriteOffHistoryTable =
            "CREATE TABLE IF NOT EXISTS write_off_history ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "inventory_id INTEGER NOT NULL,"
            "issued_to TEXT NOT NULL,"
            "issue_date DATE NOT NULL,"
            "comments TEXT,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "FOREIGN KEY (inventory_id) REFERENCES inventory(id)"
            ")";

        if (!query.exec(createWriteOffHistoryTable)) {
            qDebug() << "Failed to create write_off_history table:" << query.lastError().text();
        } else {
            qDebug() << "write_off_history table created successfully";
        }
    }

    // Проверяем наличие колонки status
    QStringList columns = getTableColumns("inventory");
    if (!columns.contains("status")) {
        qDebug() << "Adding status column to inventory...";
        if (!query.exec("ALTER TABLE inventory ADD COLUMN status TEXT DEFAULT 'available'")) {
            qDebug() << "Failed to add status column:" << query.lastError().text();

            // Пытаемся пересоздать таблицу
            qDebug() << "Attempting to recreate table...";
            if (recreateTableWithStatus()) {
                qDebug() << "Table recreated successfully with status column";
            }
        } else {
            qDebug() << "Status column added successfully";
        }
    } else {
        qDebug() << "Status column already exists";
    }

    // Создаем индекс для статуса
    query.exec("CREATE INDEX IF NOT EXISTS idx_inventory_status ON inventory(status)");

    qDebug() << "=== Updating database structure ===";

    // Сначала проверим структуру таблицы inventory
   // QStringList columns = getTableColumns("inventory");
   // qDebug() << "Current inventory columns:" << columns;

    // Проверяем существование ограничения UNIQUE на serial_number
    // Для этого нужно пересоздать таблицу без ограничения UNIQUE
    query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='inventory'");
    if (query.next()) {
        // Получаем полный SQL создания таблицы
        query.exec("SELECT sql FROM sqlite_master WHERE type='table' AND name='inventory'");
        if (query.next()) {
            QString createSql = query.value(0).toString();
            qDebug() << "Original CREATE TABLE SQL:" << createSql;

            // Проверяем наличие UNIQUE в определении
            if (createSql.contains("serial_number TEXT UNIQUE") ||
                createSql.contains("serial_number TEXT UNIQUE NOT NULL") ||
                createSql.contains("serial_number TEXT NOT NULL UNIQUE")) {

                qDebug() << "Found UNIQUE constraint on serial_number, need to recreate table...";

                // 1. Создаем временную таблицу с данными
                query.exec("CREATE TABLE IF NOT EXISTS inventory_backup AS SELECT * FROM inventory");

                // 2. Удаляем старую таблицу
                query.exec("DROP TABLE inventory");

                // 3. Создаем новую таблицу без ограничения UNIQUE
                QString newCreateSql = createSql;
                newCreateSql.replace("serial_number TEXT UNIQUE", "serial_number TEXT");
                newCreateSql.replace("serial_number TEXT UNIQUE NOT NULL", "serial_number TEXT");
                newCreateSql.replace("serial_number TEXT NOT NULL UNIQUE", "serial_number TEXT");

                qDebug() << "New CREATE TABLE SQL:" << newCreateSql;

                if (!query.exec(newCreateSql)) {
                    qDebug() << "Failed to recreate table:" << query.lastError().text();

                    // Пытаемся восстановить из backup
                    query.exec("CREATE TABLE inventory AS SELECT * FROM inventory_backup");
                    query.exec("DROP TABLE inventory_backup");
                    return false;
                }

                // 4. Восстанавливаем данные
                query.exec("INSERT INTO inventory SELECT * FROM inventory_backup");

                // 5. Удаляем временную таблицу
                query.exec("DROP TABLE inventory_backup");

                qDebug() << "Table recreated successfully without UNIQUE constraint";

                // 6. Пересоздаем индексы
                query.exec("CREATE INDEX IF NOT EXISTS idx_inventory_serial ON inventory(serial_number)");
                query.exec("CREATE INDEX IF NOT EXISTS idx_inventory_part_number ON inventory(part_number)");
                query.exec("CREATE INDEX IF NOT EXISTS idx_inventory_capacity ON inventory(capacity)");
                query.exec("CREATE INDEX IF NOT EXISTS idx_inventory_arrival_date ON inventory(arrival_date)");

                // 7. Пересоздаем триггер
                query.exec("DROP TRIGGER IF EXISTS update_inventory_timestamp");
                query.exec("CREATE TRIGGER update_inventory_timestamp "
                           "AFTER UPDATE ON inventory "
                           "BEGIN "
                           "UPDATE inventory SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id; "
                           "END;");

                qDebug() << "All indexes and triggers recreated";
            } else {
                qDebug() << "No UNIQUE constraint found on serial_number";
            }
        }
    }

    // Проверяем и добавляем отсутствующие колонки
    columns = getTableColumns("inventory");
    qDebug() << "Updated inventory columns:" << columns;

    // Добавляем created_at если отсутствует
    if (!columns.contains("created_at")) {
        qDebug() << "Adding created_at column to inventory...";
        if (!query.exec("ALTER TABLE inventory ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP")) {
            qDebug() << "Failed to add created_at:" << query.lastError().text();
        }
    }

    // Добавляем updated_at если отсутствует
    if (!columns.contains("updated_at")) {
        qDebug() << "Adding updated_at column to inventory...";
        if (!query.exec("ALTER TABLE inventory ADD COLUMN updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP")) {
            qDebug() << "Failed to add updated_at:" << query.lastError().text();
        }
    }

    // Проверяем наличие колонки capacity (добавлена ранее)
    if (!columns.contains("capacity")) {
        qDebug() << "Adding capacity column to inventory...";
        query.exec("ALTER TABLE inventory ADD COLUMN capacity TEXT");
    }

    // Проверяем триггер для updated_at
    query.exec("SELECT name FROM sqlite_master WHERE type='trigger' AND name='update_inventory_timestamp'");
    if (!query.next()) {
        qDebug() << "Creating update trigger...";
        query.exec("CREATE TRIGGER update_inventory_timestamp "
                   "AFTER UPDATE ON inventory "
                   "BEGIN "
                   "UPDATE inventory SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id; "
                   "END;");
    }

    return true;
}

bool Database::recreateTableWithStatus()
{
    QSqlQuery query;

    // Создаем временную таблицу
    if (!query.exec("CREATE TABLE IF NOT EXISTS inventory_backup AS SELECT * FROM inventory")) {
        qDebug() << "Failed to create backup table:" << query.lastError().text();
        return false;
    }

    // Получаем структуру таблицы
    query.exec("SELECT sql FROM sqlite_master WHERE type='table' AND name='inventory'");
    if (!query.next()) {
        qDebug() << "Failed to get table structure";
        return false;
    }

    QString createSql = query.value(0).toString();

    // Добавляем колонку status
    if (!createSql.contains("status TEXT")) {
        // Находим позицию для вставки (перед FOREIGN KEY)
        int lastParen = createSql.lastIndexOf(")");
        if (lastParen == -1) {
            qDebug() << "Invalid CREATE TABLE statement";
            return false;
        }

        QString newCreateSql = createSql.left(lastParen) +
                              ", status TEXT DEFAULT 'available'" +
                              createSql.mid(lastParen);

        // Удаляем старую таблицу
        query.exec("DROP TABLE inventory");

        // Создаем новую таблицу
        if (!query.exec(newCreateSql)) {
            qDebug() << "Failed to recreate table:" << query.lastError().text();
            return false;
        }

        // Восстанавливаем данные
        QStringList columns = getTableColumns("inventory_backup");
        QString insertColumns = columns.join(", ");
        QString selectColumns = columns.join(", ");

        QString insertSql = QString("INSERT INTO inventory (%1) SELECT %2 FROM inventory_backup")
                           .arg(insertColumns)
                           .arg(selectColumns);

        if (!query.exec(insertSql)) {
            qDebug() << "Failed to restore data:" << query.lastError().text();
            return false;
        }
    }

    // Удаляем временную таблицу
    query.exec("DROP TABLE inventory_backup");

    return true;
}

QStringList Database::getTableColumns(const QString &tableName)
{
    QStringList columns;
    QSqlQuery query;
    query.prepare("PRAGMA table_info(" + tableName + ")");

    if (query.exec()) {
        while (query.next()) {
            columns.append(query.value(1).toString()); // column name is at index 1
        }
    }

    return columns;
}


bool Database::updateInventoryItem(int itemId, const QString &materialType, const QString &manufacturer,
                                  const QString &modelName, const QString &partNumber, const QString &serialNumber,
                                  const QString &capacity, const QString &interfaceType, const QString &notes,
                                  const QDate &arrivalDate, const QString &invoiceNumber)
{
    qDebug() << "=== updateInventoryItem called ===";
    qDebug() << "Item ID:" << itemId;
    qDebug() << "Material Type:" << materialType;
    qDebug() << "Manufacturer:" << manufacturer;
    qDebug() << "Model:" << modelName;
    qDebug() << "Part Number:" << partNumber;
    qDebug() << "Serial:" << serialNumber;
    qDebug() << "Capacity:" << capacity;
    qDebug() << "Interface:" << interfaceType;
    qDebug() << "Arrival Date:" << arrivalDate.toString("yyyy-MM-dd");
    qDebug() << "Invoice:" << invoiceNumber;

    if (itemId <= 0 || materialType.isEmpty() || manufacturer.isEmpty() || modelName.isEmpty()) {
        qDebug() << "Validation failed!";
        qDebug() << "itemId:" << (itemId <= 0 ? "invalid" : "valid");
        qDebug() << "materialType empty:" << materialType.isEmpty();
        qDebug() << "manufacturer empty:" << manufacturer.isEmpty();
        qDebug() << "modelName empty:" << modelName.isEmpty();
        return false;
    }

    int materialId = getMaterialTypeId(materialType);
    int manufacturerId = getManufacturerId(manufacturer);
    int modelId = getModelId(materialType, manufacturer, modelName);

    qDebug() << "Material ID:" << materialId;
    qDebug() << "Manufacturer ID:" << manufacturerId;
    qDebug() << "Model ID:" << modelId;

    if (materialId == -1 || manufacturerId == -1 || modelId == -1) {
        qDebug() << "Failed to get IDs!";
        return false;
    }

    QString finalSerialNumber = serialNumber.trimmed();
    bool hasSerialNumber = !finalSerialNumber.isEmpty();

    qDebug() << "Has serial number:" << hasSerialNumber;
    qDebug() << "Final serial number:" << finalSerialNumber;

    // Если есть серийный номер, проверяем, не используется ли он другим элементом
    if (hasSerialNumber) {
        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT COUNT(*) FROM inventory WHERE serial_number = ? AND id != ?");
        checkQuery.addBindValue(finalSerialNumber);
        checkQuery.addBindValue(itemId);
        if (checkQuery.exec() && checkQuery.next()) {
            int count = checkQuery.value(0).toInt();
            if (count > 0) {
                qDebug() << "Serial number" << finalSerialNumber << "already used by another item";
                return false;
            }
        }
    }

    QStringList columns = getTableColumns("inventory");
    bool hasCapacity = columns.contains("capacity");

    qDebug() << "Has capacity column:" << hasCapacity;

    // Строим SQL запрос
    QString sql = "UPDATE inventory SET ";
    QVariantList bindValues;

    // Добавляем поля в SQL и значения в список
    sql += "material_type_id = ?, ";
    bindValues << materialId;

    sql += "manufacturer_id = ?, ";
    bindValues << manufacturerId;

    sql += "model_id = ?, ";
    bindValues << modelId;

    sql += "part_number = ?, ";
    bindValues << partNumber.trimmed();

    if (hasSerialNumber) {
        sql += "serial_number = ?, ";
        bindValues << finalSerialNumber;
    } else {
        sql += "serial_number = NULL, ";
    }

    if (hasCapacity) {
        sql += "capacity = ?, ";
        bindValues << capacity.trimmed();
    }

    sql += "interface_type = ?, ";
    bindValues << interfaceType.trimmed();

    sql += "notes = ?, ";
    bindValues << notes.trimmed();

    sql += "arrival_date = ?, ";
    bindValues << arrivalDate.toString("yyyy-MM-dd");

    sql += "invoice_number = ? ";
    bindValues << invoiceNumber.trimmed();

    sql += "WHERE id = ?";
    bindValues << itemId;

    qDebug() << "SQL:" << sql;
    qDebug() << "Bind values count:" << bindValues.count();

    QSqlQuery query;
    if (!query.prepare(sql)) {
        qDebug() << "Failed to prepare query:" << query.lastError().text();
        return false;
    }

    // Привязываем значения
    for (int i = 0; i < bindValues.count(); ++i) {
        query.bindValue(i, bindValues[i]);
    }

    bool success = query.exec();

    if (!success) {
        qDebug() << "Query execution failed:" << query.lastError().text();
        qDebug() << "Last query:" << query.lastQuery();

        // Выводим детали привязки значений
        for (int i = 0; i < bindValues.count(); ++i) {
            qDebug() << "  Value" << i << ":" << bindValues[i].toString();
        }
    } else {
        qDebug() << "Update successful, affected rows:" << query.numRowsAffected();
    }

    return success;
}


bool Database::createTables()
{
    QSqlQuery query;

    // Таблица типов материалов
    QString createMaterialTypesTable =
        "CREATE TABLE IF NOT EXISTS material_types ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT UNIQUE NOT NULL"
        ")";

    if (!query.exec(createMaterialTypesTable)) {
        qDebug() << "Error creating material_types table:" << query.lastError().text();
        return false;
    }

    // Таблица производителей
    QString createManufacturersTable =
        "CREATE TABLE IF NOT EXISTS manufacturers ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT UNIQUE NOT NULL"
        ")";

    if (!query.exec(createManufacturersTable)) {
        qDebug() << "Error creating manufacturers table:" << query.lastError().text();
        return false;
    }

    // Таблица моделей
    QString createModelsTable =
        "CREATE TABLE IF NOT EXISTS models ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "material_type_id INTEGER NOT NULL,"
        "manufacturer_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "FOREIGN KEY (material_type_id) REFERENCES material_types(id),"
        "FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id),"
        "UNIQUE(material_type_id, manufacturer_id, name)"
        ")";

    if (!query.exec(createModelsTable)) {
        qDebug() << "Error creating models table:" << query.lastError().text();
        return false;
    }

    // Таблица записей ЗИП - ИЗМЕНЕНО: убрали UNIQUE с serial_number
    QString createInventoryTable =
        "CREATE TABLE IF NOT EXISTS inventory ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "material_type_id INTEGER NOT NULL,"
        "manufacturer_id INTEGER NOT NULL,"
        "model_id INTEGER NOT NULL,"
        "part_number TEXT,"
        "serial_number TEXT,"  // Убрали UNIQUE отсюда
        "capacity TEXT,"
        "interface_type TEXT,"
        "notes TEXT,"
        "arrival_date DATE NOT NULL,"
        "invoice_number TEXT,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY (material_type_id) REFERENCES material_types(id),"
        "FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id),"
        "FOREIGN KEY (model_id) REFERENCES models(id)"
        ")";

    if (!query.exec(createInventoryTable)) {
        qDebug() << "Error creating inventory table:" << query.lastError().text();
        return false;
    }

    // Триггер для обновления updated_at
    query.exec("CREATE TRIGGER IF NOT EXISTS update_inventory_timestamp "
               "AFTER UPDATE ON inventory "
               "BEGIN "
               "UPDATE inventory SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id; "
               "END;");

    // Индексы
    query.exec("CREATE INDEX IF NOT EXISTS idx_inventory_serial ON inventory(serial_number)"); // Это обычный индекс, не UNIQUE
    query.exec("CREATE INDEX IF NOT EXISTS idx_inventory_part_number ON inventory(part_number)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_inventory_capacity ON inventory(capacity)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_inventory_arrival_date ON inventory(arrival_date)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_models_composite ON models(material_type_id, manufacturer_id)");


    // Таблица истории списаний
        QString createWriteOffHistoryTable =
            "CREATE TABLE IF NOT EXISTS write_off_history ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "inventory_id INTEGER NOT NULL,"
            "issued_to TEXT NOT NULL,"
            "issue_date DATE NOT NULL,"
            "comments TEXT,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "FOREIGN KEY (inventory_id) REFERENCES inventory(id)"
            ")";

        if (!query.exec(createWriteOffHistoryTable)) {
            qDebug() << "Error creating write_off_history table:" << query.lastError().text();
            return false;
        }

        // Добавляем колонку для статуса в таблицу inventory
        QStringList columns = getTableColumns("inventory");

        if (!columns.contains("status")) {
            qDebug() << "Adding status column to inventory...";
            if (!query.exec("ALTER TABLE inventory ADD COLUMN status TEXT DEFAULT 'available'")) {
                qDebug() << "Failed to add status column:" << query.lastError().text();
            }
        }




    // Проверяем, нужно ли добавлять стандартные данные
    query.exec("SELECT COUNT(*) FROM material_types");
    if (query.next() && query.value(0).toInt() == 0) {
        // Добавляем стандартные данные только если таблицы пустые
        query.exec("INSERT OR IGNORE INTO material_types (name) VALUES "
                   "('Блок питания'), ('Жесткий диск'), ('SSD накопитель'), ('Оперативная память'), "
                   "('Материнская плата'), ('Процессор'), ('Видеокарта'), "
                   "('Кулер'), ('Корпус'), ('Монитор'), ('Клавиатура'), ('Мышь'), "
                   "('Сетевая карта'), ('Звуковая карта'), ('Источник бесперебойного питания'), "
                   "('Кабель'), ('Разветвитель'), ('Контроллер'), ('Адаптер'), "
                   "('Оптический привод'), ('Вентилятор'), ('Термопаста')");
    }

    query.exec("SELECT COUNT(*) FROM manufacturers");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("INSERT OR IGNORE INTO manufacturers (name) VALUES "
                   "('Intel'), ('AMD'), ('ASUS'), ('Gigabyte'), ('MSI'), ('ASRock'), "
                   "('Seagate'), ('Western Digital'), ('Toshiba'), ('Hitachi'), "
                   "('Kingston'), ('Crucial'), ('Samsung'), ('Corsair'), ('ADATA'), "
                   "('Thermaltake'), ('Cooler Master'), ('Deepcool'), ('Noctua'), "
                   "('Acer'), ('Dell'), ('HP'), ('Lenovo'), ('BenQ'), "
                   "('Logitech'), ('Microsoft'), ('Razer'), ('SteelSeries'), "
                   "('D-Link'), ('TP-Link'), ('Cisco'), ('Synology'), ('QNAP')");
    }

    return true;
}

// Вспомогательные методы
int Database::getMaterialTypeId(const QString &materialType)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM material_types WHERE name = ?");
    query.addBindValue(materialType);

    if (query.exec() && query.next()) {
        int id = query.value(0).toInt();
        qDebug() << "Found material type ID:" << id << "for" << materialType;
        return id;
    } else {
        qDebug() << "Material type not found:" << materialType;
        qDebug() << "Error:" << query.lastError().text();
        return -1;
    }
}


int Database::getManufacturerId(const QString &manufacturer)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM manufacturers WHERE name = ?");
    query.addBindValue(manufacturer);

    if (query.exec() && query.next()) {
        int id = query.value(0).toInt();
        qDebug() << "Found manufacturer ID:" << id << "for" << manufacturer;
        return id;
    } else {
        qDebug() << "Manufacturer not found:" << manufacturer;
        qDebug() << "Error:" << query.lastError().text();
        return -1;
    }
}

int Database::getModelId(const QString &materialType, const QString &manufacturer, const QString &modelName)
{
    int materialId = getMaterialTypeId(materialType);
    int manufacturerId = getManufacturerId(manufacturer);

    if (materialId == -1 || manufacturerId == -1) {
        qDebug() << "Can't get model ID - missing material or manufacturer";
        return -1;
    }

    QSqlQuery query;
    query.prepare("SELECT id FROM models WHERE material_type_id = ? AND manufacturer_id = ? AND name = ?");
    query.addBindValue(materialId);
    query.addBindValue(manufacturerId);
    query.addBindValue(modelName);

    if (query.exec() && query.next()) {
        int id = query.value(0).toInt();
        qDebug() << "Found model ID:" << id << "for" << modelName;
        return id;
    } else {
        qDebug() << "Model not found:" << modelName
                 << "for material:" << materialType
                 << "manufacturer:" << manufacturer;
        qDebug() << "Error:" << query.lastError().text();

        // Попробуем добавить модель, если её нет
        qDebug() << "Attempting to add model...";
        if (addModel(materialType, manufacturer, modelName)) {
            // Попробуем получить ID снова
            query.exec();
            if (query.next()) {
                int id = query.value(0).toInt();
                qDebug() << "Model added successfully, ID:" << id;
                return id;
            }
        }

        return -1;
    }
}

bool Database::addMaterialType(const QString &type)
{
    if (type.isEmpty()) return false;

    QSqlQuery query;
    query.prepare("INSERT OR IGNORE INTO material_types (name) VALUES (?)");
    query.addBindValue(type.trimmed());

    return query.exec();
}

QStringList Database::getMaterialTypes()
{
    QStringList types;
    QSqlQuery query("SELECT name FROM material_types ORDER BY name");

    while (query.next()) {
        types.append(query.value(0).toString());
    }

    return types;
}

bool Database::addManufacturer(const QString &manufacturer)
{
    if (manufacturer.isEmpty()) return false;

    QSqlQuery query;
    query.prepare("INSERT OR IGNORE INTO manufacturers (name) VALUES (?)");
    query.addBindValue(manufacturer.trimmed());

    return query.exec();
}

QStringList Database::getManufacturers()
{
    QStringList manufacturers;
    QSqlQuery query("SELECT name FROM manufacturers ORDER BY name");

    while (query.next()) {
        manufacturers.append(query.value(0).toString());
    }

    return manufacturers;
}

bool Database::addModel(const QString &materialType, const QString &manufacturer, const QString &modelName)
{
    qDebug() << "Adding model:" << modelName << "for" << materialType << "/" << manufacturer;

    if (materialType.isEmpty() || manufacturer.isEmpty() || modelName.isEmpty()) {
        qDebug() << "Cannot add model - empty parameters";
        return false;
    }

    int materialId = getMaterialTypeId(materialType);
    int manufacturerId = getManufacturerId(manufacturer);

    if (materialId == -1 || manufacturerId == -1) {
        qDebug() << "Cannot add model - invalid material or manufacturer ID";
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT OR IGNORE INTO models (material_type_id, manufacturer_id, name) VALUES (?, ?, ?)");
    query.addBindValue(materialId);
    query.addBindValue(manufacturerId);
    query.addBindValue(modelName.trimmed());

    bool success = query.exec();

    if (!success) {
        qDebug() << "Failed to add model:" << query.lastError().text();
    } else {
        qDebug() << "Model added successfully";
        if (query.numRowsAffected() > 0) {
            qDebug() << "New row inserted";
        } else {
            qDebug() << "Model already exists (IGNORE)";
        }
    }

    return success;
}

bool Database::checkInventoryItemExists(int itemId)
{
    if (itemId <= 0) return false;

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM inventory WHERE id = ?");
    query.addBindValue(itemId);

    if (query.exec() && query.next()) {
        bool exists = query.value(0).toInt() > 0;
        qDebug() << "Item" << itemId << "exists:" << exists;
        return exists;
    }

    return false;
}


QStringList Database::getModelsByMaterialAndManufacturer(const QString &materialType, const QString &manufacturer)
{
    QStringList models;

    int materialId = getMaterialTypeId(materialType);
    int manufacturerId = getManufacturerId(manufacturer);

    if (materialId == -1 || manufacturerId == -1) {
        return models;
    }

    QSqlQuery query;
    query.prepare(
        "SELECT name FROM models WHERE material_type_id = ? AND manufacturer_id = ? ORDER BY name"
    );
    query.addBindValue(materialId);
    query.addBindValue(manufacturerId);

    if (query.exec()) {
        while (query.next()) {
            models.append(query.value(0).toString());
        }
    }

    return models;
}

QStringList Database::getModelsByMaterial(const QString &materialType)
{
    QStringList models;

    int materialId = getMaterialTypeId(materialType);
    if (materialId == -1) return models;

    QSqlQuery query;
    query.prepare(
        "SELECT DISTINCT name FROM models WHERE material_type_id = ? ORDER BY name"
    );
    query.addBindValue(materialId);

    if (query.exec()) {
        while (query.next()) {
            models.append(query.value(0).toString());
        }
    }

    return models;
}

bool Database::addInventoryItem(const QString &materialType, const QString &manufacturer, const QString &modelName,
                               const QString &partNumber, const QString &serialNumber, const QString &capacity,
                               const QString &interfaceType, const QString &notes, const QDate &arrivalDate,
                               const QString &invoiceNumber)
{
    if (materialType.isEmpty() || manufacturer.isEmpty() || modelName.isEmpty()) {
        qDebug() << "Validation failed: materialType, manufacturer, or modelName is empty";
        return false;
    }

    int materialId = getMaterialTypeId(materialType);
    int manufacturerId = getManufacturerId(manufacturer);
    int modelId = getModelId(materialType, manufacturer, modelName);

    if (materialId == -1 || manufacturerId == -1 || modelId == -1) {
        qDebug() << "Failed to get IDs for item";
        return false;
    }

    // Если серийный номер пустой, используем NULL
    QString finalSerialNumber = serialNumber.trimmed();
    bool hasSerialNumber = !finalSerialNumber.isEmpty();

    QSqlQuery query;

    if (hasSerialNumber) {
        // Проверяем, нет ли уже такого серийного номера
        query.prepare("SELECT COUNT(*) FROM inventory WHERE serial_number = ?");
        query.addBindValue(finalSerialNumber);
        if (query.exec() && query.next()) {
            int count = query.value(0).toInt();
            if (count > 0) {
                qDebug() << "Serial number" << finalSerialNumber << "already exists in database";
                // Можно либо вернуть false, либо добавить с другим номером
                // Для простоты возвращаем false
                return false;
            }
        }

        query.prepare(
            "INSERT INTO inventory (material_type_id, manufacturer_id, model_id, "
            "part_number, serial_number, capacity, interface_type, notes, arrival_date, invoice_number) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );
        query.addBindValue(materialId);
        query.addBindValue(manufacturerId);
        query.addBindValue(modelId);
        query.addBindValue(partNumber.trimmed());
        query.addBindValue(finalSerialNumber); // Используем серийный номер
        query.addBindValue(capacity.trimmed());
        query.addBindValue(interfaceType.trimmed());
        query.addBindValue(notes.trimmed());
        query.addBindValue(arrivalDate.toString("yyyy-MM-dd"));
        query.addBindValue(invoiceNumber.trimmed());
    } else {
        // Без серийного номера
        query.prepare(
            "INSERT INTO inventory (material_type_id, manufacturer_id, model_id, "
            "part_number, serial_number, capacity, interface_type, notes, arrival_date, invoice_number) "
            "VALUES (?, ?, ?, ?, NULL, ?, ?, ?, ?, ?)"
        );
        query.addBindValue(materialId);
        query.addBindValue(manufacturerId);
        query.addBindValue(modelId);
        query.addBindValue(partNumber.trimmed());
        // serial_number = NULL
        query.addBindValue(capacity.trimmed());
        query.addBindValue(interfaceType.trimmed());
        query.addBindValue(notes.trimmed());
        query.addBindValue(arrivalDate.toString("yyyy-MM-dd"));
        query.addBindValue(invoiceNumber.trimmed());
    }

    bool success = query.exec();

    if (!success) {
        qDebug() << "Failed to add inventory item:" << query.lastError().text();
    } else {
        qDebug() << "Inventory item added successfully, ID:" << query.lastInsertId().toInt();
    }

    return success;
}

bool Database::deleteInventoryItem(int itemId)
{
    if (itemId <= 0) return false;

    QSqlQuery query;
    query.prepare("DELETE FROM inventory WHERE id = ?");
    query.addBindValue(itemId);

    return query.exec();
}

QVariantMap Database::getInventoryItemById(int itemId)
{
    QVariantMap item;

    if (itemId <= 0) return item;

    QStringList columns = getTableColumns("inventory");

    QString sql =
        "SELECT i.id, mt.name as material_type, man.name as manufacturer, "
        "m.name as model, i.part_number, i.serial_number, ";

    if (columns.contains("capacity")) {
        sql += "i.capacity, ";
    } else {
        sql += "'' as capacity, ";
    }

    sql +=
        "i.interface_type, i.notes, i.arrival_date, i.invoice_number ";

    if (columns.contains("created_at")) {
        sql += ", i.created_at ";
    }

    if (columns.contains("updated_at")) {
        sql += ", i.updated_at ";
    }

    sql +=
        "FROM inventory i "
        "JOIN models m ON i.model_id = m.id "
        "JOIN material_types mt ON i.material_type_id = mt.id "
        "JOIN manufacturers man ON i.manufacturer_id = man.id "
        "WHERE i.id = ?";

    QSqlQuery query;
    query.prepare(sql);
    query.addBindValue(itemId);

    if (query.exec() && query.next()) {
        item["id"] = query.value("id");
        item["material_type"] = query.value("material_type");
        item["manufacturer"] = query.value("manufacturer");
        item["model"] = query.value("model");
        item["part_number"] = query.value("part_number");

        // Обрабатываем NULL для серийного номера
        QVariant serialValue = query.value("serial_number");
        if (serialValue.isNull() || serialValue.toString().isEmpty()) {
            item["serial_number"] = "";
        } else {
            item["serial_number"] = serialValue.toString();
        }

        item["capacity"] = query.value("capacity");
        item["interface_type"] = query.value("interface_type");
        item["notes"] = query.value("notes");
        item["arrival_date"] = query.value("arrival_date");
        item["invoice_number"] = query.value("invoice_number");

        if (columns.contains("created_at")) {
            item["created_at"] = query.value("created_at");
        }

        if (columns.contains("updated_at")) {
            item["updated_at"] = query.value("updated_at");
        }
    }

    return item;
}

QList<QVariantMap> Database::getInventoryItems()
{
    QList<QVariantMap> items;

    qDebug() << "=== Getting inventory items ===";

    QStringList columns = getTableColumns("inventory");
    qDebug() << "Available columns in inventory:" << columns;

    // Проверяем наличие колонки status
    bool hasStatusColumn = columns.contains("status");

    // Строим запрос динамически
    QString sql =
        "SELECT "
        "i.id, ";

    // Добавляем статус, если есть колонка
    if (hasStatusColumn) {
        sql += "COALESCE(i.status, 'available') as status, ";
    } else {
        sql += "'available' as status, ";
    }

    sql +=
        "COALESCE(mt.name, 'Неизвестно') as material_type, "
        "COALESCE(man.name, 'Неизвестно') as manufacturer, "
        "COALESCE(m.name, 'Неизвестно') as model, "
        "i.part_number, "
        "i.serial_number, ";

    // Добавляем capacity
    if (columns.contains("capacity")) {
        sql += "i.capacity, ";
    } else {
        sql += "'' as capacity, ";
    }

    sql +=
        "i.interface_type, "
        "i.notes, "
        "i.arrival_date, "
        "i.invoice_number ";

    // Добавляем created_at и updated_at если существуют
    if (columns.contains("created_at")) {
        sql += ", i.created_at ";
    }

    if (columns.contains("updated_at")) {
        sql += ", i.updated_at ";
    }

    sql +=
        "FROM inventory i "
        "LEFT JOIN material_types mt ON i.material_type_id = mt.id "
        "LEFT JOIN manufacturers man ON i.manufacturer_id = man.id "
        "LEFT JOIN models m ON i.model_id = m.id "
        "ORDER BY i.arrival_date DESC, i.id DESC";

    QSqlQuery query;
    bool success = query.exec(sql);

    if (!success) {
        qDebug() << "Query error:" << query.lastError().text();
        qDebug() << "SQL:" << sql;
        return items;
    }

    qDebug() << "Query successful, processing rows...";

    int count = 0;
    while (query.next()) {
        QVariantMap item;
        item["id"] = query.value("id");

        // Получаем статус
        if (hasStatusColumn) {
            item["status"] = query.value("status").toString();
        } else {
            item["status"] = "available"; // По умолчанию
        }

        item["material_type"] = query.value("material_type");
        item["manufacturer"] = query.value("manufacturer");
        item["model"] = query.value("model");
        item["part_number"] = query.value("part_number");
        item["serial_number"] = query.value("serial_number");
        item["capacity"] = query.value("capacity");
        item["interface_type"] = query.value("interface_type");
        item["notes"] = query.value("notes");
        item["arrival_date"] = query.value("arrival_date");
        item["invoice_number"] = query.value("invoice_number");

        // Опциональные поля
        if (columns.contains("created_at")) {
            item["created_at"] = query.value("created_at");
        } else {
            item["created_at"] = "";
        }

        if (columns.contains("updated_at")) {
            item["updated_at"] = query.value("updated_at");
        } else {
            item["updated_at"] = "";
        }

        // Отладочный вывод
        qDebug() << "Item" << ++count << ":"
                 << "ID:" << item["id"].toInt()
                 << "Status:" << item["status"].toString()
                 << "SN:" << item["serial_number"].toString()
                 << "Type:" << item["material_type"].toString();

        items.append(item);
    }

    qDebug() << "=== Total items loaded:" << items.size() << "===";

    return items;
}

QList<QVariantMap> Database::searchInventory(const QString &searchText)
{
    QList<QVariantMap> items;

    if (searchText.isEmpty()) {
        return getInventoryItems();
    }

    QStringList columns = getTableColumns("inventory");

    QString sql =
        "SELECT "
        "i.id, "
        "COALESCE(mt.name, 'Неизвестно') as material_type, "
        "COALESCE(man.name, 'Неизвестно') as manufacturer, "
        "COALESCE(m.name, 'Неизвестно') as model, "
        "i.part_number, "
        "i.serial_number, ";

    if (columns.contains("capacity")) {
        sql += "i.capacity, ";
    } else {
        sql += "'' as capacity, ";
    }

    sql +=
        "i.interface_type, "
        "i.notes, "
        "i.arrival_date, "
        "i.invoice_number ";

    if (columns.contains("created_at")) {
        sql += ", i.created_at ";
    }

    if (columns.contains("updated_at")) {
        sql += ", i.updated_at ";
    }

    sql +=
        "FROM inventory i "
        "LEFT JOIN material_types mt ON i.material_type_id = mt.id "
        "LEFT JOIN manufacturers man ON i.manufacturer_id = man.id "
        "LEFT JOIN models m ON i.model_id = m.id "
        "WHERE i.serial_number LIKE ? OR "
        "i.part_number LIKE ? OR ";

    if (columns.contains("capacity")) {
        sql += "i.capacity LIKE ? OR ";
    }

    sql +=
        "mt.name LIKE ? OR "
        "man.name LIKE ? OR "
        "m.name LIKE ? OR "
        "i.notes LIKE ? OR "
        "i.invoice_number LIKE ? "
        "ORDER BY i.arrival_date DESC, i.id DESC";

    QSqlQuery query;
    query.prepare(sql);

    QString searchPattern = "%" + searchText + "%";

    // Добавляем параметры в зависимости от наличия колонки capacity
    int paramCount = 7; // serial, part, mt, man, m, notes, invoice
    if (columns.contains("capacity")) {
        paramCount = 8;
    }

    for (int i = 0; i < paramCount; i++) {
        query.addBindValue(searchPattern);
    }

    if (query.exec()) {
        while (query.next()) {
            QVariantMap item;
            item["id"] = query.value("id");
            item["material_type"] = query.value("material_type");
            item["manufacturer"] = query.value("manufacturer");
            item["model"] = query.value("model");
            item["part_number"] = query.value("part_number");
            item["serial_number"] = query.value("serial_number");
            item["capacity"] = query.value("capacity");
            item["interface_type"] = query.value("interface_type");
            item["notes"] = query.value("notes");
            item["arrival_date"] = query.value("arrival_date");
            item["invoice_number"] = query.value("invoice_number");

            if (columns.contains("created_at")) {
                item["created_at"] = query.value("created_at");
            }

            if (columns.contains("updated_at")) {
                item["updated_at"] = query.value("updated_at");
            }

            items.append(item);
        }
    } else {
        qDebug() << "Search query error:" << query.lastError().text();
    }

    return items;
}

bool Database::deleteMaterialType(const QString &type)
{
    if (type.isEmpty()) return false;

    // Проверяем, используется ли тип материала
    if (isMaterialTypeUsed(type)) {
        qDebug() << "Cannot delete material type" << type << "- it is in use";
        return false;
    }

    QSqlQuery query;
    query.prepare("DELETE FROM material_types WHERE name = ?");
    query.addBindValue(type);

    bool success = query.exec();

    if (success) {
        qDebug() << "Deleted material type:" << type << "affected rows:" << query.numRowsAffected();
    } else {
        qDebug() << "Failed to delete material type:" << query.lastError().text();
    }

    return success;
}

bool Database::isMaterialTypeUsed(const QString &type)
{
    int materialId = getMaterialTypeId(type);
    if (materialId == -1) return false;

    QSqlQuery query;
    query.prepare(
        "SELECT COUNT(*) FROM inventory WHERE material_type_id = ? "
        "UNION ALL "
        "SELECT COUNT(*) FROM models WHERE material_type_id = ?"
    );
    query.addBindValue(materialId);
    query.addBindValue(materialId);

    int total = 0;
    if (query.exec()) {
        while (query.next()) {
            total += query.value(0).toInt();
        }
    }

    return total > 0;
}

bool Database::deleteManufacturer(const QString &manufacturer)
{
    if (manufacturer.isEmpty()) return false;

    // Проверяем, используется ли производитель
    if (isManufacturerUsed(manufacturer)) {
        qDebug() << "Cannot delete manufacturer" << manufacturer << "- it is in use";
        return false;
    }

    QSqlQuery query;
    query.prepare("DELETE FROM manufacturers WHERE name = ?");
    query.addBindValue(manufacturer);

    bool success = query.exec();

    if (success) {
        qDebug() << "Deleted manufacturer:" << manufacturer << "affected rows:" << query.numRowsAffected();
    } else {
        qDebug() << "Failed to delete manufacturer:" << query.lastError().text();
    }

    return success;
}

bool Database::isManufacturerUsed(const QString &manufacturer)
{
    int manufacturerId = getManufacturerId(manufacturer);
    if (manufacturerId == -1) return false;

    QSqlQuery query;
    query.prepare(
        "SELECT COUNT(*) FROM inventory WHERE manufacturer_id = ? "
        "UNION ALL "
        "SELECT COUNT(*) FROM models WHERE manufacturer_id = ?"
    );
    query.addBindValue(manufacturerId);
    query.addBindValue(manufacturerId);

    int total = 0;
    if (query.exec()) {
        while (query.next()) {
            total += query.value(0).toInt();
        }
    }

    return total > 0;
}

bool Database::deleteModel(const QString &materialType, const QString &manufacturer, const QString &modelName)
{
    if (materialType.isEmpty() || manufacturer.isEmpty() || modelName.isEmpty()) {
        return false;
    }

    // Проверяем, используется ли модель
    if (isModelUsed(materialType, manufacturer, modelName)) {
        qDebug() << "Cannot delete model" << modelName << "- it is in use";
        return false;
    }

    int materialId = getMaterialTypeId(materialType);
    int manufacturerId = getManufacturerId(manufacturer);

    if (materialId == -1 || manufacturerId == -1) {
        return false;
    }

    QSqlQuery query;
    query.prepare(
        "DELETE FROM models WHERE material_type_id = ? AND manufacturer_id = ? AND name = ?"
    );
    query.addBindValue(materialId);
    query.addBindValue(manufacturerId);
    query.addBindValue(modelName);

    bool success = query.exec();

    if (success) {
        qDebug() << "Deleted model:" << modelName << "affected rows:" << query.numRowsAffected();
    } else {
        qDebug() << "Failed to delete model:" << query.lastError().text();
    }

    return success;
}

bool Database::isModelUsed(const QString &materialType, const QString &manufacturer, const QString &modelName)
{
    int modelId = getModelId(materialType, manufacturer, modelName);
    if (modelId == -1) return false;

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM inventory WHERE model_id = ?");
    query.addBindValue(modelId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

// Методы для получения количества использований
int Database::getUsageCountForMaterialType(const QString &materialType)
{
    int materialId = getMaterialTypeId(materialType);
    if (materialId == -1) return 0;

    QSqlQuery query;
    query.prepare(
        "SELECT COUNT(*) FROM inventory WHERE material_type_id = ?"
    );
    query.addBindValue(materialId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

int Database::getUsageCountForManufacturer(const QString &manufacturer)
{
    int manufacturerId = getManufacturerId(manufacturer);
    if (manufacturerId == -1) return 0;

    QSqlQuery query;
    query.prepare(
        "SELECT COUNT(*) FROM inventory WHERE manufacturer_id = ?"
    );
    query.addBindValue(manufacturerId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

int Database::getUsageCountForModel(const QString &materialType, const QString &manufacturer, const QString &modelName)
{
    qDebug() << "=== getUsageCountForModel ===";
    qDebug() << "Material type:" << materialType;
    qDebug() << "Manufacturer:" << manufacturer;
    qDebug() << "Model name:" << modelName;

    int modelId = getModelId(materialType, manufacturer, modelName);
    qDebug() << "Model ID:" << modelId;

    if (modelId == -1) {
        qDebug() << "Model not found, usage count = 0";
        return 0;
    }

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM inventory WHERE model_id = ?");
    query.addBindValue(modelId);

    if (query.exec() && query.next()) {
        int count = query.value(0).toInt();
        qDebug() << "Usage count:" << count;
        return count;
    }

    qDebug() << "Query error:" << query.lastError().text();
    return 0;
}


bool Database::markItemAsWrittenOff(int itemId, const QString &issuedTo,
                                   const QDate &issueDate, const QString &comments)
{
    if (itemId <= 0 || issuedTo.isEmpty()) {
        return false;
    }

    QSqlQuery query;

    // Начинаем транзакцию
    db.transaction();

    // 1. Обновляем статус в инвентаре
    query.prepare("UPDATE inventory SET status = 'written_off' WHERE id = ?");
    query.addBindValue(itemId);

    if (!query.exec()) {
        db.rollback();
        qDebug() << "Failed to update inventory status:" << query.lastError().text();
        return false;
    }

    // 2. Добавляем запись в историю
    query.prepare(
        "INSERT INTO write_off_history (inventory_id, issued_to, issue_date, comments) "
        "VALUES (?, ?, ?, ?)"
    );
    query.addBindValue(itemId);
    query.addBindValue(issuedTo.trimmed());
    query.addBindValue(issueDate.toString("yyyy-MM-dd"));
    query.addBindValue(comments.trimmed());

    if (!query.exec()) {
        db.rollback();
        qDebug() << "Failed to add write-off history:" << query.lastError().text();
        return false;
    }

    db.commit();
    return true;
}

bool Database::markItemAsAvailable(int itemId)
{
    if (itemId <= 0) {
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE inventory SET status = 'available' WHERE id = ?");
    query.addBindValue(itemId);

    bool success = query.exec();

    if (success) {
        // Можно очистить последнюю запись истории или оставить как архив
        qDebug() << "Item" << itemId << "marked as available";
    } else {
        qDebug() << "Failed to mark item as available:" << query.lastError().text();
    }

    return success;
}

bool Database::isItemWrittenOff(int itemId)
{
    if (itemId <= 0) {
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT status FROM inventory WHERE id = ?");
    query.addBindValue(itemId);

    if (query.exec() && query.next()) {
        QString status = query.value(0).toString();
        return status == "written_off";
    }

    return false;
}

QVariantMap Database::getItemStatus(int itemId)
{
    QVariantMap status;

    if (itemId <= 0) {
        return status;
    }

    QSqlQuery query;
    query.prepare(
        "SELECT "
        "i.status, "
        "COALESCE(w.issued_to, '') as issued_to, "
        "COALESCE(w.issue_date, '') as issue_date, "
        "COALESCE(w.comments, '') as comments, "
        "COALESCE(w.created_at, '') as write_off_date "
        "FROM inventory i "
        "LEFT JOIN write_off_history w ON i.id = w.inventory_id "
        "WHERE i.id = ? "
        "ORDER BY w.created_at DESC LIMIT 1"
    );
    query.addBindValue(itemId);

    if (query.exec() && query.next()) {
        status["status"] = query.value("status");
        status["issued_to"] = query.value("issued_to");
        status["issue_date"] = query.value("issue_date");
        status["comments"] = query.value("comments");
        status["write_off_date"] = query.value("write_off_date");
    }

    return status;
}

QList<QVariantMap> Database::getWriteOffHistory(int itemId)
{
    QList<QVariantMap> history;

    QSqlQuery query;
    QString sql =
        "SELECT "
        "w.id, "
        "w.inventory_id, "
        "i.serial_number, "
        "i.part_number, "
        "mt.name as material_type, "
        "man.name as manufacturer, "
        "m.name as model, "
        "w.issued_to, "
        "w.issue_date, "
        "w.comments, "
        "w.created_at "
        "FROM write_off_history w "
        "JOIN inventory i ON w.inventory_id = i.id "
        "JOIN material_types mt ON i.material_type_id = mt.id "
        "JOIN manufacturers man ON i.manufacturer_id = man.id "
        "JOIN models m ON i.model_id = m.id "
        "WHERE 1=1";

    if (itemId > 0) {
        sql += " AND w.inventory_id = ?";
    }

    sql += " ORDER BY w.created_at DESC";

    query.prepare(sql);
    if (itemId > 0) {
        query.addBindValue(itemId);
    }

    if (query.exec()) {
        while (query.next()) {
            QVariantMap record;
            record["id"] = query.value("id");
            record["inventory_id"] = query.value("inventory_id");
            record["serial_number"] = query.value("serial_number");
            record["part_number"] = query.value("part_number");
            record["material_type"] = query.value("material_type");
            record["manufacturer"] = query.value("manufacturer");
            record["model"] = query.value("model");
            record["issued_to"] = query.value("issued_to");
            record["issue_date"] = query.value("issue_date");
            record["comments"] = query.value("comments");
            record["created_at"] = query.value("created_at");
            history.append(record);
        }
    }

    return history;
}
