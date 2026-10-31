#include "raylib.h"
#include <vector>
#include <cmath>

// Константы игры
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const int GRAVITY = 1600;  // Увеличенная гравитация как в Mario
const int JUMP_FORCE = -550;  // Сила прыжка как в Mario
const int PLAYER_SPEED = 400;  // Скорость движения
const int PLATFORM_HEIGHT = 20;
const int MAX_LEVEL_WIDTH = 4000; // Увеличенный уровень

// Структура монетки
struct Coin {
    Rectangle rect;
    bool collected;
    float animationY;
    float animationSpeed;
    float rotation;

    Coin(float x, float y) {
        rect = { x, y, 16, 16 };
        collected = false;
        animationY = 0;
        animationSpeed = 2.0f;
        rotation = 0;
    }

    void Update(float deltaTime) {
        if (!collected) {
            animationY += animationSpeed * deltaTime;
            if (animationY > 10 || animationY < 0) animationSpeed *= -1;
            rotation += 5 * deltaTime;
        }
    }

    void Draw() {
        if (!collected) {
            DrawRectangle(rect.x, rect.y + animationY, rect.width, rect.height, GOLD);
            DrawCircle(rect.x + rect.width / 2, rect.y + rect.height / 2 + animationY, 6, YELLOW);
            DrawCircle(rect.x + rect.width / 2 - 2, rect.y + rect.height / 2 - 2 + animationY, 2, WHITE);
        }
    }
};

// Структура врага
struct Enemy {
    Rectangle rect;
    Vector2 velocity;
    bool active;
    Color color;
    float walkTimer;
    float deathTimer;
    bool isDead;

    Enemy(float x, float y) {
        rect = { x, y, 28, 28 };
        velocity = { -120, 0 }; // Немного быстрее
        active = true;
        color = BROWN;
        walkTimer = 0;
        deathTimer = 0;
        isDead = false;
    }

    void Update(float deltaTime, std::vector<Platform>& platforms) {
        if (!active) return;

        if (isDead) {
            deathTimer += deltaTime;
            if (deathTimer > 0.5f) {
                active = false;
            }
            return;
        }

        velocity.y += GRAVITY * deltaTime;
        rect.x += velocity.x * deltaTime;

        for (auto& platform : platforms) {
            if (CheckCollisionRecs(rect, platform.rect)) {
                if (velocity.x > 0) {
                    rect.x = platform.rect.x - rect.width;
                    velocity.x *= -1;
                }
                else if (velocity.x < 0) {
                    rect.x = platform.rect.x + platform.rect.width;
                    velocity.x *= -1;
                }
            }
        }

        rect.y += velocity.y * deltaTime;
        bool onGround = false;

        for (auto& platform : platforms) {
            if (CheckCollisionRecs(rect, platform.rect)) {
                if (velocity.y > 0) {
                    rect.y = platform.rect.y - rect.height;
                    velocity.y = 0;
                    onGround = true;
                }
                else if (velocity.y < 0) {
                    rect.y = platform.rect.y + platform.rect.height;
                    velocity.y = 0;
                }
            }
        }

        walkTimer += deltaTime;

        if (onGround && (rand() % 300 == 0)) {
            velocity.x *= -1;
        }

        if (rect.y > SCREEN_HEIGHT + 100) {
            active = false;
        }
    }

    void Draw() {
        if (!active) return;

        if (isDead) {
            DrawRectangle(rect.x, rect.y + rect.height / 2, rect.width, rect.height / 2, DARKBROWN);
            DrawCircle(rect.x + 8, rect.y + rect.height / 2 + 5, 2, WHITE);
            DrawCircle(rect.x + 20, rect.y + rect.height / 2 + 5, 2, WHITE);
            return;
        }

        int offset = (int)(walkTimer * 15) % 4;

        DrawRectangleRec(rect, color);
        DrawCircle(rect.x + 8, rect.y + 10, 3, WHITE);
        DrawCircle(rect.x + 20, rect.y + 10, 3, WHITE);
        DrawCircle(rect.x + 8 + (offset > 1 ? 2 : -2), rect.y + 10, 1, BLACK);
        DrawCircle(rect.x + 20 + (offset > 1 ? 2 : -2), rect.y + 10, 1, BLACK);
        DrawLine(rect.x + 5, rect.y + 5, rect.x + 11, rect.y + 7, BLACK);
        DrawLine(rect.x + 23, rect.y + 5, rect.x + 17, rect.y + 7, BLACK);
        DrawRectangle(rect.x + 5, rect.y + rect.height - 5, 6, 5, DARKBROWN);
        DrawRectangle(rect.x + 17, rect.y + rect.height - 5, 6, 5, DARKBROWN);
    }

