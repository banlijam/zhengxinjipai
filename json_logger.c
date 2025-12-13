#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "data_struct.h"

// 简单的字符串哈希函数
unsigned long long StringHash(const wchar_t *str)
{
    unsigned long long hash = 5381;
    int c;
    
    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash;
}

// 检查JSON行是否已缓存
BOOL IsJsonLineCached(GlobalData *data, const wchar_t *line)
{
    if (!data || !line || !data->jsonLineCache)
        return FALSE;
    
    unsigned long long hash = StringHash(line);
    
    for (int i = 0; i < data->jsonLineCacheCount; i++)
    {
        if (data->jsonLineCache[i].hash == hash)
        {
            return TRUE;
        }
    }
    
    return FALSE;
}

// 添加JSON行到缓存
void AddJsonLineToCache(GlobalData *data, const wchar_t *line)
{
    if (!data || !line)
        return;
    
    unsigned long long hash = StringHash(line);
    
    // 扩展缓存数组
    if (data->jsonLineCacheCount >= data->jsonLineCacheCapacity)
    {
        int newCapacity = data->jsonLineCacheCapacity == 0 ? 100 : data->jsonLineCacheCapacity * 2;
        JsonLineCache *newCache = realloc(data->jsonLineCache, newCapacity * sizeof(JsonLineCache));
        if (!newCache)
            return;
        data->jsonLineCache = newCache;
        data->jsonLineCacheCapacity = newCapacity;
    }
    
    // 添加哈希值到缓存
    data->jsonLineCache[data->jsonLineCacheCount].hash = hash;
    data->jsonLineCacheCount++;
}

// 添加函数声明
void ProcessJsonOperationCards(GlobalData *data, const JsonOperation *operation);
BOOL IsPoolInCache(GlobalData *data, const wchar_t *pool);
void AddPoolToCache(GlobalData *data, const wchar_t *pool);
void ProcessCardName(GlobalData *data, const wchar_t *cardName);
void AdjustCardCount(GlobalData *data, const wchar_t *pool, const wchar_t *cardName, int level, int adjustment);

// 初始化 JSON 日志监控
void InitializeJsonLogging(GlobalData *data)
{
    if (!data)
        return;

    // 构建日志文件路径
    wchar_t username[100];
    DWORD usernameLen = 100;
    GetUserNameW(username, &usernameLen);

    swprintf(data->logFilePath, MAX_PATH,
             L"C:\\Users\\%s\\AppData\\LocalLow\\DarkSunStudio\\YiXianPai\\CardOperationLog.json",
             username);

    data->currentFileSize = 0;
    data->currentGameId = 0;
    data->operations = NULL;
    data->operationCount = 0;
    data->operationCapacity = 0;
    data->jsonLineCache = NULL;
    data->jsonLineCacheCount = 0;
    data->jsonLineCacheCapacity = 0;

    // 初始读取文件
    CheckJsonLogUpdates(data);
}

