#include <SDL3/SDL.h>
#include "SnakeGame.hpp"
#include "EventSimulator.hpp"
#include <agk/RecordingEngine/Log.hpp>
#include <chrono>
#include <thread>

int main(int argc, char* argv[]) {
    agk::Log::Init();
    
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        AGK_ERROR("SDL_Init failed: {}", SDL_GetError());
        return 1;
    }

    const int windowWidth = 800;
    const int windowHeight = 600;
    const int gridSize = 20;
    
    SDL_Window* window = SDL_CreateWindow("AutogeekAI - Snake Debug Simulator", windowWidth, windowHeight, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

    agk::SnakeGame game(windowWidth / gridSize, windowHeight / gridSize);
    
    bool quit = false;
    bool debugMode = false;
    SDL_Event e;
    
    auto lastUpdate = std::chrono::steady_clock::now();
    const auto updateInterval = std::chrono::milliseconds(100);

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) quit = true;
            else if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                    case SDLK_UP:    game.SetDirection(agk::Direction::Up); break;
                    case SDLK_DOWN:  game.SetDirection(agk::Direction::Down); break;
                    case SDLK_LEFT:  game.SetDirection(agk::Direction::Left); break;
                    case SDLK_RIGHT: game.SetDirection(agk::Direction::Right); break;
                    case SDLK_Z:     game.SetDirection(agk::Direction::Up); break;
                    case SDLK_S:     game.SetDirection(agk::Direction::Down); break;
                    case SDLK_Q:     game.SetDirection(agk::Direction::Left); break;
                    case SDLK_D:     game.SetDirection(agk::Direction::Right); break;
                    case SDLK_ESCAPE: quit = true; break;
                    case SDLK_F1:     debugMode = !debugMode; AGK_INFO("Debug Mode: {}", debugMode ? "ON" : "OFF"); break;
                }
            }
        }

        // --- Logic ---
        auto now = std::chrono::steady_clock::now();
        if (now - lastUpdate >= updateInterval) {
            game.Update();
            lastUpdate = now;

            if (game.IsGameOver()) {
                AGK_INFO("Game Over! Score: {}. Resetting...", game.GetScore());
                game.Reset();
            }

            // --- Simulation Logic (Auto Event Generation) ---
            if (debugMode) {
                // Periodically simulate random key presses to test the RecordingEngine's Raw Input
                if (rand() % 10 == 0) agk::EventSimulator::SimulateKeyPress(VK_UP);
                if (rand() % 20 == 0) agk::EventSimulator::SimulateMouseClick(400, 300);
            }
        }

        // --- Rendering ---
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

        // Grid dots (optional, for visibility)
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        for(int x=0; x<windowWidth; x+=gridSize)
            for(int y=0; y<windowHeight; y+=gridSize)
                SDL_RenderPoint(renderer, x, y);

        // Food
        auto food = game.GetFood();
        SDL_FRect foodRect = { (float)food.x * gridSize, (float)food.y * gridSize, (float)gridSize, (float)gridSize };
        SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
        SDL_RenderFillRect(renderer, &foodRect);

        // Snake
        const auto& snake = game.GetSnake();
        SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255);
        for (const auto& p : snake) {
            SDL_FRect r = { (float)p.x * gridSize, (float)p.y * gridSize, (float)gridSize, (float)gridSize };
            SDL_RenderFillRect(renderer, &r);
        }

        // Debug Overlay (Visual feedback of simulated inputs)
        if (debugMode) {
            SDL_FRect debugIndicator = { 10, 10, 20, 20 };
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderFillRect(renderer, &debugIndicator);
        }

        SDL_RenderPresent(renderer);
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Cap to avoid CPU saturation
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
