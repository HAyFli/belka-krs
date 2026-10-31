#pragma execution_character_set("utf-8")
#include "raylib.h"
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdlib>

Font LoadRussianFont(const char* fileName, int fontSize) {
    int codepoints[65536] = { 0 };
    int count = 0;
    for (int i = 32; i <= 126; i++) codepoints[count++] = i;
    for (int i = 0x0400; i <= 0x04FF; i++) codepoints[count++] = i;
    Font font = LoadFontEx(fileName, fontSize, codepoints, count);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

enum GameState { MENU, LEVEL_SELECT, SKINS_SHOP, CONTROLS, PAUSED, GAMEPLAY, WIN, GAMEOVER };
enum EnemyType { LUMBERJACK, VANDAL };

struct Player {
    Rectangle rect;
    Vector2   speed;
    bool      canJump;
    bool      facingRight;
    int       lives;
};

struct Enemy {
    Rectangle rect;
    Vector2   speed;
    bool      active;
    EnemyType type;
    bool      flies;
    float     boundLeft;
    float     boundRight;
};

struct Platform {
    Rectangle rect;
    Color color;
};

struct Nut {
    Rectangle rect;
    bool  active;
};

struct FireObstacle {
    std::vector<Rectangle> fireRects;
    Rectangle              buttonRect;
    bool                   extinguished;
};

// ← ОДНА структура Level с backgroundFile, без skyColor
struct Level {
    const char* name;
    const char* backgroundFile;
    std::vector<Platform>     platforms;
    std::vector<Nut>          nuts;
    std::vector<Enemy>        enemies;
    std::vector<FireObstacle> fires;
    float                     playerStartX;
    float                     playerStartY;
    float                     exitX;
    float                     groundY;
};

struct Skin {
    const char* name;
    Color       bodyColor;
    Color       tailColor;
    int         price;
    const char* textureFile;       // статичная картинка (PNG)
    const char* spritesheetFile;   // спрайт-лист анимации (PNG)
    int         framesCount;       // количество кадров в спрайт-листе
};

// Текстуры скинов (загружаются в main)
struct SkinTextures {
    Texture2D still;             // статичная текстура (для магазина и стояния)
    Texture2D spritesheet;       // спрайт-лист анимации (все кадры в одной PNG)
    int       frameCount;        // кол-во кадров в спрайт-листе
    int       currentFrame;      // текущий кадр
    float     frameTimer;        // таймер смены кадра
    float     frameSpeed;        // секунд на кадр
    Rectangle frameRec;          // прямоугольник текущего кадра
};

struct SaveData {
    int               unlockedLevel = 0;
    int               continueLevel = 0;
    int               totalNuts = 0;
    int               equippedSkin = 0;
    std::vector<bool> ownedSkin;
};

const std::vector<Skin> SKINS = {
    { "Рыжая",   { 235, 120, 20, 255 }, { 150, 40, 30, 255 },  0,   "image\\yellow.png", "image\\yellowanim.png", 3 },
    { "Чёрная",  { 45, 45, 50, 255 },   { 15, 15, 18, 255 },   90,  "image\\dark.png",   "image\\darkanim.png",   3 },
    { "Снежная", { 245, 245, 250, 255 },{ 200, 200, 210, 255 }, 150, "image\\white.png",  "image\\whiteanim.png",  3 },
    { "Золотая", { 255, 200, 40, 255 }, { 190, 130, 10, 255 }, 300, "image\\gold.png",   "image\\goldanim.png",   3 },
};

SaveData LoadSave() {
    SaveData sd;
    sd.ownedSkin.assign(SKINS.size(), false);
    sd.ownedSkin[0] = true;
    std::ifstream f("squirrel_save.txt");
    if (f.is_open()) {
        int unlocked = 0, cont = 0, nuts = 0, equipped = 0;
        if (f >> unlocked >> cont >> nuts >> equipped) {
            sd.unlockedLevel = unlocked;
            sd.continueLevel = cont;
            sd.totalNuts = nuts;
            sd.equippedSkin = equipped;
            for (size_t i = 0; i < SKINS.size(); ++i) {
                int v = 0;
                if (f >> v) sd.ownedSkin[i] = (v != 0);
            }
        }
    }
    sd.ownedSkin[0] = true;
    return sd;
}

void SaveGame(const SaveData& sd) {
    std::ofstream f("squirrel_save.txt");
    if (!f.is_open()) return;
    f << sd.unlockedLevel << " " << sd.continueLevel << " " << sd.totalNuts << " " << sd.equippedSkin << "\n";
    for (bool owned : sd.ownedSkin) f << (owned ? 1 : 0) << " ";
    f << "\n";
}

std::vector<Nut> MakeNutRow(float x, float y, int count, float spacing = 40.0f) {
    std::vector<Nut> v;
    for (int i = 0; i < count; ++i) v.push_back({ { x + i * spacing, y, 18, 18 }, true });
    return v;
}

FireObstacle MakeFireWall(float fireX, float groundY, float width, float height, float buttonX, float buttonY) {
    FireObstacle fo;
    const float groundOverlap = 20.0f; // запас, чтобы низ текстуры огня гарантированно касался земли
    fo.fireRects.push_back({ fireX, groundY - height, width, height + groundOverlap });
    fo.buttonRect = { buttonX, buttonY, 32, 22 };
    fo.extinguished = false;
    return fo;
}

std::vector<Platform> MakeStaircase(float startX, float startY, int count, float dx, float dy, float w, float h, Color color) {
    std::vector<Platform> v;
    float x = startX, y = startY;
    for (int i = 0; i < count; ++i) {
        v.push_back({ { x, y, w, h }, color });
        x += dx; y += dy;
    }
    return v;
}

template <typename T>
void AppendAll(std::vector<T>& dst, const std::vector<T>& src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

Enemy MakeLumberjack(float groundY, float speed, float boundLeft, float boundRight) {
    Enemy e{};
    e.rect = { boundLeft + (boundRight - boundLeft) / 2, groundY - 38, 38, 38 };
    e.speed = { -speed, 0 };
    e.active = true;
    e.type = LUMBERJACK;
    e.flies = false;
    e.boundLeft = boundLeft;
    e.boundRight = boundRight;
    return e;
}

Enemy MakeVandal(float startX, float startY, float speed, float boundLeft, float boundRight) {
    Enemy e{};
    e.rect = { startX, startY, 36, 34 };
    e.speed = { speed, 0 };
    e.active = true;
    e.type = VANDAL;
    e.flies = true;
    e.boundLeft = boundLeft;
    e.boundRight = boundRight;
    return e;
}

std::vector<Level> BuildLevels() {
    std::vector<Level> levels;

    // УРОВЕНЬ 1
    {
        Level lv{};
        lv.name = "Лесная опушка";
        lv.backgroundFile = "image\\level1.png";
        lv.groundY = 750;
        lv.playerStartX = 100; lv.playerStartY = lv.groundY - 38;
        lv.exitX = 3300;

        lv.platforms.push_back({ { 0,    lv.groundY, 800,  100 }, DARKGREEN });
        lv.platforms.push_back({ { 950,  lv.groundY, 600,  100 }, DARKGREEN });
        lv.platforms.push_back({ { 1700, lv.groundY, 1800, 100 }, DARKGREEN });
        lv.platforms.push_back({ { 400,  lv.groundY - 130, 200, 60 }, GREEN });
        lv.platforms.push_back({ { 1100, lv.groundY - 130, 200, 60 }, GREEN });

        AppendAll(lv.nuts, MakeNutRow(430, lv.groundY - 160, 3));
        AppendAll(lv.nuts, MakeNutRow(1130, lv.groundY - 160, 3));
        AppendAll(lv.nuts, MakeNutRow(1900, lv.groundY - 30, 4));

        lv.enemies.push_back(MakeLumberjack(lv.groundY, 2.0f, 1750, 2100));
        lv.enemies.push_back(MakeVandal(2400, lv.groundY - 140, 2.5f, 2200, 2700));

        lv.fires.push_back(MakeFireWall(2900, lv.groundY, 100, 150, 2700, lv.groundY - 30));

        levels.push_back(lv);
    }

    // УРОВЕНЬ 2
    {
        Level lv{};
        lv.name = "Тёмная роща";
        lv.backgroundFile = "image\\level2.png";
        lv.groundY = 760;
        lv.playerStartX = 80; lv.playerStartY = lv.groundY - 38;
        lv.exitX = 3500;

        Color platColor = { 45, 35, 25, 255 };
        lv.platforms.push_back({ { 0,    lv.groundY, 600,  100 }, platColor });
        lv.platforms.push_back({ { 1600, lv.groundY, 2000, 100 }, platColor });

        float sx = 720, sy = lv.groundY - 120;
        for (int i = 0; i < 5; i++) {
            lv.platforms.push_back({ { sx, sy, 140, 60 }, BROWN });
            sx += 180; sy -= 120;
        }
        lv.platforms.push_back({ { sx, sy + 120, 140, 60 }, BROWN });

        float nx = 730, ny = lv.groundY - 120;
        for (int i = 0; i < 5; i++) {
            lv.nuts.push_back({ { nx + 50, ny - 30, 18, 18 }, true });
            nx += 180; ny -= 120;
        }
        AppendAll(lv.nuts, MakeNutRow(300, lv.groundY - 30, 3));

        lv.enemies.push_back(MakeLumberjack(lv.groundY, 2.5f, 1700, 2000));
        lv.enemies.push_back(MakeVandal(2500, lv.groundY - 140, 3.0f, 2200, 2800));

        lv.fires.push_back(MakeFireWall(2100, lv.groundY, 80, 150, 1900, lv.groundY - 30));
        lv.fires.push_back(MakeFireWall(3000, lv.groundY, 80, 150, 2900, lv.groundY - 30));

        levels.push_back(lv);
    }

    // УРОВЕНЬ 3
    {
        Level lv{};
        lv.name = "Заброшенная пилорама";
        lv.backgroundFile = "image\\level3.png";
        lv.groundY = 760;
        lv.playerStartX = 80; lv.playerStartY = lv.groundY - 38;
        lv.exitX = 4000;

        lv.platforms.push_back({ { 0,    lv.groundY, 500,  100 }, DARKGRAY });
        lv.platforms.push_back({ { 900,  lv.groundY, 700,  100 }, DARKGRAY });
        lv.platforms.push_back({ { 2000, lv.groundY, 600,  100 }, DARKGRAY });
        lv.platforms.push_back({ { 3000, lv.groundY, 1200, 100 }, DARKGRAY });
        lv.platforms.push_back({ { 580,  lv.groundY - 130, 150, 60 }, GRAY });
        lv.platforms.push_back({ { 1700, lv.groundY - 130, 150, 60 }, GRAY });
        lv.platforms.push_back({ { 2700, lv.groundY - 130, 130, 60 }, GRAY });
        lv.platforms.push_back({ { 2700, lv.groundY - 260, 130, 60 }, GRAY });

        AppendAll(lv.nuts, MakeNutRow(600, lv.groundY - 160, 3));
        AppendAll(lv.nuts, MakeNutRow(1200, lv.groundY - 30, 5));
        AppendAll(lv.nuts, MakeNutRow(2710, lv.groundY - 290, 2));

        lv.enemies.push_back(MakeLumberjack(lv.groundY, 3.0f, 1000, 1500));
        lv.enemies.push_back(MakeLumberjack(lv.groundY, 3.0f, 2100, 2500));
        lv.enemies.push_back(MakeVandal(3300, lv.groundY - 140, 3.5f, 3100, 3600));

        lv.fires.push_back(MakeFireWall(1400, lv.groundY, 120, 200, 1000, lv.groundY - 30));
        lv.fires.push_back(MakeFireWall(2400, lv.groundY, 120, 200, 2100, lv.groundY - 30));
        lv.fires.push_back(MakeFireWall(3700, lv.groundY, 100, 200, 3500, lv.groundY - 30));

        levels.push_back(lv);
    }

    // УРОВЕНЬ 4
    {
        Level lv{};
        lv.name = "Горящий лес";
        lv.backgroundFile = "image\\level4.png";
        lv.groundY = 760;
        lv.playerStartX = 80; lv.playerStartY = lv.groundY - 38;
        lv.exitX = 4200;

        Color gc = { 90, 40, 25, 255 };
        lv.platforms.push_back({ { 0,    lv.groundY, 800,  100 }, gc });
        lv.platforms.push_back({ { 1100, lv.groundY, 400,  100 }, gc });
        lv.platforms.push_back({ { 1800, lv.groundY, 800,  100 }, gc });
        lv.platforms.push_back({ { 2900, lv.groundY, 1500, 100 }, gc });
        lv.platforms.push_back({ { 830,  lv.groundY - 120, 150, 60 }, BROWN });
        lv.platforms.push_back({ { 1580, lv.groundY - 120, 150, 60 }, BROWN });
        lv.platforms.push_back({ { 2080, lv.groundY - 130, 140, 60 }, BROWN });
        lv.platforms.push_back({ { 3180, lv.groundY - 130, 140, 60 }, BROWN });

        AppendAll(lv.nuts, MakeNutRow(850, lv.groundY - 150, 2));
        AppendAll(lv.nuts, MakeNutRow(1600, lv.groundY - 150, 2));
        AppendAll(lv.nuts, MakeNutRow(2100, lv.groundY - 160, 3));

        lv.enemies.push_back(MakeVandal(400, lv.groundY - 140, 3.5f, 300, 700));
        lv.enemies.push_back(MakeLumberjack(lv.groundY, 3.5f, 1150, 1450));
        lv.enemies.push_back(MakeLumberjack(lv.groundY, 3.5f, 1900, 2400));
        lv.enemies.push_back(MakeVandal(3500, lv.groundY - 140, 4.0f, 3300, 3800));

        lv.fires.push_back(MakeFireWall(600, lv.groundY, 150, 250, 200, lv.groundY - 30));
        lv.fires.push_back(MakeFireWall(2400, lv.groundY, 100, 250, 2110, lv.groundY - 160));
        lv.fires.push_back(MakeFireWall(3800, lv.groundY, 150, 250, 3210, lv.groundY - 160));

        levels.push_back(lv);
    }

    // УРОВЕНЬ 5
    {
        Level lv{};
        lv.name = "Лагерь вандалов";
        lv.backgroundFile = "image\\level5.png";
        lv.groundY = 760;
        lv.playerStartX = 80; lv.playerStartY = lv.groundY - 38;
        lv.exitX = 4800;

        Color gc = { 55, 25, 25, 255 };
        lv.platforms.push_back({ { 0,    lv.groundY, 500,  100 }, gc });
        lv.platforms.push_back({ { 800,  lv.groundY, 800,  100 }, gc });
        lv.platforms.push_back({ { 1900, lv.groundY, 600,  100 }, gc });
        lv.platforms.push_back({ { 2800, lv.groundY, 600,  100 }, gc });
        lv.platforms.push_back({ { 3700, lv.groundY, 1300, 100 }, gc });
        lv.platforms.push_back({ { 530,  lv.groundY - 120, 150, 60 }, DARKGRAY });
        lv.platforms.push_back({ { 1680, lv.groundY - 120, 150, 60 }, DARKGRAY });
        lv.platforms.push_back({ { 2580, lv.groundY - 120, 150, 60 }, DARKGRAY });
        lv.platforms.push_back({ { 3480, lv.groundY - 120, 150, 60 }, DARKGRAY });

        AppendAll(lv.nuts, MakeNutRow(550, lv.groundY - 150, 2));
        AppendAll(lv.nuts, MakeNutRow(1700, lv.groundY - 150, 2));
        AppendAll(lv.nuts, MakeNutRow(2600, lv.groundY - 150, 2));
        AppendAll(lv.nuts, MakeNutRow(3500, lv.groundY - 150, 2));
        AppendAll(lv.nuts, MakeNutRow(4200, lv.groundY - 30, 5));

        lv.enemies.push_back(MakeLumberjack(lv.groundY, 4.0f, 900, 1400));
        lv.enemies.push_back(MakeLumberjack(lv.groundY, 4.0f, 2000, 2400));
        lv.enemies.push_back(MakeLumberjack(lv.groundY, 4.0f, 2900, 3300));
        lv.enemies.push_back(MakeVandal(1000, lv.groundY - 140, 4.5f, 800, 1500));
        lv.enemies.push_back(MakeVandal(3000, lv.groundY - 140, 4.5f, 2800, 3300));
        lv.enemies.push_back(MakeVandal(4200, lv.groundY - 140, 5.0f, 3900, 4600));

        lv.fires.push_back(MakeFireWall(1400, lv.groundY, 150, 250, 850, lv.groundY - 30));
        lv.fires.push_back(MakeFireWall(2400, lv.groundY, 150, 250, 1950, lv.groundY - 30));
        lv.fires.push_back(MakeFireWall(3300, lv.groundY, 150, 250, 2850, lv.groundY - 30));
        lv.fires.push_back(MakeFireWall(4500, lv.groundY, 200, 300, 3800, lv.groundY - 30));

        levels.push_back(lv);
    }

    return levels;
}

void LoadLevel(int idx,
    const std::vector<Level>& levels,
    Player& player,
    std::vector<Platform>& platforms,
    std::vector<Nut>& nuts,
    std::vector<Enemy>& enemies,
    std::vector<FireObstacle>& fires)
{
    const Level& lv = levels[idx];
    player.rect = { lv.playerStartX, lv.playerStartY, 56, 56 };
    player.speed = { 0, 0 };
    player.canJump = false;
    player.facingRight = true;
    platforms = lv.platforms;
    nuts = lv.nuts;
    enemies = lv.enemies;
    fires = lv.fires;
}

void KillPlayer(Player& player, const Level& lv) {
    player.rect.x = lv.playerStartX;
    player.rect.y = lv.playerStartY;
    player.speed = { 0, 0 };
    player.lives--;
}

void DrawSquirrel(const Rectangle& r, const Skin& skin, bool facingRight, const Texture2D* tex = nullptr) {
    // Если есть текстура — рисуем её (с отражением по горизонтали если смотрит влево)
    if (tex && tex->id > 0) {
        float srcW = (float)tex->width;
        float srcH = (float)tex->height;
        // Отражаем по X если смотрит влево
        Rectangle src = { 0, 0, facingRight ? srcW : -srcW, srcH };
        DrawTexturePro(*tex, src, r, { 0, 0 }, 0.0f, WHITE);
        return;
    }
    // Фолбэк: рисуем примитивами (если текстура не загружена)
    float tailCx = facingRight ? r.x - 6 : r.x + r.width + 6;
    float tailCy = r.y + r.height * 0.35f;
    DrawEllipse((int)tailCx, (int)tailCy, 16, 24, skin.tailColor);
    DrawEllipse((int)tailCx, (int)tailCy, 10, 17, skin.bodyColor);
    DrawRectangleRec(r, skin.bodyColor);
    float earBaseY = r.y + 6;
    DrawTriangle({ r.x + 4, earBaseY }, { r.x + 12, r.y - 10 }, { r.x + 17, earBaseY }, skin.bodyColor);
    DrawTriangle({ r.x + r.width - 17, earBaseY }, { r.x + r.width - 12, r.y - 10 }, { r.x + r.width - 4, earBaseY }, skin.bodyColor);
    float eyeY = r.y + r.height * 0.32f;
    float eyeOffset = facingRight ? 4.0f : -4.0f;
    DrawCircle((int)(r.x + r.width * 0.32f + eyeOffset), (int)eyeY, 4, WHITE);
    DrawCircle((int)(r.x + r.width * 0.68f + eyeOffset), (int)eyeY, 4, WHITE);
    DrawCircle((int)(r.x + r.width * 0.32f + eyeOffset), (int)eyeY, 2, BLACK);
    DrawCircle((int)(r.x + r.width * 0.68f + eyeOffset), (int)eyeY, 2, BLACK);
    DrawCircle((int)(r.x + r.width / 2 + (facingRight ? 6 : -6)), (int)(r.y + r.height * 0.55f), 3, BLACK);
}

void DrawEnemy(const Enemy& en) {
    Color base = (en.type == LUMBERJACK) ? Color{ 95, 60, 35, 255 } : Color{ 60, 65, 80, 255 };
    DrawRectangleRec(en.rect, base);
    if (en.type == LUMBERJACK) {
        DrawRectangle((int)(en.rect.x + en.rect.width - 6), (int)(en.rect.y - 12), 6, 28, DARKBROWN);
        DrawTriangle(
            { en.rect.x + en.rect.width - 6, en.rect.y - 12 },
            { en.rect.x + en.rect.width + 14, en.rect.y - 4 },
            { en.rect.x + en.rect.width - 6, en.rect.y + 8 },
            LIGHTGRAY);
    }
    else {
        DrawCircle((int)(en.rect.x + en.rect.width / 2), (int)(en.rect.y - 8), 8, RED);
        DrawCircle((int)(en.rect.x + en.rect.width / 2), (int)(en.rect.y - 8), 4, MAROON);
    }
    DrawCircle((int)(en.rect.x + 10), (int)(en.rect.y + 14), 3, RED);
    DrawCircle((int)(en.rect.x + en.rect.width - 10), (int)(en.rect.y + 14), 3, RED);
}

void DrawFireObstacle(const FireObstacle& fo, const Font& font, int frameCount, const Texture2D& fireTexture, const Texture2D& unfireTexture) {
    if (!fo.extinguished) {
        for (const auto& fr : fo.fireRects) {
            if (fireTexture.id > 0) {
                DrawTexturePro(
                    fireTexture,
                    { 0, 0, (float)fireTexture.width, (float)fireTexture.height },
                    fr,
                    { 0, 0 }, 0.0f, WHITE
                );
            }
        }
        if (unfireTexture.id > 0) {
            DrawTexturePro(
                unfireTexture,
                { 0, 0, (float)unfireTexture.width, (float)unfireTexture.height },
                fo.buttonRect,
                { 0, 0 }, 0.0f, WHITE
            );
        }
        DrawTextEx(font, "E", { fo.buttonRect.x + 16, fo.buttonRect.y - 26 }, 22, 1, WHITE);
    }
    // Огонь потушен — ни огонь, ни кнопка не рисуются
}

// Рисует платформу тайлингом block.png
// Берём только видимую часть текстуры (y=40..150) и растягиваем на rect
// Тайлим по X кусками 100px чтобы паттерн повторялся
void DrawTiledPlatform(const Rectangle& rect, const Texture2D& tex, const Camera2D& cam) {
    if (tex.id == 0) {
        // fallback — просто цветной прямоугольник
        DrawRectangleRec(rect, Color{ 34, 139, 34, 255 });
        return;
    }

    const float SRC_Y1 = 40.0f;   // начало видимой части (трава)
    const float SRC_Y2 = 150.0f;  // конец видимой части
    const float SRC_H = SRC_Y2 - SRC_Y1;  // 110px
    const float SRC_TW = 100.0f;  // ширина тайла в текстуре (800/8)
    float bw = (float)tex.width;

    // Экранные координаты платформы
    Vector2 tl = GetWorldToScreen2D({ rect.x, rect.y }, cam);
    Vector2 br = GetWorldToScreen2D({ rect.x + rect.width, rect.y + rect.height }, cam);
    float sw = br.x - tl.x;  // ширина на экране
    float sh = br.y - tl.y;  // высота на экране

    // Ширина одного тайла на экране (масштаб пропорционален высоте)
    float scaleH = sh / SRC_H;           // сколько экранных пикселей на 1px текстуры
    float tileSW = SRC_TW * scaleH;      // ширина тайла на экране

    int nx = (int)(sw / tileSW) + 2;

    BeginScissorMode((int)tl.x, (int)tl.y, (int)sw, (int)sh);
    for (int tx = 0; tx < nx; tx++) {
        float srcX = fmod((float)(tx * (int)SRC_TW), bw - SRC_TW);
        DrawTexturePro(tex,
            { srcX, SRC_Y1, SRC_TW, SRC_H },           // только видимая полоса текстуры
            { tl.x + tx * tileSW, tl.y, tileSW, sh },  // на весь rect платформы
            { 0, 0 }, 0.0f, WHITE);
    }
    EndScissorMode();
}

int main() {
    InitWindow(0, 0, "Белка спасает мир");
    SetExitKey(KEY_NULL);

    int monitor = GetCurrentMonitor();
    // Безрамочное окно размером с экран вместо настоящего fullscreen.
    // Так игра выглядит как полноэкранная, но остаётся обычным окном —
    // Alt+Tab и переключение на другие приложения работают нормально,
    // и игра не "захватывает" экран целиком.
    SetWindowState(FLAG_WINDOW_UNDECORATED);
    SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
    Vector2 monPos = GetMonitorPosition(monitor);
    SetWindowPosition((int)monPos.x, (int)monPos.y);
    SetTargetFPS(60);

    int SW = GetScreenWidth();
    int SH = GetScreenHeight();
    const float GRAVITY = 0.5f;
    const float MAX_FALL = 15.0f;
    const float JUMP_FORCE = -13.0f;
    const float MOVE_SPEED = 6.0f;
    const int   START_LIVES = 3;

    int cp[512];
    for (int i = 0; i < 96; i++)  cp[i] = 32 + i;
    for (int i = 0; i < 256; i++) cp[96 + i] = 0x0400 + i;
    Font font = LoadFontEx("arial.ttf", 42, cp, 352);

    // ── Текстуры скинов ──────────────────────────────────────────────────
    std::vector<SkinTextures> skinTextures(SKINS.size());
    for (size_t i = 0; i < SKINS.size(); ++i) {
        skinTextures[i].still = LoadTexture(SKINS[i].textureFile);

        skinTextures[i].spritesheet = LoadTexture(SKINS[i].spritesheetFile);
        skinTextures[i].frameCount = SKINS[i].framesCount;
        skinTextures[i].currentFrame = 0;
        skinTextures[i].frameTimer = 0.0f;
        skinTextures[i].frameSpeed = 0.1f;

        // Вычисляем ширину одного кадра (кадры идут горизонтально)
        float frameW = 0.0f, frameH = 0.0f;
        if (skinTextures[i].spritesheet.id > 0 && skinTextures[i].frameCount > 0) {
            frameW = (float)skinTextures[i].spritesheet.width / skinTextures[i].frameCount;
            frameH = (float)skinTextures[i].spritesheet.height;
        }
        skinTextures[i].frameRec = { 0.0f, 0.0f, frameW, frameH };
    }

    std::vector<Level> levels = BuildLevels();
    const int LEVEL_COUNT = (int)levels.size();
    SaveData sd = LoadSave();

    GameState state = MENU;
    int menuSel = 0;
    int levelSelSel = 0;
    int skinSel = 0;
    int pauseSel = 0;
    int currentLvl = 0;
    int animFrameCount = 0;  // переименовано чтобы не конфликтовало

    const std::vector<std::string> menuItems = { "Играть", "Уровни", "Скины", "Управление", "Выход" };
    const std::vector<std::string> pauseItems = { "Продолжить", "Начать уровень заново", "Главное меню" };

    Player player{ { 100, 500, 56, 56 }, { 0, 0 }, false, true, START_LIVES };
    std::vector<Platform>     platforms;
    std::vector<Nut>          nuts;
    std::vector<Enemy>        enemies;
    std::vector<FireObstacle> fires;

    Camera2D camera{};
    camera.offset = { SW / 2.0f, SH / 2.0f };
    camera.zoom = 1.0f;

    // ── Фоновая текстура уровня ─────────────────────────────────────────
    Texture2D bgTexture = { 0 };
    int lastLoadedLevel = -1;

    // ── Фоновая текстура главного меню (грузится один раз, не скроллится) ─
    Texture2D menuTexture = LoadTexture("image\\menu.png");

    // ── Текстуры огня и кнопки тушения (грузятся один раз) ───────────────
    Texture2D fireTexture = LoadTexture("image\\fire.png");
    Texture2D unfireTexture = LoadTexture("image\\unfire.png");
    Texture2D blockTexture = LoadTexture("image\\block.png");

    while (!WindowShouldClose()) {

        // Вызов внешней справки CHM по нажатию F1
        if (IsKeyPressed(KEY_F1)) {

            // Заменили currentState на state, а PLAYING на GAMEPLAY
            if (state == GAMEPLAY) {
                state = PAUSED; // Заменили PAUSE на PAUSED
            }

            system("start NewProject.chm");
        }

        animFrameCount++;

        // Обновление анимации — только при движении
        {
            int si = sd.equippedSkin;
            auto& st = skinTextures[si];
            if (st.frameCount > 1 && st.spritesheet.id > 0) {
                st.frameTimer += GetFrameTime();
                if (st.frameTimer >= st.frameSpeed) {
                    st.frameTimer = 0.0f;
                    st.currentFrame = (st.currentFrame + 1) % st.frameCount;
                    float frameW = (float)st.spritesheet.width / st.frameCount;
                    st.frameRec.x = st.currentFrame * frameW;
                }
            }
        }

        // Загружаем фон при входе в уровень или смене уровня
        if ((state == GAMEPLAY || state == PAUSED) && lastLoadedLevel != currentLvl) {
            if (bgTexture.id > 0) UnloadTexture(bgTexture);
            bgTexture = LoadTexture(levels[currentLvl].backgroundFile);
            lastLoadedLevel = currentLvl;
        }

        // ── ВВОД ────────────────────────────────────────────────────────
        if (state == MENU) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) menuSel = (menuSel + (int)menuItems.size() - 1) % (int)menuItems.size();
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) menuSel = (menuSel + 1) % (int)menuItems.size();
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (menuSel == 0) {
                    currentLvl = sd.continueLevel;
                    player.lives = START_LIVES;
                    LoadLevel(currentLvl, levels, player, platforms, nuts, enemies, fires);
                    state = GAMEPLAY;
                }
                else if (menuSel == 1) { state = LEVEL_SELECT; levelSelSel = currentLvl; }
                else if (menuSel == 2) { state = SKINS_SHOP;   skinSel = sd.equippedSkin; }
                else if (menuSel == 3) { state = CONTROLS; }
                else { SaveGame(sd); break; }
            }
        }
        else if (state == LEVEL_SELECT) {
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) levelSelSel = (levelSelSel + LEVEL_COUNT - 1) % LEVEL_COUNT;
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) levelSelSel = (levelSelSel + 1) % LEVEL_COUNT;
            if (IsKeyPressed(KEY_ESCAPE)) state = MENU;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (levelSelSel <= sd.unlockedLevel) {
                    currentLvl = levelSelSel;
                    player.lives = START_LIVES;
                    LoadLevel(currentLvl, levels, player, platforms, nuts, enemies, fires);
                    state = GAMEPLAY;
                }
            }
        }
        else if (state == SKINS_SHOP) {
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) skinSel = (skinSel + (int)SKINS.size() - 1) % (int)SKINS.size();
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) skinSel = (skinSel + 1) % (int)SKINS.size();
            if (IsKeyPressed(KEY_ESCAPE)) state = MENU;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (sd.ownedSkin[skinSel]) {
                    sd.equippedSkin = skinSel;
                    SaveGame(sd);
                }
                else if (sd.totalNuts >= SKINS[skinSel].price) {
                    sd.totalNuts -= SKINS[skinSel].price;
                    sd.ownedSkin[skinSel] = true;
                    sd.equippedSkin = skinSel;
                    SaveGame(sd);
                }
            }
        }
        else if (state == CONTROLS) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) state = MENU;
        }
        else if (state == PAUSED) {
            if (IsKeyPressed(KEY_ESCAPE)) state = GAMEPLAY;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) pauseSel = (pauseSel + (int)pauseItems.size() - 1) % (int)pauseItems.size();
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) pauseSel = (pauseSel + 1) % (int)pauseItems.size();
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (pauseSel == 0) { state = GAMEPLAY; }
                else if (pauseSel == 1) {
                    player.lives = START_LIVES;
                    LoadLevel(currentLvl, levels, player, platforms, nuts, enemies, fires);
                    state = GAMEPLAY;
                }
                else { SaveGame(sd); state = MENU; }
            }
        }
        else if (state == GAMEOVER || state == WIN) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) state = MENU;
        }
        else if (state == GAMEPLAY) {
            if (IsKeyPressed(KEY_ESCAPE)) { pauseSel = 0; state = PAUSED; }

            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) { player.speed.x = -MOVE_SPEED; player.facingRight = false; }
            else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { player.speed.x = MOVE_SPEED; player.facingRight = true; }
            else player.speed.x = 0;

            if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) && player.canJump) {
                player.speed.y = JUMP_FORCE;
                player.canJump = false;
            }

            player.speed.y += GRAVITY;
            if (player.speed.y > MAX_FALL) player.speed.y = MAX_FALL;

            player.rect.x += player.speed.x;
            player.rect.y += player.speed.y;
            if (player.rect.x < 0) player.rect.x = 0;

            // Стало:
            player.canJump = false;
            for (const auto& plat : platforms) {
                if (CheckCollisionRecs(player.rect, plat.rect)) {
                    float playerBottom = player.rect.y + player.rect.height;
                    float playerTop = player.rect.y;
                    float platTop = plat.rect.y;
                    float platBottom = plat.rect.y + plat.rect.height;

                    float overlapTop = playerBottom - platTop;
                    float overlapBottom = platBottom - playerTop;

                    if (overlapTop < overlapBottom && player.speed.y >= 0) {
                        // Приземление сверху
                        player.rect.y = platTop - player.rect.height;
                        player.speed.y = 0;
                        player.canJump = true;
                    }
                    else if (player.speed.y < 0) {
                        // Удар о потолок
                        player.rect.y = platBottom;
                        player.speed.y = 0;
                    }
                }
            }

            bool burned = false;
            for (auto& fo : fires) {
                if (!fo.extinguished) {
                    for (const auto& fr : fo.fireRects) {
                        if (CheckCollisionRecs(player.rect, fr)) { burned = true; break; }
                    }
                }
                if (burned) break;
            }
            if (burned) {
                KillPlayer(player, levels[currentLvl]);
                if (player.lives <= 0) { state = GAMEOVER; SaveGame(sd); continue; }
            }

            Rectangle interactZone = { player.rect.x - 16, player.rect.y - 16, player.rect.width + 32, player.rect.height + 32 };
            if (IsKeyPressed(KEY_E)) {
                for (auto& fo : fires) {
                    if (!fo.extinguished && CheckCollisionRecs(interactZone, fo.buttonRect))
                        fo.extinguished = true;
                }
            }

            for (auto& en : enemies) {
                if (!en.active) continue;
                en.rect.x += en.speed.x;
                if (en.rect.x <= en.boundLeft) { en.rect.x = en.boundLeft;                  en.speed.x = std::fabs(en.speed.x); }
                if (en.rect.x + en.rect.width >= en.boundRight) { en.rect.x = en.boundRight - en.rect.width; en.speed.x = -std::fabs(en.speed.x); }

                if (!en.flies) {
                    en.speed.y += GRAVITY;
                    if (en.speed.y > MAX_FALL) en.speed.y = MAX_FALL;
                    en.rect.y += en.speed.y;
                    for (const auto& plat : platforms) {
                        if (CheckCollisionRecs(en.rect, plat.rect)) {
                            if (en.speed.y > 0 && en.rect.y + en.rect.height - en.speed.y <= plat.rect.y + 1) {
                                en.rect.y = plat.rect.y - en.rect.height;
                                en.speed.y = 0;
                            }
                        }
                    }
                }

                if (CheckCollisionRecs(player.rect, en.rect)) {
                    bool stomp = (!en.flies) && player.speed.y > 0 &&
                        player.rect.y + player.rect.height - player.speed.y <= en.rect.y + 14;
                    if (stomp) { en.active = false; player.speed.y = -11.0f; }
                    else {
                        KillPlayer(player, levels[currentLvl]);
                        if (player.lives <= 0) { state = GAMEOVER; SaveGame(sd); break; }
                    }
                }
            }

            for (auto& n : nuts) {
                if (n.active && CheckCollisionRecs(player.rect, n.rect)) { n.active = false; sd.totalNuts++; }
            }

            if (player.lives <= 0) { state = GAMEOVER; SaveGame(sd); }
            else if (player.rect.y > SH + 600) {
                KillPlayer(player, levels[currentLvl]);
                if (player.lives <= 0) { state = GAMEOVER; SaveGame(sd); }
            }

            if (state == GAMEPLAY && player.rect.x >= levels[currentLvl].exitX) {
                currentLvl++;
                if (currentLvl > sd.unlockedLevel) sd.unlockedLevel = currentLvl;
                sd.continueLevel = (currentLvl < LEVEL_COUNT) ? currentLvl : (LEVEL_COUNT - 1);
                SaveGame(sd);
                if (currentLvl >= LEVEL_COUNT) { state = WIN; }
                else { LoadLevel(currentLvl, levels, player, platforms, nuts, enemies, fires); }
            }

            camera.target = { player.rect.x + player.rect.width / 2.0f, player.rect.y + player.rect.height / 2.0f };
        }

        // ── ОТРИСОВКА ───────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);

        if (state == GAMEPLAY || state == PAUSED) {
            // Фон — на весь экран, до камеры (не скроллится)
            if (bgTexture.id > 0) {
                DrawTexturePro(
                    bgTexture,
                    { 0, 0, (float)bgTexture.width, (float)bgTexture.height },
                    { 0, 0, (float)SW, (float)SH },
                    { 0, 0 }, 0.0f, WHITE
                );
                // Затемнение фона
                DrawRectangle(0, 0, SW, SH, Color{ 0, 0, 0, 160 });
            }

            BeginMode2D(camera);

            // Рисуем все платформы с тайлингом block.png
            EndMode2D();
            for (const auto& plat : platforms)
                DrawTiledPlatform(plat.rect, blockTexture, camera);
            BeginMode2D(camera);

            for (const auto& n : nuts) {
                if (n.active) {
                    int cx = (int)(n.rect.x + 9), cy = (int)(n.rect.y + 9);
                    DrawCircle(cx, cy, 9, BROWN);
                    DrawCircle(cx, cy - 2, 7, GOLD);
                }
            }

            for (const auto& fo : fires) DrawFireObstacle(fo, font, animFrameCount, fireTexture, unfireTexture);
            for (const auto& en : enemies) if (en.active) DrawEnemy(en);

            float fx = levels[currentLvl].exitX;
            float gy = levels[currentLvl].groundY;
            DrawRectangle((int)fx, (int)(gy - 120), 10, 120, DARKGRAY);
            DrawRectangle((int)fx + 10, (int)(gy - 120), 40, 30, GREEN);
            DrawTextEx(font, "Выход", { fx - 10, gy - 155 }, 22, 1, WHITE);

            // Выбираем текстуру: анимация при движении, статика при стоянии
            {
                int si = sd.equippedSkin;
                bool isMoving = (player.speed.x != 0);
                if (si >= 0 && si < (int)skinTextures.size()) {
                    auto& st = skinTextures[si];
                    if (isMoving && st.spritesheet.id > 0 && st.frameCount > 0) {
                        // Спрайт-лист: вырезаем текущий кадр, с отражением если влево
                        Rectangle src = st.frameRec;
                        if (!player.facingRight) src.width = -src.width;
                        DrawTexturePro(st.spritesheet, src, player.rect, { 0, 0 }, 0.0f, WHITE);
                    }
                    else {
                        // Статичная текстура стояния
                        const Texture2D* drawTex = (st.still.id > 0) ? &st.still : nullptr;
                        DrawSquirrel(player.rect, SKINS[si], player.facingRight, drawTex);
                    }
                }
                else {
                    DrawSquirrel(player.rect, SKINS[si], player.facingRight, nullptr);
                }
            }

            EndMode2D();

            // HUD поверх фона
            const char* lvlName = levels[currentLvl].name;
            Vector2 lnSize = MeasureTextEx(font, lvlName, 26, 1);
            DrawTextEx(font, lvlName, { SW / 2.0f - lnSize.x / 2, 12 }, 26, 1, WHITE);
            DrawTextEx(font, TextFormat("Орешки: %d", sd.totalNuts), { 20, 18 }, 30, 2, GOLD);
            DrawTextEx(font, TextFormat("Жизни: %d", player.lives), { 20, 58 }, 30, 2, RED);
            DrawTextEx(font, TextFormat("Уровень %d / %d", currentLvl + 1, LEVEL_COUNT), { (float)SW - 260, 18 }, 26, 1, LIGHTGRAY);
            DrawTextEx(font, "ESC — пауза", { (float)SW - 220, 50 }, 22, 1, GRAY);

            if (state == PAUSED) {
                DrawRectangle(0, 0, SW, SH, Color{ 0, 0, 0, 160 });
                const char* ptitle = "Пауза";
                Vector2 pts = MeasureTextEx(font, ptitle, 44, 2);
                DrawTextEx(font, ptitle, { SW / 2.0f - pts.x / 2, SH / 3.0f - 70 }, 44, 2, YELLOW);
                for (size_t i = 0; i < pauseItems.size(); ++i) {
                    Vector2 ms = MeasureTextEx(font, pauseItems[i].c_str(), 30, 2);
                    Color   col = (pauseSel == (int)i) ? YELLOW : WHITE;
                    DrawTextEx(font, pauseItems[i].c_str(), { SW / 2.0f - ms.x / 2, SH / 3.0f + i * 50.0f }, 30, 2, col);
                }
            }
        }
        else if (state == MENU) {
            // Фон меню — на весь экран, статичный, без скролла (как и фон уровней)
            if (menuTexture.id > 0) {
                DrawTexturePro(
                    menuTexture,
                    { 0, 0, (float)menuTexture.width, (float)menuTexture.height },
                    { 0, 0, (float)SW, (float)SH },
                    { 0, 0 }, 0.0f, WHITE
                );
            }

            const char* title = "Белка спасает мир";
            Vector2 ts = MeasureTextEx(font, title, 46, 2);
            Vector2 titlePos = { SW / 2.0f - ts.x / 2, SH / 5.0f };

            // Подложка под заголовком, чтобы текст не сливался с фоном
            float titlePadX = 36.0f, titlePadY = 18.0f;
            Rectangle titleBox = {
                titlePos.x - titlePadX,
                titlePos.y - titlePadY,
                ts.x + titlePadX * 2.0f,
                ts.y + titlePadY * 2.0f
            };
            DrawRectangleRounded(titleBox, 0.35f, 8, Color{ 15, 15, 15, 180 });
            DrawRectangleRoundedLinesEx(titleBox, 0.35f, 8, 2.0f, Color{ 255, 215, 80, 220 });

            // Тень + сам текст для дополнительного контраста
            DrawTextEx(font, title, { titlePos.x + 3, titlePos.y + 3 }, 46, 2, Color{ 0, 0, 0, 200 });
            DrawTextEx(font, title, titlePos, 46, 2, YELLOW);

            // ── Кнопки меню: прямоугольники со скруглёнными краями ───────
            const float btnW = 360.0f;
            const float btnH = 56.0f;
            const float btnGap = 16.0f;
            const float roundness = 0.4f;   // степень скругления углов (0..1)
            const int   segments = 8;       // сглаженность скругления

            for (size_t i = 0; i < menuItems.size(); ++i) {
                bool selected = (menuSel == (int)i);

                Rectangle btn = {
                    SW / 2.0f - btnW / 2.0f,
                    SH / 2.5f + i * (btnH + btnGap),
                    btnW,
                    btnH
                };

                Color fillColor = selected
                    ? Color{ 90, 70, 20, 230 }
                : Color{ 20, 20, 20, 170 };
                Color borderColor = selected ? YELLOW : Color{ 230, 230, 230, 200 };

                DrawRectangleRounded(btn, roundness, segments, fillColor);
                DrawRectangleRoundedLinesEx(btn, roundness, segments, selected ? 3.0f : 2.0f, borderColor);

                Vector2 ms = MeasureTextEx(font, menuItems[i].c_str(), 34, 2);
                Color   txtCol = selected ? YELLOW : WHITE;
                DrawTextEx(font, menuItems[i].c_str(),
                    { btn.x + btn.width / 2.0f - ms.x / 2.0f, btn.y + btn.height / 2.0f - ms.y / 2.0f },
                    34, 2, txtCol);
            }

            const char* nutsLine = TextFormat("Орешков собрано: %d", sd.totalNuts);
            Vector2 ns = MeasureTextEx(font, nutsLine, 24, 1);
            DrawTextEx(font, nutsLine, { SW / 2.0f - ns.x / 2, (float)SH - 70 }, 24, 1, GOLD);
        }
        else if (state == LEVEL_SELECT) {
            const char* title = "Выбор уровня";
            Vector2 ts = MeasureTextEx(font, title, 40, 2);
            DrawTextEx(font, title, { SW / 2.0f - ts.x / 2, 60 }, 40, 2, WHITE);

            float boxW = 200, boxH = 140, gap = 30;
            float totalW = LEVEL_COUNT * boxW + (LEVEL_COUNT - 1) * gap;
            float startX = SW / 2.0f - totalW / 2.0f;
            float boxY = SH / 2.0f - boxH / 2.0f;

            for (int i = 0; i < LEVEL_COUNT; ++i) {
                bool      unlocked = (i <= sd.unlockedLevel);
                Rectangle box = { startX + i * (boxW + gap), boxY, boxW, boxH };
                Color     fill = unlocked ? Color{ 40, 70, 40, 255 } : Color{ 40, 40, 40, 255 };
                DrawRectangleRec(box, fill);
                if (levelSelSel == i) DrawRectangleLinesEx(box, 4, YELLOW);

                const char* num = TextFormat("%d", i + 1);
                Vector2     numSz = MeasureTextEx(font, num, 44, 2);
                DrawTextEx(font, num, { box.x + boxW / 2 - numSz.x / 2, box.y + 18 }, 44, 2, unlocked ? WHITE : GRAY);

                const char* nm = levels[i].name;
                Vector2     nmSz = MeasureTextEx(font, nm, 18, 1);
                DrawTextEx(font, nm, { box.x + boxW / 2 - nmSz.x / 2, box.y + boxH - 50 }, 18, 1, unlocked ? LIGHTGRAY : GRAY);

                if (!unlocked) {
                    const char* lock = "ЗАКРЫТО";
                    Vector2     lSz = MeasureTextEx(font, lock, 16, 1);
                    DrawTextEx(font, lock, { box.x + boxW / 2 - lSz.x / 2, box.y + boxH - 26 }, 16, 1, RED);
                }
            }
            const char* hint = "Стрелки — выбор уровня, Enter — играть, ESC — назад";
            Vector2 hs = MeasureTextEx(font, hint, 20, 1);
            DrawTextEx(font, hint, { SW / 2.0f - hs.x / 2, (float)SH - 60 }, 20, 1, LIGHTGRAY);
        }
        else if (state == SKINS_SHOP) {
            const char* title = "Скины белки";
            Vector2 ts = MeasureTextEx(font, title, 40, 2);
            DrawTextEx(font, title, { SW / 2.0f - ts.x / 2, 50 }, 40, 2, WHITE);

            const char* nutsLine = TextFormat("Орешков: %d", sd.totalNuts);
            Vector2 ns = MeasureTextEx(font, nutsLine, 26, 1);
            DrawTextEx(font, nutsLine, { SW / 2.0f - ns.x / 2, 110 }, 26, 1, GOLD);

            float boxW = 180, boxH = 200, gap = 26;
            float totalW = SKINS.size() * boxW + (SKINS.size() - 1) * gap;
            float startX = SW / 2.0f - totalW / 2.0f;
            float boxY = SH / 2.0f - boxH / 2.0f;

            for (size_t i = 0; i < SKINS.size(); ++i) {
                Rectangle box = { startX + i * (boxW + gap), boxY, boxW, boxH };
                DrawRectangleRec(box, Color{ 35, 35, 45, 255 });
                if (skinSel == (int)i) DrawRectangleLinesEx(box, 4, YELLOW);

                Rectangle preview = { box.x + boxW / 2 - 32, box.y + 20, 64, 64 };
                // Показываем первый кадр спрайт-листа как превью
                if (i < skinTextures.size() && skinTextures[i].spritesheet.id > 0 && skinTextures[i].frameCount > 0) {
                    float fw = (float)skinTextures[i].spritesheet.width / skinTextures[i].frameCount;
                    float fh = (float)skinTextures[i].spritesheet.height;
                    DrawTexturePro(skinTextures[i].spritesheet,
                        { 0, 0, fw, fh },
                        preview, { 0, 0 }, 0.0f, WHITE);
                }
                else if (i < skinTextures.size() && skinTextures[i].still.id > 0) {
                    DrawTexturePro(skinTextures[i].still,
                        { 0, 0, (float)skinTextures[i].still.width, (float)skinTextures[i].still.height },
                        preview, { 0, 0 }, 0.0f, WHITE);
                }
                else {
                    DrawSquirrel(preview, SKINS[i], true);
                }

                Vector2 nmSz = MeasureTextEx(font, SKINS[i].name, 18, 1);
                DrawTextEx(font, SKINS[i].name, { box.x + boxW / 2 - nmSz.x / 2, box.y + 100 }, 18, 1, WHITE);

                std::string status;
                Color statusColor = WHITE;
                if (sd.equippedSkin == (int)i) { status = "НАДЕТО";      statusColor = LIME; }
                else if (sd.ownedSkin[i]) { status = "В наличии";   statusColor = LIGHTGRAY; }
                else { status = TextFormat("%d орешков", SKINS[i].price);      statusColor = GOLD; }

                Vector2 stSz = MeasureTextEx(font, status.c_str(), 18, 1);
                DrawTextEx(font, status.c_str(), { box.x + boxW / 2 - stSz.x / 2, box.y + 140 }, 18, 1, statusColor);
            }
            const char* hint = "Стрелки — выбор, Enter — надеть/купить, ESC — назад";
            Vector2 hs = MeasureTextEx(font, hint, 20, 1);
            DrawTextEx(font, hint, { SW / 2.0f - hs.x / 2, (float)SH - 60 }, 20, 1, LIGHTGRAY);
        }
        else if (state == CONTROLS) {
            const char* title = "Управление";
            Vector2 ts = MeasureTextEx(font, title, 40, 2);
            DrawTextEx(font, title, { SW / 2.0f - ts.x / 2, 70 }, 40, 2, WHITE);

            std::vector<std::string> lines = {
                "Управление персонажем:",
                "",
                "W / ПРОБЕЛ — прыжок.",
                "A / D или СТРЕЛКИ — движение влево/вправо.",
                "E — взаимодействие с объектами (потушить огонь).",
                "",
                "Орешки: собирайте золотые орешки на уровнях.",
                "Это валюта игры — в главном меню в разделе \"Скины\"",
                "их можно обменять на уникальные образы для белки!",
                "",
                "Огонь смертелен! Тушите его до того, как перепрыгнуть."
            };

            float y = SH / 3.2f;
            for (const auto& line : lines) {
                Vector2 ls = MeasureTextEx(font, line.c_str(), 24, 1);
                DrawTextEx(font, line.c_str(), { SW / 2.0f - ls.x / 2, y }, 24, 1, LIGHTGRAY);
                y += 38;
            }
            const char* hint = "Enter или ESC — назад в меню";
            Vector2 hs = MeasureTextEx(font, hint, 20, 1);
            DrawTextEx(font, hint, { SW / 2.0f - hs.x / 2, (float)SH - 60 }, 20, 1, GRAY);
        }
        else if (state == WIN) {
            const char* l1 = "Лес спасён!";
            const char* l2 = "Вандалы и лесорубы отступили!";
            const char* l3 = TextFormat("Собрано орешков: %d", sd.totalNuts);
            const char* l4 = "Нажмите Enter или Пробел, чтобы вернуться в меню";
            Vector2 s1 = MeasureTextEx(font, l1, 54, 2);
            Vector2 s2 = MeasureTextEx(font, l2, 32, 2);
            Vector2 s3 = MeasureTextEx(font, l3, 32, 2);
            DrawTextEx(font, l1, { SW / 2.0f - s1.x / 2, SH / 2.0f - 120 }, 54, 2, GOLD);
            DrawTextEx(font, l2, { SW / 2.0f - s2.x / 2, SH / 2.0f - 50 }, 32, 2, WHITE);
            DrawTextEx(font, l3, { SW / 2.0f - s3.x / 2, SH / 2.0f + 10 }, 32, 2, YELLOW);
            Vector2 s4 = MeasureTextEx(font, l4, 22, 1);
            DrawTextEx(font, l4, { SW / 2.0f - s4.x / 2, SH / 2.0f + 80 }, 22, 1, LIGHTGRAY);
        }
        else if (state == GAMEOVER) {
            const char* l1 = "Игра окончена";
            const char* l2 = TextFormat("Орешков собрано: %d", sd.totalNuts);
            const char* l3 = "Нажмите Enter или Пробел для возврата в меню";
            Vector2 s1 = MeasureTextEx(font, l1, 54, 2);
            Vector2 s2 = MeasureTextEx(font, l2, 32, 2);
            Vector2 s3 = MeasureTextEx(font, l3, 22, 1);
            DrawTextEx(font, l1, { SW / 2.0f - s1.x / 2, SH / 2.0f - 100 }, 54, 2, RED);
            DrawTextEx(font, l2, { SW / 2.0f - s2.x / 2, SH / 2.0f - 20 }, 32, 2, WHITE);
            DrawTextEx(font, l3, { SW / 2.0f - s3.x / 2, SH / 2.0f + 50 }, 22, 1, LIGHTGRAY);
        }

        EndDrawing();
    }

    if (bgTexture.id > 0) UnloadTexture(bgTexture);
    if (menuTexture.id > 0) UnloadTexture(menuTexture);
    if (fireTexture.id > 0) UnloadTexture(fireTexture);
    if (blockTexture.id > 0) UnloadTexture(blockTexture);
    if (unfireTexture.id > 0) UnloadTexture(unfireTexture);
    for (auto& st : skinTextures) {
        if (st.still.id > 0) UnloadTexture(st.still);
        if (st.spritesheet.id > 0) UnloadTexture(st.spritesheet);
    }
    SaveGame(sd);
    UnloadFont(font);
    CloseWindow();
    return 0;
}