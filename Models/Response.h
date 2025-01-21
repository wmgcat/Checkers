#pragma once

enum class Response
{
    OK, // Возможность походить
    BACK, // Откатить ход 
    REPLAY, // Перезапуск игры
    QUIT, // Выход из игры
    CELL // Ход на клетку
};
