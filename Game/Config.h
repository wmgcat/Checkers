#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
  public:
    Config()
    {
        reload();
    }

    void reload()
    {
        std::ifstream fin(project_path + "settings.json"); // Открывает JSON файл по пути project_path
        fin >> config; // Записывает данные из файла в приватную переменную типа json - config
        fin.close(); // Закрывает открытый файл JSON
    }

    /*
      Оператор круглых скобок используется для доступа к конкретной настройке из json config.
      setting_dir — путь настроек
      setting_name — ключ в пути настроек
      Возвращает значение настройки в JSON-объект
    */
    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;
};
