#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "data_struct.h"

// 声明嵌入的CSV数据
extern const char embedded_csv_data[];

// 使用 extern 声明全局变量
extern GlobalData g_GlobalData;

// UTF-8 到宽字符转换函数
wchar_t *UTF8ToWide(const char *utf8_str)
{
    if (!utf8_str || strlen(utf8_str) == 0)
    {
        wchar_t *empty = (wchar_t *)malloc(sizeof(wchar_t));
        if (empty)
            empty[0] = L'\0';
        return empty;
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (wide_len == 0)
        return NULL;

    wchar_t *wide_str = (wchar_t *)malloc(wide_len * sizeof(wchar_t));
    if (!wide_str)
        return NULL;

    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wide_str, wide_len);
    return wide_str;
}

// 安全的字符串复制函数
void SafeWcsncpy(wchar_t *dest, const wchar_t *src, size_t dest_size)
{
    if (dest && dest_size > 0)
    {
        if (src && wcslen(src) > 0)
        {
            wcsncpy(dest, src, dest_size - 1);
        }
        else
        {
            dest[0] = L'\0';
        }
        dest[dest_size - 1] = L'\0';
    }
}

// 在名称到卡池映射中查找
NameToPool *FindNameToPool(const wchar_t *name)
{
    for (int i = 0; i < g_GlobalData.nameToPoolCount; i++)
    {
        if (wcscmp(g_GlobalData.nameToPoolMap[i].name, name) == 0)
        {
            return &g_GlobalData.nameToPoolMap[i];
        }
    }
    return NULL;
}

// 在卡池到卡牌映射中查找
PoolToCards *FindPoolToCards(const wchar_t *pool)
{
    for (int i = 0; i < g_GlobalData.poolToCardsCount; i++)
    {
        if (wcscmp(g_GlobalData.poolToCardsMap[i].pool, pool) == 0)
        {
            return &g_GlobalData.poolToCardsMap[i];
        }
    }
    return NULL;
}

// 添加卡牌到卡池映射
void AddCardToPoolMap(const wchar_t *pool, const Card *card)
{
    PoolToCards *poolMap = FindPoolToCards(pool);
    if (!poolMap)
    {
        // 创建新的卡池映射
        if (g_GlobalData.poolToCardsCount >= g_GlobalData.poolToCardsCapacity)
        {
            int newCapacity = g_GlobalData.poolToCardsCapacity == 0 ? 10 : g_GlobalData.poolToCardsCapacity * 2;
            PoolToCards *newMap = realloc(g_GlobalData.poolToCardsMap, newCapacity * sizeof(PoolToCards));
            if (!newMap)
                return;
            g_GlobalData.poolToCardsMap = newMap;
            g_GlobalData.poolToCardsCapacity = newCapacity;
        }

        poolMap = &g_GlobalData.poolToCardsMap[g_GlobalData.poolToCardsCount];
        SafeWcsncpy(poolMap->pool, pool, 50);
        poolMap->cards = NULL;
        poolMap->cardCount = 0;
        poolMap->capacity = 0;
        g_GlobalData.poolToCardsCount++;
    }

    // 添加卡牌到卡池
    if (poolMap->cardCount >= poolMap->capacity)
    {
        int newCapacity = poolMap->capacity == 0 ? 10 : poolMap->capacity * 2;
        Card *newCards = realloc(poolMap->cards, newCapacity * sizeof(Card));
        if (!newCards)
            return;
        poolMap->cards = newCards;
        poolMap->capacity = newCapacity;
    }

    Card *target = &poolMap->cards[poolMap->cardCount];
    SafeWcsncpy(target->pool, card->pool, 50);
    SafeWcsncpy(target->name, card->name, 50);
    SafeWcsncpy(target->level, card->level, 10);
    SafeWcsncpy(target->count, card->count, 10);
    poolMap->cardCount++;
}

// 添加名称到卡池映射
void AddNameToPoolMap(const wchar_t *name, const wchar_t *pool)
{
    if (FindNameToPool(name))
        return; // 已存在

    if (g_GlobalData.nameToPoolCount >= g_GlobalData.nameToPoolCapacity)
    {
        int newCapacity = g_GlobalData.nameToPoolCapacity == 0 ? 100 : g_GlobalData.nameToPoolCapacity * 2;
        NameToPool *newMap = realloc(g_GlobalData.nameToPoolMap, newCapacity * sizeof(NameToPool));
        if (!newMap)
            return;
        g_GlobalData.nameToPoolMap = newMap;
        g_GlobalData.nameToPoolCapacity = newCapacity;
    }

    NameToPool *map = &g_GlobalData.nameToPoolMap[g_GlobalData.nameToPoolCount];
    SafeWcsncpy(map->name, name, 50);
    SafeWcsncpy(map->pool, pool, 50);
    g_GlobalData.nameToPoolCount++;
}

