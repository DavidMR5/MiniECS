#include <iostream>

#include "ecs/EntityManager.h"

int main()
{
    ecs::EntityManager manager;

    std::cout << "MiniECS - Entity Management\n";
    std::cout << "===========================\n\n";

    // Crear dos entidades
    ecs::Entity player = manager.create();
    ecs::Entity enemy = manager.create();

    std::cout << "Player created\n";
    std::cout << "  ID: " << player.id << '\n';
    std::cout << "  Generation: " << player.generation << '\n';
    std::cout << "  Alive: " << (manager.isAlive(player) ? "Yes" : "No") << "\n\n";

    std::cout << "Enemy created\n";
    std::cout << "  ID: " << enemy.id << '\n';
    std::cout << "  Generation: " << enemy.generation << '\n';
    std::cout << "  Alive: " << (manager.isAlive(enemy) ? "Yes" : "No") << "\n\n";

    // Destruir al enemigo
    manager.destroy(enemy);

    std::cout << "Enemy destroyed\n";
    std::cout << "  Alive: " << (manager.isAlive(enemy) ? "Yes" : "No") << "\n\n";

    // Crear una nueva entidad
    ecs::Entity newEnemy = manager.create();

    std::cout << "New enemy created\n";
    std::cout << "  ID: " << newEnemy.id << '\n';
    std::cout << "  Generation: " << newEnemy.generation << '\n';
    std::cout << "  Alive: " << (manager.isAlive(newEnemy) ? "Yes" : "No") << "\n\n";

    // Comprobar que la antigua entidad sigue siendo inválida
    std::cout << "Old enemy reference\n";
    std::cout << "  ID: " << enemy.id << '\n';
    std::cout << "  Generation: " << enemy.generation << '\n';
    std::cout << "  Alive: " << (manager.isAlive(enemy) ? "Yes" : "No") << '\n';

    return 0;
}