#ifndef DATA_STRUCT_H
#define DATA_STRUCT_H

#include <windows.h>

// 最大卡牌数量
#define MAX_CARDS 500

// 卡牌数据结构定义
typedef struct
{
    wchar_t pool[50];  // 卡池
    wchar_t name[50];  // 名称
    wchar_t level[10]; // 等级
    wchar_t count[10]; // 初始张数
} Card;

// 名称到卡池的映射结构
typedef struct
{
    wchar_t name[50]; // 卡牌名称
    wchar_t pool[50]; // 所属卡池
} NameToPool;

// 卡池到卡牌列表的映射结构
typedef struct
{
    wchar_t pool[50]; // 卡池名称
    Card *cards;      // 该卡池下的卡牌数组
    int cardCount;    // 该卡池下的卡牌数量
    int capacity;     // 数组容量
} PoolToCards;

// JSON 日志中的卡牌信息
typedef struct
{
    wchar_t name[100]; // 卡牌名称
    int level;         // 等级
    int rarity;        // 稀有度
} JsonCard;

// JSON 操作记录
typedef struct
{
    int operation;    // 操作类型
    int round;        // 回合数
    JsonCard srcCard; // 源卡牌
    JsonCard dstCard; // 目标卡牌
    JsonCard *cards;  // 卡牌数组
    int cardCount;    // 卡牌数量
} JsonOperation;

// 卡池缓存结构
typedef struct
{
    wchar_t pool[50]; // 卡池名称
    BOOL added;       // 是否已添加到当前对局
} PoolCache;

// JSON行缓存结构
typedef struct
{
    unsigned long long hash; // JSON行的哈希值
} JsonLineCache;

// 当前对局卡牌数据结构
typedef struct
{
    Card *cards;      // 当前对局的卡牌数组
    int cardCount;    // 卡牌数量
    int cardCapacity; // 数组容量
} CurrentGameCards;

// 全局数据结构
typedef struct
{
    Card *allCards;      // 所有卡牌的数组
    int allCardCount;    // 总卡牌数量
    int allCardCapacity; // 所有卡牌数组容量

    NameToPool *nameToPoolMap; // 名称到卡池的映射
    int nameToPoolCount;       // 映射数量
    int nameToPoolCapacity;    // 映射数组容量

    PoolToCards *poolToCardsMap; // 卡池到卡牌列表的映射
    int poolToCardsCount;        // 卡池数量
    int poolToCardsCapacity;     // 卡池数组容量

    // JSON 日志相关
    wchar_t logFilePath[MAX_PATH]; // 日志文件路径
    long currentFileSize;          // 当前文件大小
    int currentGameId;             // 当前对局ID
    JsonOperation *operations;     // 操作记录数组
    int operationCount;            // 操作记录数量
    int operationCapacity;         // 操作记录容量
    CurrentGameCards currentGame;  // 当前对局卡牌数据
    PoolCache *poolCache;          // 卡池缓存
    int poolCacheCount;            // 缓存数量
    int poolCacheCapacity;         // 缓存容量

    BOOL poolChanged;  // 卡池是否发生变动
    int lastCardCount; // 上一次的卡牌数量
    int lastOperationCount; // 上一次的操作记录数量
    JsonLineCache *jsonLineCache; // JSON行缓存
    int jsonLineCacheCount;       // 缓存数量
    int jsonLineCacheCapacity;    // 缓存容量
} GlobalData;

// 简单的字符串哈希函数
unsigned long long StringHash(const wchar_t *str);
BOOL IsJsonLineCached(GlobalData *data, const wchar_t *line);
void AddJsonLineToCache(GlobalData *data, const wchar_t *line);

// 全局变量声明
extern GlobalData g_GlobalData;
extern HWND g_hMainWindow;

// 函数声明
// 数据加载函数
BOOL LoadEmbeddedData(GlobalData *data);
void FreeGlobalData(GlobalData *data);

// JSON 日志处理函数
void InitializeJsonLogging(GlobalData *data);
BOOL CheckJsonLogUpdates(GlobalData *data);
void ParseJsonLogFile(GlobalData *data);
void FreeJsonOperations(GlobalData *data);
void ProcessNewGame(GlobalData *data, int gameId);
void ProcessJsonOperationCards(GlobalData *data, const JsonOperation *operation);

// 工具函数
wchar_t *UTF8ToWide(const char *utf8_str);
void SafeWcsncpy(wchar_t *dest, const wchar_t *src, size_t dest_size);
NameToPool *FindNameToPool(const wchar_t *name);
PoolToCards *FindPoolToCards(const wchar_t *pool);
void AddCardToPoolMap(const wchar_t *pool, const Card *card);
void AddNameToPoolMap(const wchar_t *name, const wchar_t *pool);

#endif