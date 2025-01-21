#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // Возвращает результат игры: 0 - выход, 1 - победа чёрного, 2 - победа белого, или ничья
    int play()
    {
        // Фиксируем начальное время для измерения длительности игры
        auto start = chrono::steady_clock::now();
        if (is_replay)
        {
            // Сброс настроек при новой игре
            logic = Logic(&board, &config);
            config.reload();
            board.redraw();
        }
        else
        {
            // Отрисовка доски
            board.start_draw();
        }
        is_replay = false;

        int turn_num = -1; // кол-во ходов
        bool is_quit = false;
        const int Max_turns = config("Game", "MaxNumTurns"); // Получаем макс кол-во ходов за игру
        while (++turn_num < Max_turns) // Ход:
        {
            beat_series = 0;
            logic.find_turns(turn_num % 2);

            // Если ходов нет, игра завершена
            if (logic.turns.empty())
                break;

            // Устанавливаем максимальную глубину анализа для бота
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            
            // Проверяем, является ли текущий игрок ботом
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Если игрок человек, выполняем его ход
                auto resp = player_turn(turn_num % 2);
                if (resp == Response::QUIT) // Нажатие на выход
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY) // Нажатие на новая игра
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK) // Нажатие на отменить ход
                {
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                bot_turn(turn_num % 2); // ход бота
        }
        auto end = chrono::steady_clock::now(); // запись времени окончания хода

        // Запись хода в лог файл:
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Перезапуск игры
        if (is_replay)
            return play();

        // Выход
        if (is_quit)
            return 0;
        int res = 2;

        // Окончание игры из-за максимального кол-ва ходов
        if (turn_num == Max_turns)
        {
            res = 0;
        }
        else if (turn_num % 2)
        {
            res = 1;
        }
        board.show_final(res); // отображение экрана победы
        auto resp = hand.wait(); // Ожидание действия игрока
        if (resp == Response::REPLAY) // Перезапуск игры
        {
            is_replay = true;
            return play();
        }
        return res;
    }

  private:

    // Ход бота
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now(); // Начало хода

        auto delay_ms = config("Bot", "BotDelayMS"); // Задержка перед ходом бота (Берется из конфига с настройками)
        // new thread for equal delay for each turn
        thread th(SDL_Delay, delay_ms);
        auto turns = logic.find_best_turns(color); // Поиск хода
        th.join();
        bool is_first = true;
        // making moves
        for (auto turn : turns)
        {
            if (!is_first)
            {
                SDL_Delay(delay_ms);  // Задержка
            }
            is_first = false;
            beat_series += (turn.xb != -1);
            board.move_piece(turn, beat_series); // Ход бота
        }

        auto end = chrono::steady_clock::now(); // Время конца хода

        // Запись в лог хода
        ofstream fout(project_path + "log.txt", ios_base::app); 
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close(); // Закрытие файла с логами
    }

    // Функция отвечает за ход игрока (человека) в игре
    // color: цвет текущего игрока (true - белый, false - чёрный)
    // Возвращает Response::QUIT, если игрок выходит, Response::REPLAY для перезапуска игры, 
    // и Response::OK для успешного завершения хода
    Response player_turn(const bool color)
    {
        // Вектор для хранения координат клеток с возможными ходами
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y); // Добавляем все доступные начальные позиции для хода
        }

        board.highlight_cells(cells); // Подсвечиваем доступные клетки
        move_pos pos = {-1, -1, -1, -1}; // Инициализация структуры для хранения текущего хода
        POS_T x = -1, y = -1; // Координаты текущей выбранной клетки

        while (true)
        {
            auto resp = hand.get_cell(); // Получаем координаты выбранной клетки от игрока
            if (get<0>(resp) != Response::CELL) // Проверяем, не выбрал ли игрок "выход" или "перезапуск"
                return get<0>(resp);

            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)}; // Координаты выбранной клетки

            bool is_correct = false; // Флаг, указывающий на корректность выбора клетки
            for (auto turn : logic.turns)
            {
                // Проверяем, является ли клетка начальной позицией для нового хода
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                // Проверяем, соответствует ли выбор клетки завершению существующего хода
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn; // Сохраняем текущий ход
                    break;
                }
            }

            // Если ход найден, завершаем цикл
            if (pos.x != -1)
                break;

            // Если выбор некорректен, сбрасываем текущий выбор и подсветку
            if (!is_correct)
            {
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells); // Снова подсвечиваем доступные клетки
                }
                x = -1;
                y = -1;
                continue;
            }

            // Устанавливаем текущие координаты
            x = cell.first;
            y = cell.second;

            board.clear_highlight(); // Убираем подсветку
            board.set_active(x, y);  // Подсвечиваем выбранную клетку

            // Подсвечиваем все клетки, куда можно сделать ход из текущей клетки
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);
        }

        // Сбрасываем подсветку и активную клетку после выбора хода
        board.clear_highlight();
        board.clear_active();

        // Выполняем перемещение фигуры
        board.move_piece(pos, pos.xb != -1);

        // Если нет серии взятий (комбо), возвращаем Response::OK
        if (pos.xb == -1)
            return Response::OK;

        // Продолжаем серию взятий, если это возможно
        beat_series = 1; // Устанавливаем начальное значение серии
        while (true)
        {
            logic.find_turns(pos.x2, pos.y2); // Ищем возможные ходы для продолжения взятия
            if (!logic.have_beats) // Если больше нет взятий, выходим из цикла
                break;

            // Подсвечиваем клетки для взятия
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2); // Устанавливаем активную клетку

            // Попытка продолжить серию взятий
            while (true)
            {
                auto resp = hand.get_cell(); // Получаем клетку от игрока
                if (get<0>(resp) != Response::CELL) // Проверяем, не выбрал ли игрок "выход" или "перезапуск"
                    return get<0>(resp);

                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)}; // Координаты выбранной клетки

                bool is_correct = false; // Проверка корректности выбранной клетки
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn; // Сохраняем текущий ход
                        break;
                    }
                }
                if (!is_correct)
                    continue;

                // Если ход корректен, выполняем его
                board.clear_highlight();
                board.clear_active();
                beat_series += 1; // Увеличиваем длину серии взятий
                board.move_piece(pos, beat_series);
                break;
            }
        }

        // Возвращаем по завершении хода
        return Response::OK;
    }


  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