// 检查 JSON 日志更新
void CheckJsonLogUpdates(GlobalData *data)
{
    if (!data)
        return;

    FILE *file = _wfopen(data->logFilePath, L"r, ccs=UTF-8");
    if (!file)
    {
        // 文件不存在或无法打开
        data->currentFileSize = 0;
        data->currentGameId = 0;
        FreeJsonOperations(data);
        return;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 如果文件大小变化，重新解析
    if (fileSize != data->currentFileSize)
    {
        ParseJsonLogFile(data);
        data->currentFileSize = fileSize;
    }

    fclose(file);
}

// 解析 JSON 日志文件
void ParseJsonLogFile(GlobalData *data)
{
    if (!data)
        return;

    FILE *file = _wfopen(data->logFilePath, L"r, ccs=UTF-8");
    if (!file)
        return;

    // 读取第一行（对局ID）
    wchar_t firstLine[100];
    if (fgetws(firstLine, 100, file))
    {
        // 移除换行符
        firstLine[wcslen(firstLine) - 1] = L'\0';
        int newGameId = _wtoi(firstLine);

        // 检查是否是新对局
        if (newGameId != data->currentGameId && newGameId > 0)
        {
            ProcessNewGame(data, newGameId);
        }
    }

    // 读取后续的 JSON 行
    wchar_t lineBuffer[4096];
    
    // 记录已读取的行数
    int lineCount = 0;
    
    while (fgetws(lineBuffer, 4096, file))
    {
        lineCount++;
        
        // 跳过空行
        if (wcslen(lineBuffer) <= 1)
            continue;

        // 移除换行符
        lineBuffer[wcslen(lineBuffer) - 1] = L'\0';
        
        // 检查是否已缓存该JSON行
        if (IsJsonLineCached(data, lineBuffer))
        {
            // 已读取过，跳过
            continue;
        }

        // 转换为 UTF-8 进行 cJSON 解析
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, lineBuffer, -1, NULL, 0, NULL, NULL);
        char *utf8Line = (char *)malloc(utf8Len);
        WideCharToMultiByte(CP_UTF8, 0, lineBuffer, -1, utf8Line, utf8Len, NULL, NULL);

        // 解析 JSON
        cJSON *json = cJSON_Parse(utf8Line);
        if (json)
        {
            // 扩展操作记录数组
            if (data->operationCount >= data->operationCapacity)
            {
                int newCapacity = data->operationCapacity == 0 ? 10 : data->operationCapacity * 2;
                JsonOperation *newOps = realloc(data->operations, newCapacity * sizeof(JsonOperation));
                if (newOps)
                {
                    data->operations = newOps;
                    data->operationCapacity = newCapacity;
                }
            }

            if (data->operationCount < data->operationCapacity)
            {
                JsonOperation *op = &data->operations[data->operationCount];

                // 初始化操作记录
                memset(op, 0, sizeof(JsonOperation));

                // 解析基本字段
                cJSON *operation = cJSON_GetObjectItem(json, "operation");
                cJSON *round = cJSON_GetObjectItem(json, "round");

                if (operation)
                    op->operation = operation->valueint;
                if (round)
                    op->round = round->valueint;

                // 解析 srcCard
                cJSON *srcCard = cJSON_GetObjectItem(json, "srcCard");
                if (srcCard)
                {
                    cJSON *name = cJSON_GetObjectItem(srcCard, "name");
                    cJSON *level = cJSON_GetObjectItem(srcCard, "level");
                    cJSON *rarity = cJSON_GetObjectItem(srcCard, "rarity");

                    if (name && name->valuestring)
                    {
                        wchar_t *wideName = UTF8ToWide(name->valuestring);
                        SafeWcsncpy(op->srcCard.name, wideName, 100);
                        free(wideName);
                    }
                    if (level)
                        op->srcCard.level = level->valueint;
                    if (rarity)
                        op->srcCard.rarity = rarity->valueint;
                }

                // 解析 dstCard
                cJSON *dstCard = cJSON_GetObjectItem(json, "dstCard");
                if (dstCard)
                {
                    cJSON *name = cJSON_GetObjectItem(dstCard, "name");
                    cJSON *level = cJSON_GetObjectItem(dstCard, "level");
                    cJSON *rarity = cJSON_GetObjectItem(dstCard, "rarity");

                    if (name && name->valuestring)
                    {
                        wchar_t *wideName = UTF8ToWide(name->valuestring);
                        SafeWcsncpy(op->dstCard.name, wideName, 100);
                        free(wideName);
                    }
                    if (level)
                        op->dstCard.level = level->valueint;
                    if (rarity)
                        op->dstCard.rarity = rarity->valueint;
                }

                // 解析 cards 数组
                cJSON *cards = cJSON_GetObjectItem(json, "cards");
                op->cards = NULL;
                op->cardCount = 0;

                if (cards && cJSON_IsArray(cards))
                {
                    int arraySize = cJSON_GetArraySize(cards);
                    op->cards = malloc(arraySize * sizeof(JsonCard));
                    op->cardCount = arraySize;

                    for (int i = 0; i < arraySize; i++)
                    {
                        cJSON *cardItem = cJSON_GetArrayItem(cards, i);
                        if (cardItem)
                        {
                            cJSON *name = cJSON_GetObjectItem(cardItem, "name");
                            cJSON *level = cJSON_GetObjectItem(cardItem, "level");
                            cJSON *rarity = cJSON_GetObjectItem(cardItem, "rarity");

                            if (name && name->valuestring)
                            {
                                wchar_t *wideName = UTF8ToWide(name->valuestring);
                                SafeWcsncpy(op->cards[i].name, wideName, 100);
                                free(wideName);
                            }
                            if (level)
                                op->cards[i].level = level->valueint;
                            if (rarity)
                                op->cards[i].rarity = rarity->valueint;
                        }
                    }
                }

                ProcessJsonOperationCards(data, op);
                data->operationCount++;
                
                // 添加到JSON行缓存
                AddJsonLineToCache(data, lineBuffer);
            }

            cJSON_Delete(json);
        }

        free(utf8Line);
    }

    fclose(file);

    // 调试输出
    wchar_t debugMsg[256];
    swprintf(debugMsg, 256, L"解析完成: 对局ID=%d, 操作记录=%d", data->currentGameId, data->operationCount);
    OutputDebugStringW(debugMsg);
}

