#include <iostream>
#include <filesystem>
#include "database/database.h"

using namespace std;

int main() {
    try {
        Database db("schema.json", "mydatabase", "data");

        auto names = db.get_collection_names();
        if(names.get_size() == 0){
            throw runtime_error("в schema.json нет коллекций");
        }

        string c1 = names[0];
        string c2 = (names.get_size() > 1 ? names[1] : names[0]);

        auto& collection1 = db.get_collection(c1);
        auto& collection2 = db.get_collection(c2);

        cout << "Тест " << c1 << endl;

        collection1.insert({
            {"_id", "doc1"},
            {"key1", "значение1"},
            {"key2", 100},
            {"key3", {{"key1", "вложенное1"}, {"key2", 50}}},
            {"key4", true}
        });

        cout << "После doc1 - файлы: " << collection1.get_file_info().get_size() << endl;

        collection1.insert({
            {"_id", "doc2"},
            {"key1", "значение2"},
            {"key2", 200},
            {"key3", {{"key1", "вложенное2"}, {"key2", 75}}},
            {"key4", false}
        });

        cout << "После doc2 - файлы: " << collection1.get_file_info().get_size() << endl;

        cout << "Содержимое " << c1 << ":" << endl;
        collection1.print_file_contents();

        cout << "Всего документов: " << collection1.find().get_size() << endl;
        cout << "Документов с key2=100: " << collection1.find({{"key2", 100}}).get_size() << endl;
        cout << "Документов с key2>150: " << collection1.find({{"key2", {{"$gt", 150}}}}).get_size() << endl;

        collection1.update_one({{"_id", "doc1"}}, {{"$set", {{"key2", 999}}}});
        cout << "После обновления doc1:" << endl;
        collection1.print_file_contents();

        cout << "Тест " << c2 << endl;

        collection2.insert({
            {"_id", "temp1"},
            {"key1", "тест1"},
            {"key2", 300},
            {"key3", {{"key1", "влож1"}, {"key2", 100}}},
            {"key4", true}
        });

        collection2.insert({
            {"_id", "temp2"},
            {"key1", "тест2"},
            {"key2", 400},
            {"key3", {{"key1", "влож2"}, {"key2", 200}}},
            {"key4", false}
        });

        cout << "После вставки в " << c2 << ":" << endl;
        collection2.print_file_contents();

        collection2.delete_one({{"_id", "temp1"}});
        cout << "После удаления temp1:" << endl;
        collection2.print_file_contents();

        cout << "Итого:" << endl;
        cout << "Файлов " << c1 << ": " << collection1.get_file_info().get_size() << endl;
        cout << "Файлов " << c2 << ": " << collection2.get_file_info().get_size() << endl;
    }
    catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << endl;
        return 1;
    }
    return 0;
}