    void Die() {
        if (!isDead) {
            isDead = true;
            velocity = { 0, 0 };
        }
    }
};

// Структура платформы
struct Platform {
    Rectangle rect;
    Color color;
    bool isBrick;
    bool isQuestion;
    bool used;

    Platform(float x, float y, float width, Color col, bool brick = false, bool question = false) {
        rect = { x, y, width, PLATFORM_HEIGHT };
        color = col;
        isBrick = brick;
        isQuestion = question;
        used = false;
    }

    void Draw() {
        DrawRectangleRec(rect, color);
        if (isBrick) {
            for (float i = rect.x; i < rect.x + rect.width; i += 20) {
                DrawLine(i, rect.y, i, rect.y + PLATFORM_HEIGHT, DARKGRAY);
            }
            for (float i = rect.y; i < rect.y + PLATFORM_HEIGHT; i += 10) {
                DrawLine(rect.x, i, rect.x + rect.width, i, DARKGRAY);
            }
        }
        if (isQuestion && !used) {
            DrawText("?", rect.x + rect.width / 2 - 5, rect.y + 2, 16, BLACK);
            DrawRectangleLines(rect.x, rect.y, rect.width, rect.height, DARKBROWN);
            float pulse = sin(GetTime() * 10) * 2;
            DrawRectangleLines(rect.x - pulse / 2, rect.y - pulse / 2, rect.width + pulse, rect.height + pulse, Fade(GOLD, 0.5f));
        }
    }
};

// Структура частиц
struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float life;
    float maxLife;

    Particle(float x, float y, Vector2 vel, Color col) {
        position = { x, y };
        velocity = vel;
        color = col;
        life = 0.5f;
        maxLife = 0.5f;
    }

    bool Update(float deltaTime) {
        life -= deltaTime;
        position.x += velocity.x * deltaTime;
        position.y += velocity.y * deltaTime;
        velocity.y += 500 * deltaTime;
        return life > 0;
    }

    void Draw() {
        float alpha = life / maxLife;
        DrawCircle(position.x, position.y, 3, Fade(color, alpha));
    }
};

// Структура игрока
struct Player {
    Rectangle rect;
    Vector2 velocity;
    Color color;
    bool canJump;
    bool isBig;
    int coinsCollected;
    float jumpBuffer;
    float coyoteTime;
    float invincibilityTimer;
    bool isInvincible;
    float deathAnimation;
    std::vector<Particle> particles;

    Player(float x, float y) {
        rect = { x, y, 30, 30 };
        velocity = { 0, 0 };
        color = RED;
        canJump = true;
        isBig = false;
        coinsCollected = 0;
        jumpBuffer = 0;
        coyoteTime = 0;
        invincibilityTimer = 0;
        isInvincible = false;
        deathAnimation = 0;
    }

    void AddParticles(float x, float y) {
        for (int i = 0; i < 8; i++) {
            Vector2 vel = { (float)(rand() % 300 - 150), (float)(rand() % 200 - 100) };
            particles.push_back(Particle(x, y, vel, GOLD));
        }
    }