// 修改 ProcessNewGame 函数
void ProcessNewGame(GlobalData *data, int gameId)
{
    if (!data)
        return;

    // 清空之前的操作记录和当前对局数据
    FreeJsonOperations(data);

    // 清空当前对局卡牌数据
    if (data->currentGame.cards)
    {
        free(data->currentGame.cards);
        data->currentGame.cards = NULL;
    }
    data->currentGame.cardCount = 0;
    data->currentGame.cardCapacity = 0;

    // 清空卡池缓存
    if (data->poolCache)
    {
        free(data->poolCache);
        data->poolCache = NULL;
    }
    data->poolCacheCount = 0;
    data->poolCacheCapacity = 0;
    
    // 清空JSON行缓存
    if (data->jsonLineCache)
    {
        free(data->jsonLineCache);
        data->jsonLineCache = NULL;
    }
    data->jsonLineCacheCount = 0;
    data->jsonLineCacheCapacity = 0;

    data->currentGameId = gameId;

    // 调试输出
    wchar_t debugMsg[256];
    swprintf(debugMsg, 256, L"新对局开始: ID=%d", gameId);
    OutputDebugStringW(debugMsg);
}

// 释放 JSON 操作记录
void FreeJsonOperations(GlobalData *data)
{
    if (!data)
        return;

    if (data->operations)
    {
        for (int i = 0; i < data->operationCount; i++)
        {
            if (data->operations[i].cards)
            {
                free(data->operations[i].cards);
            }
        }

        free(data->operations);
        data->operations = NULL;
        data->operationCount = 0;
        data->operationCapacity = 0;
    }
    
    // 释放JSON行缓存
    if (data->jsonLineCache)
    {
        free(data->jsonLineCache);
        data->jsonLineCache = NULL;
        data->jsonLineCacheCount = 0;
        data->jsonLineCacheCapacity = 0;
    }
}

// 检查卡池是否已在缓存中
BOOL IsPoolInCache(GlobalData *data, const wchar_t *pool)
{
    if (!data || !data->poolCache || !pool)
        return FALSE;

    for (int i = 0; i < data->poolCacheCount; i++)
    {
        if (wcscmp(data->poolCache[i].pool, pool) == 0)
        {
            return data->poolCache[i].added;
        }
    }
    return FALSE;
}

