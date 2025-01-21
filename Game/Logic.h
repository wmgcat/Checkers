#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

// Констанца для оценки
const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }
private:
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // Изменяет состояние доски в соответствии с заданным ходом
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;  // Удаление побитой фигуры
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;  // Превращение в дамку при достижении противоположной стороны
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];  // Перемещение фигуры на новую позицию
        mtx[turn.x][turn.y] = 0;  // Очистка старой позиции
        return mtx;  // Возврат обновленного состояния доски
    }

    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // Вычисляет оценку текущего состояния доски для бота
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);  // Подсчет белых шашек
                wq += (mtx[i][j] == 3);  // Подсчет белых дамок
                b += (mtx[i][j] == 2);  // Подсчет черных шашек
                bq += (mtx[i][j] == 4);  // Подсчет черных дамок
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);  // Учет позиции для белых шашек
                    b += 0.05 * (mtx[i][j] == 2) * (i);  // Учет позиции для черных шашек
                }
            }
        }
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        if (w + wq == 0)
            return INF;  // Максимальная оценка, если все белые фигуры побиты
        if (b + bq == 0)
            return 0;  // Минимальная оценка, если все черные фигуры побиты
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;  // Изменение коэффициента для дамок при использовании расширенной оценки
        }
        return (b + bq * q_coef) / (w + wq * q_coef);  // Вычисление итоговой оценки состояния
    }

    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
                               double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        if (depth == Max_depth) { // При достижении максимальной глубины, возврат значения
          return calc_score(mtx, (depth % 2 == color));
        }
        // Если есть серия побитий - поиск хода
        if (x != -1) {
            find_turns(x, y, mtx);
        }
        else {
            find_turns(color, mtx); // Иначе поиск все возможных ходов, доступных данному игроку
        }
        // Сохранение значения, по аналогии выше
        auto now_turns = turns;
        auto now_have_beats = have_beats;
        // Сохранение значения, по аналогии выше
        if (!now_have_beats && x != 0) {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }
        // Проверка, есть ли ходы.
        if (turns.empty()) {
            // Если есть, то происходит возврат вычисления, победил или проиграл текущий игрок
            return(depth % 2 ? 0 : INF); // Победа или поражение
        }
        double min_score = INF + 1;
        double max_score = -1;
        for (auto turn : now_turns) {
            double score;
            // Если есть побития, продолжается серия рекурсивных запусков
            if (now_have_beats) {
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            else {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            // Обновляет минимум и максимум
            min_score = min(min_score, score);
            max_score = max(max_score, score);
            // Альфа-Бета отсечения.
            if (depth % 2) { // Если наш ход, то мы максимизатор
                alpha = max(alpha, max_score);
            }
            else {
                beta = min(beta, min_score);
            }
            if (optimization != "O0" && alpha > beta) { // Если альфа больше беты - остановка цикла
                break;
            }
            if (optimization != "O2" && alpha == beta) { // Есть вероятность недосчета значений,смена возвращаемыъ параметров
                return(depth % 2 ? max_score + 1 : min_score - 1);
            }
        }
        return(depth % 2 ? max_score : min_score);
    }

public:
    void find_turns(const bool color)
    {
        // Поиск всех возможных ходов для заданного цвета на текущей доске
        find_turns(color, board->get_board());
    }

    void find_turns(const POS_T x, const POS_T y)
    {
        // Поиск всех возможных ходов для фигуры на заданной позиции
        find_turns(x, y, board->get_board());
    }

private:
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);  // Поиск ходов для каждой фигуры заданного цвета
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();  // Очистка списка ходов, если найдены ходы с побитием
                    }
                    if ((have_beats_before && have_beats)  !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());  // Добавление найденных ходов
                    }
                }
            }
        }
        turns = res_turns;  // Обновление списка всех возможных ходов
        shuffle(turns.begin(), turns.end(), rand_eng);  // Перемешивание ходов для случайности
        have_beats = have_beats_before;  // Обновление флага наличия ходов с побитием
    }

    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // Проверка возможности побития
        switch (type)
        {
        case 1:
        case 2:
            // Проверка для обычных шашек
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);  // Добавление хода с побитием
                }
            }
            break;
        default:
            // Проверка для дамок
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);  // Добавление хода с побитием
                        }
                    }
                }
            }
            break;
        }
        // Проверка других возможных ходов
        if (!turns.empty())
        {
            have_beats = true;
            return;  // Выход, если найдены ходы с побитием
        }
        switch (type)
        {
        case 1:
        case 2:
            // Проверка для обычных шашек
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);  // Добавление обычного хода
                }
                break;
            }
        default:
            // Проверка для дамок
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);  // Добавление обычного хода
                    }
                }
            }
            break;
        }
    }

public:
    vector<move_pos> turns;  // Список возможных ходов для текущего состояния
    bool have_beats;  // Флаг наличия ходов с побитием
    int Max_depth;  // Максимальная глубина поиска для бота

private:
    default_random_engine rand_eng;  // Генератор случайных чисел для перемешивания ходов
    string scoring_mode;  // Режим оценки для бота: только количество или количество и позиция
    string optimization;  // Уровень оптимизации для бота
    vector<move_pos> next_move;  // Вектор для хранения следующего лучшего хода
    vector<int> next_best_state;  // Вектор для хранения следующего лучшего состояния
    Board *board;  // Указатель на объект Board для доступа к состоянию доски
    Config *config;  // Указатель на объект Config для доступа к настройкам игры
};