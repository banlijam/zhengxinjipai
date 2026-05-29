#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "data_struct.h"

// 全局变量定义
GlobalData g_GlobalData = {0};
HWND g_hMainWindow = NULL;

// 定时器ID
#define TIMER_ID_JSON 1001

// 颜色定义
#define COLOR_BG RGB(15, 20, 30)        // 深蓝黑色背景
#define COLOR_HEADER_BG RGB(25, 35, 55) // 标题栏背景
#define COLOR_TEXT RGB(220, 220, 220)   // 主文字颜色
#define COLOR_ACCENT RGB(0, 150, 255)   // 强调色（蓝色）
#define COLOR_BORDER RGB(50, 65, 90)    // 边框颜色

// 根据等级获取颜色
COLORREF GetColorByLevel(const wchar_t *level)
{
    if (!level || wcslen(level) == 0)
        return COLOR_TEXT; // 默认文字颜色

    int levelNum = _wtoi(level);
    switch (levelNum)
    {
    case 1:
        return RGB(240, 240, 240); // 白色（更亮）
    case 2:
        return RGB(80, 230, 80); // 绿色（更亮）
    case 3:
        return RGB(80, 180, 255); // 蓝色（更亮）
    case 4:
        return RGB(200, 100, 255); // 紫色（更亮）
    case 5:
        return RGB(255, 230, 80); // 金色
    default:
        return COLOR_TEXT;
    }
}

// 计算需要的列数和窗口尺寸
void CalculateWindowSize(int cardCount, int *columns, int *width, int *height)
{
    // 每列最多显示12条卡牌
    *columns = (cardCount + 11) / 12; // 向上取整
    if (*columns < 1)
        *columns = 1;

    // 每列宽度为120像素（进一步减小）
    *width = (*columns * 90) + 30; // 30像素为左右边距
    if (*width < 280)
        *width = 280; // 最小宽度

    // 计算高度：标题栏(50) + 卡牌区域(12*24) + 状态栏(35) + 边距(10)
    // 固定12行高度，确保所有卡牌都能显示
    *height = 50 + (12 * 24) + 35 + 40;
    if (*height < 180)
        *height = 180; // 最小高度
}

BOOL IsCardEmpty(const Card *card)
{
    if (!card)
        return TRUE;
    if (wcslen(card->name) == 0)
        return TRUE;
    return FALSE;
}

// 绘制圆角矩形
void DrawRoundRect(HDC hdc, int x, int y, int width, int height, int radius, COLORREF color)
{
    HBRUSH hBrush = CreateSolidBrush(color);
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, x, y, x + width, y + height, radius, radius);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

// 计算所有卡牌的总数量（累加所有卡牌的count字段）
int CalculateTotalCardCount(const GlobalData *data)
{
    if (!data || !data->currentGame.cards)
        return 0;

    int total = 0;
    for (int i = 0; i < data->currentGame.cardCount; i++)
    {
        if (!IsCardEmpty(&data->currentGame.cards[i]))
        {
            int count = _wtoi(data->currentGame.cards[i].count);
            if (count > 0) // 只累加正数数量
            {
                total += count;
            }
        }
    }
    return total;
}