    void Update(float deltaTime, std::vector<Platform>& platforms,
        std::vector<Coin>& coins, std::vector<Enemy>& enemies) {

        for (int i = particles.size() - 1; i >= 0; i--) {
            if (!particles[i].Update(deltaTime)) {
                particles.erase(particles.begin() + i);
            }
        }

        if (isInvincible) {
            invincibilityTimer -= deltaTime;
            if (invincibilityTimer <= 0) {
                isInvincible = false;
            }
        }

        // Горизонтальное движение с ускорением как в Mario
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
            velocity.x = -PLAYER_SPEED;
        }
        else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
            velocity.x = PLAYER_SPEED;
        }
        else {
            velocity.x *= 0.85f; // Трение как в Mario
        }

        // Ограничение скорости
        if (velocity.x > PLAYER_SPEED) velocity.x = PLAYER_SPEED;
        if (velocity.x < -PLAYER_SPEED) velocity.x = -PLAYER_SPEED;

        // Буфер прыжка
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
            jumpBuffer = 0.12f;
        }

        if (jumpBuffer > 0) {
            jumpBuffer -= deltaTime;
        }

        // Coyote time
        if (canJump) {
            coyoteTime = 0.1f;
        }
        else {
            coyoteTime -= deltaTime;
        }

        // Прыжок
        if (jumpBuffer > 0 && coyoteTime > 0 && deathAnimation <= 0) {
            velocity.y = JUMP_FORCE;
            canJump = false;
            coyoteTime = 0;
            jumpBuffer = 0;
            AddParticles(rect.x + rect.width / 2, rect.y + rect.height);
        }

        // Ускорение вниз
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
            velocity.y += GRAVITY * deltaTime * 1.3f;
        }
        else {
            velocity.y += GRAVITY * deltaTime;
        }

        if (velocity.y > 1000) velocity.y = 1000;

        // Движение
        rect.x += velocity.x * deltaTime;

        for (auto& platform : platforms) {
            if (CheckCollisionRecs(rect, platform.rect)) {
                if (velocity.x > 0) {
                    rect.x = platform.rect.x - rect.width;
                }
                else if (velocity.x < 0) {
                    rect.x = platform.rect.x + platform.rect.width;
                }
            }
        }

        rect.y += velocity.y * deltaTime;

        canJump = false;
        for (auto& platform : platforms) {
            if (CheckCollisionRecs(rect, platform.rect)) {
                if (velocity.y > 0) {
                    rect.y = platform.rect.y - rect.height;
                    velocity.y = 0;
                    canJump = true;
                }
                else if (velocity.y < 0) {
                    rect.y = platform.rect.y + platform.rect.height;
                    velocity.y = 0;

                    if (isBig && platform.isBrick && !platform.used) {
                        platform.rect.width = 0;
                        platform.used = true;
                        AddParticles(platform.rect.x, platform.rect.y);
                    }

                    if (platform.isQuestion && !platform.used) {
                        platform.isQuestion = false;
                        platform.used = true;
                        platform.color = DARKGRAY;
                        coinsCollected += 5;
                        AddParticles(platform.rect.x + platform.rect.width / 2, platform.rect.y);
                    }
                }
            }
        }

        // Сбор монет
        for (auto& coin : coins) {
            if (!coin.collected && CheckCollisionRecs(rect, coin.rect)) {
                coin.collected = true;
                coinsCollected++;
                AddParticles(coin.rect.x + coin.rect.width / 2, coin.rect.y + coin.rect.height / 2);
            }
        }

        // Столкновение с врагами
        for (auto& enemy : enemies) {
            if (!enemy.active) continue;

            if (CheckCollisionRecs(rect, enemy.rect)) {
                if (velocity.y > 0 && rect.y + rect.height - enemy.rect.y < 15 && !enemy.isDead) {
                    enemy.Die();
                    velocity.y = -450;
                    AddParticles(enemy.rect.x + enemy.rect.width / 2, enemy.rect.y + enemy.rect.height / 2);
                }
                else if (!isInvincible && deathAnimation <= 0) {
                    if (isBig) {
                        isBig = false;
                        rect.height = 30;
                        rect.y += 15;
                        isInvincible = true;
                        invincibilityTimer = 2.0f;
                        AddParticles(rect.x + rect.width / 2, rect.y + rect.height / 2);
                    }
                    else {
                        deathAnimation = 1.0f;
                        velocity = { 0, -400 };
                        AddParticles(rect.x + rect.width / 2, rect.y + rect.height / 2);
                    }
                }
            }
        }

        // Анимация смерти
        if (deathAnimation > 0) {
            deathAnimation -= deltaTime;
            if (deathAnimation <= 0) {
                rect.x = 100;
                rect.y = 500;
                velocity = { 0, 0 };
                canJump = true;
                coinsCollected = 0;
                isBig = false;
                rect.height = 30;
            }
        }

        // Границы
        if (rect.x < 0) rect.x = 0;
        if (rect.x + rect.width > MAX_LEVEL_WIDTH) rect.x = MAX_LEVEL_WIDTH - rect.width;

        if (rect.y > SCREEN_HEIGHT && deathAnimation <= 0) {
            if (isBig) {
                isBig = false;
                rect.height = 30;
                rect.y = 500;
                isInvincible = true;
                invincibilityTimer = 2.0f;
            }
            else {
                deathAnimation = 1.0f;
            }
        }
    }

    void Draw() {
        if (isInvincible && (int)(GetTime() * 10) % 2 == 0) {
            return;
        }

        if (deathAnimation > 0) {
            DrawRectangle(rect.x, rect.y + (1 - deathAnimation) * 50, rect.width, rect.height, RED);
            return;
        }

        if (isBig) {
            DrawRectangleRec(rect, color);
            DrawRectangle(rect.x, rect.y - 8, rect.width, 8, RED);
            DrawRectangle(rect.x + 5, rect.y - 12, 20, 5, RED);
            DrawRectangle(rect.x, rect.y + 25, rect.width, 10, BLUE);
        }
        else {
            DrawRectangleRec(rect, color);
            DrawRectangle(rect.x, rect.y - 5, rect.width, 5, RED);
            DrawRectangle(rect.x, rect.y + 20, rect.width, 10, BLUE);
        }

        // Усы
        DrawRectangle(rect.x + 8, rect.y + 15, 6, 3, BLACK);
        DrawRectangle(rect.x + 16, rect.y + 15, 6, 3, BLACK);

        // Глаза
        int eyeOffset = (velocity.x > 50) ? 3 : (velocity.x < -50) ? -3 : 0;
        DrawCircle(rect.x + 10 + eyeOffset, rect.y + 10, 3, WHITE);
        DrawCircle(rect.x + 20 + eyeOffset, rect.y + 10, 3, WHITE);
        DrawCircle(rect.x + 10 + eyeOffset, rect.y + 10, 1, BLACK);
        DrawCircle(rect.x + 20 + eyeOffset, rect.y + 10, 1, BLACK);

        // Кепка
        DrawRectangle(rect.x + 5, rect.y - 5, 20, 5, RED);

        for (auto& particle : particles) {
            particle.Draw();
        }
    }
};

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Super Mario Style Platformer");
    ToggleFullscreen();
    SetTargetFPS(60);
    SetExitKey(KEY_ESCAPE);
    InitAudioDevice();

    Player player(100, 500);

    Camera2D camera = { 0 };
    camera.offset = { SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    std::vector<Platform> platforms;

    // Пол - теперь ниже
    platforms.push_back(Platform(0, SCREEN_HEIGHT - 50, MAX_LEVEL_WIDTH, DARKGRAY));

    // НАЧАЛЬНАЯ ЗОНА - платформы ниже
    platforms.push_back(Platform(150, 520, 100, LIGHTGRAY));
    platforms.push_back(Platform(300, 480, 80, LIGHTGRAY));
    platforms.push_back(Platform(200, 420, 60, LIGHTGRAY));
    platforms.push_back(Platform(100, 360, 80, LIGHTGRAY));
    platforms.push_back(Platform(400, 350, 60, LIGHTGRAY));

    // ЗОНА С БЛОКАМИ
    platforms.push_back(Platform(500, 480, 40, LIGHTGRAY, false, true));
    platforms.push_back(Platform(550, 480, 40, LIGHTGRAY, true));
    platforms.push_back(Platform(600, 480, 40, LIGHTGRAY, true));
    platforms.push_back(Platform(650, 480, 40, LIGHTGRAY, false, true));
    platforms.push_back(Platform(500, 420, 120, LIGHTGRAY));
    platforms.push_back(Platform(650, 360, 80, LIGHTGRAY));

    // ВЕРХНЯЯ ЗОНА
    platforms.push_back(Platform(800, 450, 100, LIGHTGRAY));
    platforms.push_back(Platform(950, 400, 80, LIGHTGRAY));
    platforms.push_back(Platform(1100, 350, 100, LIGHTGRAY));
    platforms.push_back(Platform(1250, 300, 80, LIGHTGRAY));
    platforms.push_back(Platform(1400, 250, 100, LIGHTGRAY));
    platforms.push_back(Platform(1550, 200, 80, LIGHTGRAY));

    // ЗОНА С ВРАГАМИ
    platforms.push_back(Platform(1800, 520, 150, LIGHTGRAY));
    platforms.push_back(Platform(2000, 470, 100, LIGHTGRAY));
    platforms.push_back(Platform(2150, 420, 80, LIGHTGRAY));
    platforms.push_back(Platform(2300, 370, 100, LIGHTGRAY));

    // БОНУСНАЯ ЗОНА
    platforms.push_back(Platform(2500, 520, 60, LIGHTGRAY, false, true));
    platforms.push_back(Platform(2600, 520, 60, LIGHTGRAY, false, true));
    platforms.push_back(Platform(2700, 520, 60, LIGHTGRAY, false, true));
    platforms.push_back(Platform(2800, 520, 60, LIGHTGRAY, false, true));

    // ФИНИШ
    platforms.push_back(Platform(3000, 520, 80, LIGHTGRAY));
    platforms.push_back(Platform(3150, 470, 80, LIGHTGRAY));
    platforms.push_back(Platform(3300, 420, 80, LIGHTGRAY));
    platforms.push_back(Platform(3450, 370, 80, LIGHTGRAY));
    platforms.push_back(Platform(3600, 320, 80, LIGHTGRAY));
    platforms.push_back(Platform(3750, 200, 100, GREEN));

    // МОНЕТЫ
    std::vector<Coin> coins;
    // Начальная зона
    for (int i = 0; i < 5; i++) coins.push_back(Coin(180 + i * 25, 500));
    for (int i = 0; i < 3; i++) coins.push_back(Coin(320 + i * 25, 460));
    for (int i = 0; i < 3; i++) coins.push_back(Coin(130 + i * 25, 400));
    for (int i = 0; i < 3; i++) coins.push_back(Coin(420 + i * 25, 330));

    // Средняя зона
    for (int i = 0; i < 5; i++) coins.push_back(Coin(520 + i * 20, 400));
    for (int i = 0; i < 3; i++) coins.push_back(Coin(670 + i * 25, 340));
    for (int i = 0; i < 4; i++) coins.push_back(Coin(820 + i * 25, 430));
    for (int i = 0; i < 3; i++) coins.push_back(Coin(970 + i * 25, 380));

    // Верхняя зона
    for (int i = 0; i < 5; i++) coins.push_back(Coin(1120 + i * 20, 330));
    for (int i = 0; i < 3; i++) coins.push_back(Coin(1270 + i * 25, 280));
    for (int i = 0; i < 4; i++) coins.push_back(Coin(1420 + i * 25, 230));
    for (int i = 0; i < 3; i++) coins.push_back(Coin(1570 + i * 25, 180));

    // Зона с врагами
    for (int i = 0; i < 8; i++) coins.push_back(Coin(1820 + i * 25, 500));
    for (int i = 0; i < 5; i++) coins.push_back(Coin(2020 + i * 25, 450));
    for (int i = 0; i < 3; i++) coins.push_back(Coin(2170 + i * 25, 400));

    // Бонусная зона
    for (int i = 0; i < 8; i++) coins.push_back(Coin(2520 + i * 25, 500));

    // ВРАГИ
    std::vector<Enemy> enemies;
    enemies.push_back(Enemy(530, 480 - 28));
    enemies.push_back(Enemy(600, 480 - 28));
    enemies.push_back(Enemy(830, 450 - 28));
    enemies.push_back(Enemy(980, 400 - 28));
    enemies.push_back(Enemy(1130, 350 - 28));
    enemies.push_back(Enemy(1280, 300 - 28));
    enemies.push_back(Enemy(1430, 250 - 28));
    enemies.push_back(Enemy(1830, 520 - 28));
    enemies.push_back(Enemy(1950, 520 - 28));
    enemies.push_back(Enemy(2070, 470 - 28));
    enemies.push_back(Enemy(2190, 420 - 28));
    enemies.push_back(Enemy(2340, 370 - 28));

    float deltaTime = 0;
    bool gameWon = false;
    float flagAnimation = 0;

    while (!WindowShouldClose()) {
        deltaTime = GetFrameTime();

        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
        }

        if (IsKeyPressed(KEY_R) && gameWon) {
            gameWon = false;
            player = Player(100, 500);
            player.coinsCollected = 0;
            enemies.clear();
            enemies.push_back(Enemy(530, 480 - 28));
            enemies.push_back(Enemy(600, 480 - 28));
            enemies.push_back(Enemy(830, 450 - 28));
            enemies.push_back(Enemy(980, 400 - 28));
            enemies.push_back(Enemy(1130, 350 - 28));
            enemies.push_back(Enemy(1280, 300 - 28));
            enemies.push_back(Enemy(1430, 250 - 28));
            enemies.push_back(Enemy(1830, 520 - 28));
            enemies.push_back(Enemy(1950, 520 - 28));
            enemies.push_back(Enemy(2070, 470 - 28));
            enemies.push_back(Enemy(2190, 420 - 28));
            enemies.push_back(Enemy(2340, 370 - 28));

            // Сброс монет
            for (auto& coin : coins) {
                coin.collected = false;
            }
        }

        if (!gameWon) {
            player.Update(deltaTime, platforms, coins, enemies);

            for (auto& coin : coins) coin.Update(deltaTime);
            for (auto& enemy : enemies) enemy.Update(deltaTime, platforms);

            camera.target = { player.rect.x + player.rect.width / 2,
                             player.rect.y + player.rect.height / 2 };

            if (camera.target.x < SCREEN_WIDTH / 2) camera.target.x = SCREEN_WIDTH / 2;
            if (camera.target.x > MAX_LEVEL_WIDTH - SCREEN_WIDTH / 2)
                camera.target.x = MAX_LEVEL_WIDTH - SCREEN_WIDTH / 2;
            if (camera.target.y < SCREEN_HEIGHT / 2) camera.target.y = SCREEN_HEIGHT / 2;
            if (camera.target.y > 400) camera.target.y = 400;

            if (player.rect.x > 3720 && player.rect.y < 220) {
                gameWon = true;
            }

            // Превращение в большого при 20 монетах
            if (player.coinsCollected >= 30 && !player.isBig) {
                player.isBig = true;
                player.rect.height = 45;
                player.rect.y -= 15;
            }
        }

        flagAnimation += deltaTime * 5;

        BeginDrawing();
        ClearBackground(SKYBLUE);

        // Облака
        for (int i = 0; i < 15; i++) {
            float cloudX = camera.target.x - SCREEN_WIDTH / 2 + i * 250;
            cloudX = fmod(cloudX + 50, MAX_LEVEL_WIDTH + 300) - 150;
            DrawCircle(cloudX, 80, 40, WHITE);
            DrawCircle(cloudX + 40, 80, 40, WHITE);
            DrawCircle(cloudX + 20, 50, 40, WHITE);
        }

        BeginMode2D(camera);

        for (auto& platform : platforms) platform.Draw();
        for (auto& coin : coins) coin.Draw();
        for (auto& enemy : enemies) enemy.Draw();
        player.Draw();

        // Флаг
        if (!gameWon && player.rect.x > 3700) {
            DrawRectangle(3750, 200, 10, 100, BROWN);
            for (int i = 0; i < 3; i++) {
                DrawTriangle(
                    Vector2{ 3760, 200 },
                    Vector2{ 3760 + sin(flagAnimation + i) * 40, 230 },
                    Vector2{ 3760, 260 },
                    GREEN
                );
            }
        }

        EndMode2D();

        // UI
        DrawRectangle(0, 0, SCREEN_WIDTH, 120, Fade(BLACK, 0.7f));
        DrawText("SUPER MARIO STYLE", 20, 10, 30, RED);
        DrawText(TextFormat("Coins: %d / 30", player.coinsCollected), 20, 45, 25, GOLD);
        DrawText("WORLD 1-1", SCREEN_WIDTH - 150, 20, 25, WHITE);

        DrawText("A/D/Arrows  |  SPACE/W/UP Jump  |  S/DOWN Fast Fall", 20, 80, 15, WHITE);
        DrawText("F11 Fullscreen  |  R Restart  |  ESC Exit", 20, 100, 15, WHITE);

        if (player.isInvincible) {
            DrawText("INVINCIBLE!", SCREEN_WIDTH / 2 - 60, 80, 20, YELLOW);
        }

        if (!player.isBig && player.coinsCollected < 30) {
            DrawText("Collect 30 coins to grow!", SCREEN_WIDTH / 2 - 160, 80, 15, YELLOW);
        }

        if (gameWon) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.8f));
            DrawText("YOU WIN!", SCREEN_WIDTH / 2 - MeasureText("YOU WIN!", 80) / 2,
                SCREEN_HEIGHT / 2 - 50, 80, GREEN);
            DrawText(TextFormat("Final Score: %d coins", player.coinsCollected),
                SCREEN_WIDTH / 2 - MeasureText(TextFormat("Final Score: %d coins", player.coinsCollected), 30) / 2,
                SCREEN_HEIGHT / 2 + 20, 30, GOLD);
            DrawText("Press R to play again | ESC to exit",
                SCREEN_WIDTH / 2 - MeasureText("Press R to play again | ESC to exit", 20) / 2,
                SCREEN_HEIGHT / 2 + 80, 20, WHITE);
        }

        EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}