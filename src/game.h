#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#include "game_config.h"

typedef enum GameMode {
    GAME_MODE_PLAYING = 0,
    GAME_MODE_PAUSED,
    GAME_MODE_LEVEL_UP,
    GAME_MODE_GAME_OVER,
    GAME_MODE_VICTORY
} GameMode;

typedef enum EnemyType {
    ENEMY_BAT = 0,
    ENEMY_ZOMBIE,
    ENEMY_VAMPIRE,
    ENEMY_TYPE_COUNT
} EnemyType;

typedef enum GameDifficulty {
    DIFFICULTY_EASY = 0,
    DIFFICULTY_HARD
} GameDifficulty;

typedef enum WeaponType {
    WEAPON_MAGIC_BOLT = 0,
    WEAPON_HOLY_AURA,
    WEAPON_PIERCING_LANCE,
    WEAPON_STAR_BURST,
    WEAPON_COUNT
} WeaponType;

typedef enum SoundFlag {
    SOUND_UI_MOVE = 1u << 0,
    SOUND_UI_CONFIRM = 1u << 1,
    SOUND_SHOOT = 1u << 2,
    SOUND_XP_PICKUP = 1u << 3,
    SOUND_HEAL_PICKUP = 1u << 4,
    SOUND_POWER_PICKUP = 1u << 5,
    SOUND_LEVEL_UP = 1u << 6,
    SOUND_HIT = 1u << 7,
    SOUND_GAME_OVER = 1u << 8,
    SOUND_VICTORY = 1u << 9
} SoundFlag;

typedef enum PickupType {
    PICKUP_XP = 0, // 0이라고 정의 안해줘도 됨(나중에 없애는게 더 깔끔할 듯)
    PICKUP_TREASURE_CHEST
} PickupType;

typedef enum MiniEventType {
    MINI_EVENT_NONE = 0,
    MINI_EVENT_BAT_STORM
} MiniEventType;

typedef struct InputState {
    bool up;
    bool down;
    bool left;
    bool right;
    bool select;
    bool back;
    bool start;
    bool ranking;
    bool restart;
    bool quit;
    bool escape;
    bool pauseToggle;
    bool muteToggle;
    int number;
    char typedChar;   /* 이름 입력용 일반 문자 */
} InputState;

typedef struct Vec2 {
    float x;
    float y;
} Vec2;

typedef struct Player {
    Vec2 position;
    int maxHealth;
    int health;
    int level;
    int xp;
    int xpToNextLevel;
    int score;
    int kills;
    float moveCooldown;
    float invulnerableTimer;
    int magnetRange;
    /* 패시브 아이템: 회복의 붕대 */
    int hpRecoveryLevel;    /* 0=없음, 1=활성 */
    int bandageKillCount;   /* 다음 회복까지 누적 킬 수 */
    float hpRecoveryTimer;  /* 활성 남은 시간 (획득 시 60초, 0되면 소멸) */
    /* 패시브 아이템: 무적의 방패 */
    int shieldLevel;        /* 0=없음, 1=보유 */
    float shieldTimer;      /* 남은 무적 시간 (>0 이면 무적) */
    /* 속도 배율 */
    float attackSpeedMult;
    float moveSpeedMult;
} Player;

typedef struct Enemy {
    bool active;
    EnemyType type;
    Vec2 position;
    int health;
    int maxHealth;
    int damage;
    int xpValue;
    int scoreValue;
    float moveCooldown;
    float moveDelay;
    char glyph;
} Enemy;

typedef struct Projectile {
    bool active;
    Vec2 position;
    Vec2 velocity;
    int damage;
    float lifetime;
    int pierce;
    int areaHit;       /* 0=단일, 1=범위(3x3) */
    char glyph;
} Projectile;

typedef struct Pickup {
    bool active;
    PickupType type;
    Vec2 position;
    int value;
    float moveCooldown;
} Pickup;

/* 혈사포 레이저 시각 효과 */
typedef struct Laser {
    bool active;
    bool goLeft;
    bool goRight;
    bool goUp;
    bool goDown;
    int playerX;   /* 발사 시점 플레이어 X */
    int playerY;   /* 발사 시점 플레이어 Y */
    float timer;
} Laser;

typedef struct Weapon {
    WeaponType type;
    int level;
    int damage;
    int projectileCount;
    int range;
    float cooldown;
    float timer;
    char glyph;
} Weapon;

typedef struct UpgradeOption {
    const char *name;
    const char *description;
    WeaponType weapon;
    int kind;
} UpgradeOption;

typedef struct Game {
    GameMode mode;
    GameDifficulty difficulty;
    int mapWidth;
    int mapHeight;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    Projectile projectiles[MAX_PROJECTILES];
    Pickup pickups[MAX_PICKUPS];
    Weapon weapons[WEAPON_COUNT];
    WeaponType activeWeapon;    /* 현재 장착 중인 무기 */
    UpgradeOption upgrades[UPGRADE_CHOICES];
    int selectedUpgrade;
    float elapsed;
    float spawnTimer;
    float spawnInterval;
    float spawnStartInterval;
    float spawnRampPerSecond;
    float spawnMinInterval;
    float midEnemyStart;
    float highEnemyStart;
    int midEnemyChance;
    int highEnemyChance;
    float speedWarningTimer;  /* >0 이면 "몬스터 강력" 경고 표시 */
    int   lastSpeedStep;      /* 마지막으로 감지한 속도 단계 */
    MiniEventType activeMiniEvent;
    float nextMiniEventTime;
    float miniEventTimer;
    float miniEventMessageTimer;
    int miniEventVariant;
    unsigned int pendingSounds;
    Laser laser;
} Game;

float GameDistanceSquared(Vec2 a, Vec2 b);
Vec2 GameNormalize(Vec2 v);
Vec2 GameAdd(Vec2 a, Vec2 b);
Vec2 GameScale(Vec2 v, float scale);
int GameRound(float value);
int GameClampInt(int value, int min, int max);
bool GameMapIsBlocked(const Game *game, int x, int y);
char GameMapTile(const Game *game, int x, int y);
const char *GameDifficultyName(GameDifficulty difficulty);

void GameInit(Game *game, GameDifficulty difficulty);
void GameUpdate(Game *game, const InputState *input, float dt);
void GameApplyUpgrade(Game *game, int index);
void GameStartRewardChoice(Game *game);
void GameRequestSound(Game *game, unsigned int flags);

void PlayerUpdate(Game *game, const InputState *input, float dt);

void EnemiesSpawnWave(Game *game);
void EnemiesSpawnBatStorm(Game *game, int count);
void EnemiesUpdate(Game *game, float dt);
void EnemiesDamageInRadius(Game *game, Vec2 center, int radius, int damage);
void EnemyDefeat(Game *game, Enemy *enemy);

void PickupSpawn(Game *game, Vec2 position, int value);
void PickupSpawnTyped(Game *game, Vec2 position, PickupType type, int value);
void PickupsUpdate(Game *game, float dt);

void WeaponsUpdate(Game *game, float dt);
void ProjectilesUpdate(Game *game, float dt);
void CombatResolve(Game *game);

#endif
