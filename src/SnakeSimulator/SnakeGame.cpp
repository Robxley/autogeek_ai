#include "SnakeGame.hpp"
#include <algorithm>

namespace agk {

    SnakeGame::SnakeGame(int width, int height) : m_gridWidth(width), m_gridHeight(height), m_rng(std::random_device{}()) {
        Reset();
    }

    void SnakeGame::Reset() {
        m_snake.clear();
        m_snake.push_back({m_gridWidth / 2, m_gridHeight / 2});
        m_snake.push_back({m_gridWidth / 2 - 1, m_gridHeight / 2});
        m_snake.push_back({m_gridWidth / 2 - 2, m_gridHeight / 2});
        
        m_dir = Direction::Right;
        m_nextDir = Direction::Right;
        m_score = 0;
        m_gameOver = false;
        
        SpawnFood();
    }

    void SnakeGame::SpawnFood() {
        std::uniform_int_distribution<int> distW(0, m_gridWidth - 1);
        std::uniform_int_distribution<int> distH(0, m_gridHeight - 1);
        
        bool onSnake;
        do {
            m_food = {distW(m_rng), distH(m_rng)};
            onSnake = std::find(m_snake.begin(), m_snake.end(), m_food) != m_snake.end();
        } while (onSnake);
    }

    void SnakeGame::SetDirection(Direction dir) {
        // Prevent 180 degree turns
        if (dir == Direction::Up && m_dir != Direction::Down) m_nextDir = dir;
        else if (dir == Direction::Down && m_dir != Direction::Up) m_nextDir = dir;
        else if (dir == Direction::Left && m_dir != Direction::Right) m_nextDir = dir;
        else if (dir == Direction::Right && m_dir != Direction::Left) m_nextDir = dir;
    }

    void SnakeGame::Update() {
        if (m_gameOver) return;
        
        m_dir = m_nextDir;
        Point head = m_snake.front();
        Point nextHead = head;
        
        switch (m_dir) {
            case Direction::Up:    nextHead.y--; break;
            case Direction::Down:  nextHead.y++; break;
            case Direction::Left:  nextHead.x--; break;
            case Direction::Right: nextHead.x++; break;
        }
        
        // Wall collision
        if (nextHead.x < 0 || nextHead.x >= m_gridWidth || nextHead.y < 0 || nextHead.y >= m_gridHeight) {
            m_gameOver = true;
            return;
        }
        
        // Self collision
        if (std::find(m_snake.begin(), m_snake.end(), nextHead) != m_snake.end()) {
            m_gameOver = true;
            return;
        }
        
        m_snake.push_front(nextHead);
        
        // Food collision
        if (nextHead == m_food) {
            m_score += 10;
            SpawnFood();
        } else {
            m_snake.pop_back();
        }
    }

} // namespace agk
