#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#define DEFAULT_MAP_WIDTH 96
#define DEFAULT_MAP_HEIGHT 30
#define MAX_MAP_WIDTH DEFAULT_MAP_WIDTH
#define MAX_MAP_HEIGHT DEFAULT_MAP_HEIGHT

#define MAX_ENEMIES 96
#define MAX_PROJECTILES 96
#define MAX_PICKUPS 96
#define UPGRADE_CHOICES 3
#define MAX_RANKINGS 5
#define MAX_NAME_LEN 15

#define SURVIVAL_SECONDS 600.0   /* PPT: 10분 */

typedef enum AppScreen {
    SCREEN_TITLE = 0,
    SCREEN_SETUP,
    SCREEN_RANKING,
    SCREEN_GAME,
    SCREEN_NAME_INPUT
} AppScreen;

typedef enum GameMode {
    GAME_MODE_PLAYING = 0,
    GAME_MODE_PAUSED,
    GAME_MODE_LEVEL_UP,
    GAME_MODE_GAME_OVER,
    GAME_MODE_VICTORY
} GameMode;

typedef enum EnemyType {
    ENEMY_ONE_HP = 0,
    ENEMY_THREE_HP,
    ENEMY_FORTY_HP,
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
    SOUND_ATTACK = 1u << 0,
    SOUND_XP = 1u << 1,
    SOUND_LEVEL_UP = 1u << 2,
    SOUND_HIT = 1u << 3,
    SOUND_GAME_OVER = 1u << 4,
    SOUND_VICTORY = 1u << 5
} SoundFlag;

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
    int hpRecoveryLevel;    /* 0=없음, 1~3=레벨 */
    float hpRecoveryTimer;  /* 다음 회복까지 남은 시간 */
    /* 패시브 아이템: 무적의 방패 */
    int shieldLevel;        /* 0=없음, 1~3=레벨 */
    float shieldTimer;      /* 남은 무적 시간 (>0 이면 무적) */
    float shieldCooldown;   /* 다음 방패 발동까지 남은 시간 */
    /* 속도 배율 */
    float attackSpeedMult;  /* 1.0 = 기본, 1.2 = +20%, 1.44 = +40% */
    float moveSpeedMult;    /* 이동속도 배율 */
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
    char glyph;
} Projectile;

typedef struct Pickup {
    bool active;
    Vec2 position;
    int value;
    float moveCooldown;
} Pickup;

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
    WeaponType activeWeapon;    /* 현재 장착 중인 무기 (PPT: 무기 1개) */
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
    float auraPulseTimer;
    float speedWarningTimer;  /* >0 이면 "몬스터 강력" 경고 표시 */
    int   lastSpeedStep;      /* 마지막으로 감지한 속도 단계 */
    unsigned int pendingSounds;
} Game;

typedef struct RankingEntry {
    int score;
    int seconds;
    int kills;
    int level;
    char result[16];
    char name[MAX_NAME_LEN + 1];
} RankingEntry;

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
void GameRequestSound(Game *game, unsigned int flags);

void PlayerUpdate(Game *game, const InputState *input, float dt);

void EnemiesSpawnWave(Game *game);
void EnemiesUpdate(Game *game, float dt);
void EnemiesDamageInRadius(Game *game, Vec2 center, int radius, int damage);

void PickupSpawn(Game *game, Vec2 position, int value);
void PickupsUpdate(Game *game, float dt);

void WeaponsUpdate(Game *game, float dt);
void ProjectilesUpdate(Game *game, float dt);
void CombatResolve(Game *game);

void UiDrawTitle(void);
void UiDrawSetup(GameDifficulty selectedDifficulty);
void UiDrawRanking(const RankingEntry entries[MAX_RANKINGS], int count);
void UiDrawNameInput(const Game *game, const char *name, int nameLen);
void UiDrawGame(const Game *game);
void UiPlaySounds(unsigned int flags, bool enabled);

void RankingLoad(RankingEntry entries[MAX_RANKINGS], int *count);
void RankingAddAndSave(const Game *game, const char *name, const char *result);

#endif