// 从嵌入数据加载
BOOL LoadEmbeddedData(GlobalData *data)
{
    if (!data)
        return FALSE;

    // 使用嵌入的 CSV 数据
    char *data_copy = _strdup(embedded_csv_data);
    if (!data_copy)
        return FALSE;

    data->allCardCount = 0;
    char *context = NULL;
    char *line = strtok_s(data_copy, "\n", &context);

    // 跳过标题行
    line = strtok_s(NULL, "\n", &context);

    while (line && data->allCardCount < MAX_CARDS)
    {
        // 移除可能的回车符
        line[strcspn(line, "\r")] = 0;

        // 跳过空行
        if (strlen(line) == 0)
        {
            line = strtok_s(NULL, "\n", &context);
            continue;
        }

        // 扩展所有卡牌数组
        if (data->allCardCount >= data->allCardCapacity)
        {
            int newCapacity = data->allCardCapacity == 0 ? 100 : data->allCardCapacity * 2;
            Card *newCards = realloc(data->allCards, newCapacity * sizeof(Card));
            if (!newCards)
                break;
            data->allCards = newCards;
            data->allCardCapacity = newCapacity;
        }

        // 初始化所有字段为空
        wchar_t *fields[4] = {NULL, NULL, NULL, NULL};

        // 解析CSV行
        char *field_context = NULL;
        char *token = strtok_s(line, ",", &field_context);
        int field = 0;

        while (token && field < 4)
        {
            fields[field] = UTF8ToWide(token);
            token = strtok_s(NULL, ",", &field_context);
            field++;
        }

        // 存储到所有卡牌数组
        Card *card = &data->allCards[data->allCardCount];
        SafeWcsncpy(card->pool, fields[0], 50);
        SafeWcsncpy(card->name, fields[1], 50);
        SafeWcsncpy(card->level, fields[2], 10);
        SafeWcsncpy(card->count, fields[3], 10);

        // 创建名称到卡池映射（如果名称不为空）
        if (fields[1] && wcslen(fields[1]) > 0)
        {
            AddNameToPoolMap(fields[1], fields[0]);
        }

        // 创建卡池到卡牌列表映射（如果卡池不为空）
        if (fields[0] && wcslen(fields[0]) > 0)
        {
            AddCardToPoolMap(fields[0], card);
        }

        data->allCardCount++;

        // 清理临时字段
        for (int i = 0; i < 4; i++)
        {
            if (fields[i])
                free(fields[i]);
        }

        line = strtok_s(NULL, "\n", &context);
    }

    free(data_copy);
    return TRUE;
}

// 释放全局数据
void FreeGlobalData(GlobalData *data)
{
    if (!data)
        return;

    // 释放所有卡牌数组
    if (data->allCards)
    {
        free(data->allCards);
        data->allCards = NULL;
    }

    // 释放名称到卡池映射
    if (data->nameToPoolMap)
    {
        free(data->nameToPoolMap);
        data->nameToPoolMap = NULL;
    }

    // 释放卡池到卡牌映射（需要先释放每个卡池的卡牌数组）
    if (data->poolToCardsMap)
    {
        for (int i = 0; i < data->poolToCardsCount; i++)
        {
            if (data->poolToCardsMap[i].cards)
            {
                free(data->poolToCardsMap[i].cards);
            }
        }
        free(data->poolToCardsMap);
        data->poolToCardsMap = NULL;
    }

    // 重置计数
    data->allCardCount = 0;
    data->allCardCapacity = 0;
    data->nameToPoolCount = 0;
    data->nameToPoolCapacity = 0;
    data->poolToCardsCount = 0;
    data->poolToCardsCapacity = 0;

    // 释放 JSON 操作记录
    FreeJsonOperations(data);

    // 释放当前对局卡牌数据
    if (data->currentGame.cards)
    {
        free(data->currentGame.cards);
        data->currentGame.cards = NULL;
    }

    // 释放卡池缓存
    if (data->poolCache)
    {
        free(data->poolCache);
        data->poolCache = NULL;
    }
}