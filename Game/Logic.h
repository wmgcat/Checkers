#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

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

    vector<move_pos> find_best_turns(const bool color)
    {
        // очищаем вектора
        next_move.clear();
        next_best_state.clear();
        
        find_first_best_turn(board->get_board(), color, -1, -1, 0); // находим лучший первый ход

        vector<move_pos> res; // создаем вектор результата
        int state = 0; // начальное состояние равно нулю
        do {
            res.push_back(next_move[state]); // Добавляем ходы в результат
            state = next_best_state[state]; // переходим в следующее состояние
        } while (state != -1 && next_move[state].x != -1);
        return res; // возвращаем результат
    }

private:
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
        double alpha = -1)
    {
        // заполняем вектора
        next_move.emplace_back(-1, -1, -1, -1);
        next_best_state.push_back(-1);
        if (state != 0) // если state не равно нулю, просчитываем ходы
            find_turns(x, y, mtx);
        // создаем копии turns и have_beats
        auto now_turns = turns;
        auto now_have_beats = have_beats;

        if (!now_have_beats && state != 0)
        {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha); // запускаем рекурсию 
        }

        double best_score = -1; // лучший счет изначально равен единице
        for (auto turn : now_turns) // перебираем все ходы
        {
            size_t new_state = next_move.size(); 
            double score;
            if (now_have_beats) // если есть кого бить, то продолжаем рекурсию
            {
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, new_state, best_score);   
            }
            else { // если нигого не бьем
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score); 
            }
            if (score > best_score) { // проверяем лучше ли новый результат чем best_score
                // если да, то обновляем информацию
                best_score = score;
                next_move[state] = turn;
                next_best_state[state] = (now_have_beats ? new_state : -1);
            }
        }
        return best_score; 
    }

    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
        double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        if (depth == Max_depth) { // условие выхода из рекурсии
            return calc_score(mtx, (depth % 2 == color)); // возвращаем наилучший результат
        }
        if (x != -1) { // получаем ходы
            find_turns(x, y, mtx); // по координатам
        }
        else {
            find_turns(color, mtx); // по цвету
        }
        // сохраняем новое значение
        auto now_turns = turns;
        auto now_have_beats = have_beats;
        if (!now_have_beats && x != -1) {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta); // запускаем рекурсию 
        }

        if (turns.empty()) { // если ходов нету
            return (depth % 2 ? 0 : INF); // значит мы либо проиграли, либо выиграли
        }

        // иначе считаем лучшие ходы
        double min_score = INF + 1;
        double max_score = -1;
        for (auto turn : now_turns) {
            double score; 
            if (now_have_beats) { // если есть побития то продолжаем серию
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            else {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            // обновление минимума и максимума
            min_score = min(min_score, score); 
            max_score = max(max_score, score); 
            // делаем альфа и бета отсечения
            if (depth % 2) { // если ходим мы то двигаем левую границу
                alpha = max(alpha, max_score);
            }
            else { // иначе правую
                beta = min(beta, min_score);
            }
            // проверяем уровень оптимизации
            if (optimization != "O0" && alpha > beta) {
                break;
            }
            if (optimization == "O2" && alpha == beta) {
                return (depth % 2 ? max_score + 1 : min_score - 1);
            }
        }
        return (depth % 2 ? max_score : min_score); // возвращаем результат
    }

    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const // производит ход на матрице, возвращает её копию
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)  // подсчитывает количество белых и черных пешек и королев
        {
            for (POS_T j = 0; j < 8; ++j)
            { 
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        if (w + wq == 0)
            return INF; // возвращает бесконечность если нет белых
        if (b + bq == 0)
            return 0; // возвращает 0 если нет черных
        int q_coef = 4; // вес дамки
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        return (b + bq * q_coef) / (w + wq * q_coef); // иначе возвращает общий счет высчитанный по этой формуле
    }

public:
    void find_turns(const bool color) // принимает цвет, вызывает другую функцию которая ищет возможные ходы
    {
        find_turns(color, board->get_board());
    }

    void find_turns(const POS_T x, const POS_T y) // аналогично предыдущей, но принимает не цвет а координаты 
    {
        find_turns(x, y, board->get_board()); 
    }

private:
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx) // ищет ходы. принимает цвет ходящего, а так же матрицу с состоянием поля 
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i) // проходимся по всем глеткам
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color) // если клетка совпадает с выбранным цветом, то выполняем еще один find turns, но уже от этой клетки
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx) // тоже ищет возможные ходы, но принимает позицию а не цвет
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type) // проверяет тип фигуры
        { // логика побитий
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
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
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    vector<move_pos> turns; // ходы которые были найдены с помощью функции find_turns
    bool have_beats; // флажёк, маркирующий, являются ли наши ходы побитиями
    int Max_depth; // максимальная глубина просчета

  private:
    default_random_engine rand_eng; // хранит тип способа получения случайностей
    string scoring_mode; // отвечал за оценку поля
    string optimization; // отвечает за тип оптимизации (есть 3 типа)
    // два вектора, отвечающие за восстановление последовательности ходов
    vector<move_pos> next_move;
    vector<int> next_best_state;
    Board *board; // указатель на объект класса доска
    Config *config; // указатель на объект класса конфиг
};