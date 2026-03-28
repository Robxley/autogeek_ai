#pragma once

#include <vector>
#include <deque>
#include <random>

namespace agk {

    enum class Direction { Up, Down, Left, Right };
    
    struct Point {
        int x, y;
        bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    };

    /**
     * @brief Simple Snake Game logic for debugging the RecordingEngine.
     */
    class SnakeGame {
    public:
        SnakeGame(int width, int height);

        void Update();
        void SetDirection(Direction dir);
        
        const std::deque<Point>& GetSnake() const { return m_snake; }
        Point GetFood() const { return m_food; }
        int GetScore() const { return m_score; }
        bool IsGameOver() const { return m_gameOver; }
        
        int GetGridWidth() const { return m_gridWidth; }
        int GetGridHeight() const { return m_gridHeight; }

        void Reset();

    private:
        void SpawnFood();

        int m_gridWidth;
        int m_gridHeight;
        std::deque<Point> m_snake;
        Point m_food;
        Direction m_dir;
        Direction m_nextDir;
        int m_score;
        bool m_gameOver;
        
        std::mt19937 m_rng;
    };

} // namespace agk