// 添加卡池到缓存
void AddPoolToCache(GlobalData *data, const wchar_t *pool)
{
    if (!data || !pool)
        return;

    // 检查是否已存在
    for (int i = 0; i < data->poolCacheCount; i++)
    {
        if (wcscmp(data->poolCache[i].pool, pool) == 0)
        {
            data->poolCache[i].added = TRUE;
            return;
        }
    }

    // 扩展缓存数组
    if (data->poolCacheCount >= data->poolCacheCapacity)
    {
        int newCapacity = data->poolCacheCapacity == 0 ? 10 : data->poolCacheCapacity * 2;
        PoolCache *newCache = realloc(data->poolCache, newCapacity * sizeof(PoolCache));
        if (!newCache)
            return;
        data->poolCache = newCache;
        data->poolCacheCapacity = newCapacity;
    }

    // 添加新卡池
    SafeWcsncpy(data->poolCache[data->poolCacheCount].pool, pool, 50);
    data->poolCache[data->poolCacheCount].added = TRUE;
    data->poolCacheCount++;
}

// 添加卡池的所有卡牌到当前对局
void AddPoolToCurrentGame(GlobalData *data, const wchar_t *poolName)
{
    if (!data || !poolName)
        return;

    // 在卡池到卡牌映射中查找
    PoolToCards *poolMapping = FindPoolToCards(poolName);
    if (!poolMapping)
    {
        wchar_t debugMsg[256];
        swprintf(debugMsg, 256, L"未找到卡池映射: %s", poolName);
        OutputDebugStringW(debugMsg);
        return;
    }

    // 添加该卡池的所有卡牌到当前对局，按照原始顺序，包括空白卡牌
    for (int i = 0; i < poolMapping->cardCount; i++)
    {
        // 扩展数组
        if (data->currentGame.cardCount >= data->currentGame.cardCapacity)
        {
            int newCapacity = data->currentGame.cardCapacity == 0 ? 50 : data->currentGame.cardCapacity * 2;
            Card *newCards = realloc(data->currentGame.cards, newCapacity * sizeof(Card));
            if (!newCards)
                return;
            data->currentGame.cards = newCards;
            data->currentGame.cardCapacity = newCapacity;
        }

        // 添加卡牌到数组末尾
        Card *target = &data->currentGame.cards[data->currentGame.cardCount];
        SafeWcsncpy(target->pool, poolMapping->cards[i].pool, 50);
        SafeWcsncpy(target->name, poolMapping->cards[i].name, 50);
        SafeWcsncpy(target->level, poolMapping->cards[i].level, 10);
        SafeWcsncpy(target->count, poolMapping->cards[i].count, 10);
        data->currentGame.cardCount++;
    }

    // 标记卡池为已添加
    AddPoolToCache(data, poolName);

    // 设置卡池变动标记
    data->poolChanged = TRUE;

    // 调试输出
    wchar_t debugMsg[256];
    swprintf(debugMsg, 256, L"添加卡池 %s 的 %d 张卡牌到当前对局", poolName, poolMapping->cardCount);
    OutputDebugStringW(debugMsg);
}

// 调整卡牌数量
void AdjustCardCount(GlobalData *data, const wchar_t *pool, const wchar_t *cardName, int level, int adjustment)
{
    if (!data || !pool || !cardName)
        return;

    // 将等级转换为字符串
    wchar_t levelStr[10];
    swprintf(levelStr, 10, L"%d", level);

    // 查找卡牌
    for (int i = 0; i < data->currentGame.cardCount; i++)
    {
        if (wcscmp(data->currentGame.cards[i].pool, pool) == 0 &&
            wcscmp(data->currentGame.cards[i].name, cardName) == 0 &&
            wcscmp(data->currentGame.cards[i].level, levelStr) == 0)
        {
            // 获取当前数量
            int currentCount = _wtoi(data->currentGame.cards[i].count);

            // 调整数量
            currentCount += adjustment;

            // 更新数量（允许负数）
            wchar_t newCount[10];
            swprintf(newCount, 10, L"%d", currentCount);
            SafeWcsncpy(data->currentGame.cards[i].count, newCount, 10);

            // 调试输出
            wchar_t debugMsg[256];
            swprintf(debugMsg, 256, L"调整卡牌数量: %s (%s) Lv%d %d -> %d",
                     cardName, pool, level, currentCount - adjustment, currentCount);
            OutputDebugStringW(debugMsg);

            return;
        }
    }

    // 如果卡牌不存在，输出调试信息但不添加
    wchar_t debugMsg[256];
    swprintf(debugMsg, 256, L"调整卡牌数量失败: 未找到卡牌 %s (%s) Lv%d", cardName, pool, level);
    OutputDebugStringW(debugMsg);
}

