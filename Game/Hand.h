#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Обработка действий игрока
class Hand
{
  public:
    Hand(Board *board) : board(board)
    {
    }
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK; // Изначально можно ходить
        int x = -1, y = -1;
        int xc = -1, yc = -1;
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT: // Выход из игры
                    resp = Response::QUIT;
                    break;
                case SDL_MOUSEBUTTONDOWN: // Нажатие мышкой
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK; // Отменить ход
                    }
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY; // Перезапуск игры
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL; // Или клетка
                    }
                    else
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;
                case SDL_WINDOWEVENT: // Изменение размера окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();
                        break;
                    }
                }
                if (resp != Response::OK)
                    break;
            }
        }
        return {resp, xc, yc};
    }

    Response wait() const // Ожидание действий после окончания игры
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT: // Выход
                    resp = Response::QUIT;
                    break;
                case SDL_WINDOWEVENT_SIZE_CHANGED: // Изменение размера окна
                    board->reset_window_size();
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY; // Перезапуск игры
                }
                break;
                }
                if (resp != Response::OK)
                    break;
            }
        }
        return resp;
    }

  private:
    Board *board;
};