// 主窗口过程
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_CREATE:
    {
        g_hMainWindow = hwnd;

        // 初始化 JSON 日志监控
        InitializeJsonLogging(&g_GlobalData);

        // 启动定时器，每秒检查一次 JSON 日志更新
        SetTimer(hwnd, TIMER_ID_JSON, 1000, NULL);

        return 0;
    }

    case WM_ERASEBKGND:
    {
        // 处理背景擦除，填充自定义背景色
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hwnd, &rect);
        HBRUSH hBrush = CreateSolidBrush(COLOR_BG);
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);
        return 1;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        // 设置背景
        HBRUSH hBackgroundBrush = CreateSolidBrush(COLOR_BG);
        FillRect(hdc, &clientRect, hBackgroundBrush);
        DeleteObject(hBackgroundBrush);

        // 绘制标题栏背景
        RECT headerRect = {0, 0, clientRect.right, 50};
        HBRUSH hHeaderBrush = CreateSolidBrush(COLOR_HEADER_BG);
        FillRect(hdc, &headerRect, hHeaderBrush);
        DeleteObject(hHeaderBrush);

        // 绘制标题栏底部边框
        HPEN hBorderPen = CreatePen(PS_SOLID, 2, COLOR_BORDER);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hBorderPen);
        MoveToEx(hdc, 0, 50, NULL);
        LineTo(hdc, clientRect.right, 50);
        SelectObject(hdc, hOldPen);
        DeleteObject(hBorderPen);

        // 设置标题字体（调大一号）
        HFONT hTitleFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

        // 设置正文字体（调大一号）
        HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

        HFONT hOldFont = (HFONT)SelectObject(hdc, hTitleFont);

        // 设置文本背景透明
        SetBkMode(hdc, TRANSPARENT);

        // 绘制主标题
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT titleRect = {15, 10, clientRect.right - 15, 40};
        DrawTextW(hdc, L"正新记牌", -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 绘制对局ID
        wchar_t gameIdText[50];
        swprintf(gameIdText, 50, L"对局 ID: %d", g_GlobalData.currentGameId);
        RECT gameIdRect = {clientRect.right - 180, 10, clientRect.right - 15, 40};
        DrawTextW(hdc, gameIdText, -1, &gameIdRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        // 切换到正文字体
        SelectObject(hdc, hFont);

        // 计算列数和位置
        int cardCount = g_GlobalData.currentGame.cardCount;
        int columns = (cardCount + 11) / 12; // 每列12条，向上取整
        if (columns < 1)
            columns = 1;

        int colWidth = 90;  // 每列宽度（减小）
        int rowHeight = 24; // 每行高度（增加以适应更大字体）
        int startX = 15;    // 起始X坐标
        int startY = 60;    // 起始Y坐标（在标题栏下方）

        // 绘制数据 - 按列排列，确保所有列都能显示
        int nonEmptyCount = 0;
        int totalCards = 0;

        for (int i = 0; i < cardCount; i++)
        {
            int col = i / 12; // 列索引（每列12行）
            int row = i % 12; // 行索引

            int x = startX + (col * colWidth);
            int y = startY + (row * rowHeight);

            // 检查卡牌是否为空
            if (IsCardEmpty(&g_GlobalData.currentGame.cards[i]))
            {
                // 空白卡牌 - 显示空行（不绘制任何内容，但保留位置）
                continue;
            }

            nonEmptyCount++;

            // 根据等级设置文本颜色
            COLORREF textColor = GetColorByLevel(g_GlobalData.currentGame.cards[i].level);
            SetTextColor(hdc, textColor);

            // 绘制卡牌名称（左对齐）- 减小名称和数字之间的宽度
            RECT nameRect = {x, y, x + colWidth - 25, y + rowHeight}; // 从-35改为-25
            DrawTextW(hdc, g_GlobalData.currentGame.cards[i].name, -1, &nameRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // 绘制数量（右对齐，带背景）
            int count = _wtoi(g_GlobalData.currentGame.cards[i].count);
            totalCards += count > 0 ? count : 0;
            wchar_t countText[20];
            swprintf(countText, 20, L"%d", count);

            // 为数量添加圆角背景 - 减小宽度
            if (count > 0)
            {
                COLORREF countBgColor = RGB(40, 50, 70);                               // 数量背景色
                DrawRoundRect(hdc, x + colWidth - 25, y + 6, 22, 16, 6, countBgColor); // 从-32改为-25，宽度从28改为22
            }

            RECT countRect = {x + colWidth - 25, y, x + colWidth - 3, y + rowHeight}; // 从-32改为-25
            SetTextColor(hdc, RGB(255, 255, 255));                                    // 数量文字用白色
            DrawTextW(hdc, countText, -1, &countRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // 绘制底部状态栏背景
        RECT statusRect = {0, clientRect.bottom - 35, clientRect.right, clientRect.bottom};
        HBRUSH hStatusBrush = CreateSolidBrush(COLOR_HEADER_BG);
        FillRect(hdc, &statusRect, hStatusBrush);
        DeleteObject(hStatusBrush);

        // 绘制状态栏顶部边框
        hBorderPen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
        hOldPen = (HPEN)SelectObject(hdc, hBorderPen);
        MoveToEx(hdc, 0, clientRect.bottom - 35, NULL);
        LineTo(hdc, clientRect.right, clientRect.bottom - 35);
        SelectObject(hdc, hOldPen);
        DeleteObject(hBorderPen);

        // 显示卡牌总数（改为累加所有卡牌数量）
        SetTextColor(hdc, COLOR_TEXT);
        wchar_t countText[100];
        swprintf(countText, 100, L"总计: %d 张", totalCards);
        RECT countRect = {15, clientRect.bottom - 35, clientRect.right - 15, clientRect.bottom - 5};
        DrawTextW(hdc, countText, -1, &countRect, DT_LEFT | DT_VCENTER);

        // 显示列数信息
        wchar_t colText[50];
        swprintf(colText, 50, L"列数: %d", columns);
        RECT colRect = {clientRect.right - 80, clientRect.bottom - 35, clientRect.right - 15, clientRect.bottom - 5};
        DrawTextW(hdc, colText, -1, &colRect, DT_RIGHT | DT_VCENTER);

        // 恢复旧字体并清理
        SelectObject(hdc, hOldFont);
        DeleteObject(hTitleFont);
        DeleteObject(hFont);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_TIMER:
        if (wParam == TIMER_ID_JSON)
        {
            // 检查日志更新，只有当数据真正变化时才更新界面
            BOOL dataChanged = CheckJsonLogUpdates(&g_GlobalData);

            // 检查对局ID是否变化
            static int lastGameId = 0;
            BOOL gameIdChanged = (g_GlobalData.currentGameId != lastGameId);

            // 检查卡池或卡牌数量是否变化
            BOOL shouldResize = g_GlobalData.poolChanged ||
                                (g_GlobalData.currentGame.cardCount != g_GlobalData.lastCardCount);

            // 只有在数据变化或需要调整窗口大小时才重绘
            if (dataChanged || shouldResize || gameIdChanged)
            {
                // 更新窗口标题显示对局ID和应用名称
                wchar_t title[256];
                swprintf(title, 256, L"正新记牌 - 对局ID: %d", g_GlobalData.currentGameId);
                SetWindowTextW(hwnd, title);

                if (shouldResize)
                {
                    // 根据卡牌数量调整窗口大小
                    int columns, newWidth, newHeight;
                    CalculateWindowSize(g_GlobalData.currentGame.cardCount, &columns, &newWidth, &newHeight);

                    // 获取当前窗口位置
                    RECT windowRect;
                    GetWindowRect(hwnd, &windowRect);
                    int currentX = windowRect.left;
                    int currentY = windowRect.top;

                    // 调整窗口大小（保持位置不变）
                    SetWindowPos(hwnd, NULL, currentX, currentY, newWidth, newHeight, SWP_NOZORDER);

                    // 重置变动标记和记录当前卡牌数量
                    g_GlobalData.poolChanged = FALSE;
                    g_GlobalData.lastCardCount = g_GlobalData.currentGame.cardCount;
                }

                // 只在数据真正变化时才重绘窗口
                InvalidateRect(hwnd, NULL, FALSE);
            }

            // 更新最后对局ID
            lastGameId = g_GlobalData.currentGameId;
        }
        return 0;

    case WM_CLOSE:
        // 停止定时器
        KillTimer(hwnd, TIMER_ID_JSON);
        FreeGlobalData(&g_GlobalData);
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 初始化全局数据
    memset(&g_GlobalData, 0, sizeof(GlobalData));
    g_GlobalData.poolChanged = FALSE;
    g_GlobalData.lastCardCount = 0;
    g_GlobalData.lastOperationCount = 0;

    // 在启动时加载数据
    if (!LoadEmbeddedData(&g_GlobalData))
    {
        MessageBoxW(NULL, L"加载数据失败", L"错误", MB_ICONERROR);
        return 1;
    }

    // 显示加载的卡牌数量（调试用）
    wchar_t debugMsg[100];
    swprintf(debugMsg, 100, L"成功加载 %d 张卡牌", g_GlobalData.allCardCount);
    OutputDebugStringW(debugMsg);

    // 注册主窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MainApp";
    wc.hbrBackground = (HBRUSH)CreateSolidBrush(COLOR_BG); // 设置自定义背景色
    wc.lpszMenuName = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); // 使用默认图标

    if (!RegisterClassW(&wc))
    {
        FreeGlobalData(&g_GlobalData);
        return 1;
    }

    // 计算初始窗口大小
    int initialColumns, initialWidth, initialHeight;
    CalculateWindowSize(0, &initialColumns, &initialWidth, &initialHeight);

    // 创建主窗口
    HWND hwnd = CreateWindowExW(
        0,
        L"MainApp",
        L"正新记牌 - 对局ID: 0",                                // 初始标题
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, // 移除调整大小和最大化按钮
        CW_USEDEFAULT, CW_USEDEFAULT, initialWidth, initialHeight,
        NULL, NULL, hInstance, NULL);

    if (!hwnd)
    {
        FreeGlobalData(&g_GlobalData);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}