// 处理卡牌名称，只检查并添加卡池
void ProcessCardName(GlobalData *data, const wchar_t *cardName)
{
    if (!data || !cardName || wcslen(cardName) == 0)
        return;

    // 在名称到卡池映射中查找
    NameToPool *nameMapping = FindNameToPool(cardName);
    if (!nameMapping)
    {
        // 调试输出未找到的卡牌
        wchar_t debugMsg[256];
        swprintf(debugMsg, 256, L"未找到卡牌映射: %s", cardName);
        OutputDebugStringW(debugMsg);
        return;
    }

    // 获取卡池名称
    const wchar_t *poolName = nameMapping->pool;

    // 检查卡池是否已处理
    if (IsPoolInCache(data, poolName))
    {
        return; // 该卡池已添加过
    }

    // 添加整个卡池
    AddPoolToCurrentGame(data, poolName);
}

// 处理 JSON 操作记录中的卡牌
void ProcessJsonOperationCards(GlobalData *data, const JsonOperation *operation)
{
    if (!data || !operation)
        return;

    // 先处理卡牌名称映射（确保卡池已添加）
    // 处理 srcCard
    if (operation->srcCard.name[0] != L'\0')
    {
        ProcessCardName(data, operation->srcCard.name);
    }

    // 处理 dstCard
    if (operation->dstCard.name[0] != L'\0')
    {
        ProcessCardName(data, operation->dstCard.name);
    }

    // 处理 cards 数组
    if (operation->cards && operation->cardCount > 0)
    {
        for (int i = 0; i < operation->cardCount; i++)
        {
            if (operation->cards[i].name[0] != L'\0')
            {
                ProcessCardName(data, operation->cards[i].name);
            }
        }
    }

    // 然后根据操作类型调整卡牌数量
    switch (operation->operation)
    {
    case 0: // operation 0: cards 数组中的每张卡牌数量减1
        if (operation->cards && operation->cardCount > 0)
        {
            for (int i = 0; i < operation->cardCount; i++)
            {
                if (operation->cards[i].name[0] != L'\0')
                {
                    // 在名称到卡池映射中查找卡池
                    NameToPool *nameMapping = FindNameToPool(operation->cards[i].name);
                    if (nameMapping)
                    {
                        AdjustCardCount(data, nameMapping->pool, operation->cards[i].name, operation->cards[i].level, -1);
                    }
                }
            }
        }
        break;

    case 1: // operation 1: srcCard 数量减2, dstCard 数量减1
        if (operation->srcCard.name[0] != L'\0')
        {
            NameToPool *srcMapping = FindNameToPool(operation->srcCard.name);
            if (srcMapping)
            {
                AdjustCardCount(data, srcMapping->pool, operation->srcCard.name, operation->srcCard.level, -2);
            }
        }
        if (operation->dstCard.name[0] != L'\0')
        {
            NameToPool *dstMapping = FindNameToPool(operation->dstCard.name);
            if (dstMapping)
            {
                AdjustCardCount(data, dstMapping->pool, operation->dstCard.name, operation->dstCard.level, -1);
            }
        }
        break;

    case 2: // operation 2: srcCard 数量减1
        // 暂时不处理
        break;

    default:
        // 其他操作类型，暂时不处理
        break;
    }
}