#undef UNICODE
#undef _UNICODE


#include <graphics.h>
#include <time.h>
#include <conio.h>
#include <thread>
#include <atomic>
#include <windows.h>
#include <vector>
#include <cassert>
#include <fstream>  // 文件I/O
#include <string>
#include <algorithm> // 用于排序
#include <ctime>     // 用于计时
#include <map>       // 用于映射敌机类型到图像
#include <memory>    // 用于智能指针

using namespace std;
constexpr auto width = 600;
constexpr auto height = 700;
#define MAXSTAR 200	// 星星总数

// 定义难度结构体
struct Difficulty {
    std::string name;      // 难度名称
    int enemyCount;        // 敌机数量
    int enemySpeed;        // 敌机速度
    int bulletInterval;    // 子弹发射间隔
    int enemyDamage;       // 敌机伤害值
    float propDropRate;    // 道具掉落率（0-1）
    int bossAppearScore;   // BOSS出现的分数阈值
};

// 定义分数记录结构体
struct ScoreRecord {
    std::string playerName;
    unsigned long long score;
    std::string difficulty;
    time_t timestamp;
};

// 定义敌机类型枚举
enum EnemyType {
    SMALL = 0,    // 小型敌机
    MEDIUM = 1,   // 中型敌机
    LARGE = 2,    // 大型敌机
    BOSS = 3      // BOSS敌机
};

// 定义道具类型枚举
enum PropType {
    BOMB = 0,     // 炸弹（清屏）
    LIFE = 1      // 生命值
};

// 定义爆炸效果结构体
struct Explosion {
    vector<IMAGE> frames;   // 爆炸动画帧
    int x, y;               // 爆炸位置
    int currentFrame;       // 当前帧
    int frameDelay;         // 帧延迟
    int frameCounter;       // 帧计数器
    
    Explosion(int x, int y, vector<IMAGE>& frames) 
        : x(x), y(y), frames(frames), currentFrame(0), frameDelay(3), frameCounter(0) {}
    
    bool Update() {
        frameCounter++;
        if (frameCounter >= frameDelay) {
            frameCounter = 0;
            currentFrame++;
        }
        return currentFrame < frames.size();
    }
    
    void Draw() const {  // 将Draw方法标记为const
        if (currentFrame < frames.size()) {
            putimage(x, y, &frames[currentFrame]);
        }
    }
};

// 全局变量
std::vector<ScoreRecord> scoreRecords;  // 分数记录
Difficulty currentDifficulty;           // 当前难度
bool isPaused = false;                  // 暂停状态
clock_t gameStartTime;                  // 游戏开始时间
int gameTime = 0;                       // 游戏时长(秒)
map<EnemyType, vector<IMAGE>> enemyImages;  // 敌机图像映射
map<EnemyType, vector<IMAGE>> explosionImages;  // 爆炸图像映射
map<PropType, IMAGE> propImages;        // 道具图像映射
vector<IMAGE> bulletImages;             // 子弹图像
vector<IMAGE> heroImages;               // 英雄图像
vector<IMAGE> heroExplosionImages;      // 英雄爆炸图像
IMAGE backgroundImage;                  // 背景图像
bool showedBossWarning = false;         // 是否已显示Boss警告

// 添加暂停菜单图像
IMAGE pauseMenuImage;                   // 暂停菜单背景
IMAGE resumeButtonNor;                  // 继续游戏按钮(正常状态)
IMAGE resumeButtonSel;                  // 继续游戏按钮(选中状态)
IMAGE restartButtonNor;                  // 重新开始按钮(正常状态)
IMAGE restartButtonSel;                  // 重新开始按钮(选中状态)
IMAGE quitButtonNor;                     // 退出按钮(正常状态)
IMAGE quitButtonSel;                     // 退出按钮(选中状态)

// 预定义难度级别
Difficulty EASY = { "简单", 3, 2, 30, 1, 0.05f, 500 };
Difficulty NORMAL = { "中等", 4, 3, 25, 1, 0.03f, 300 };
Difficulty HARD = { "困难", 5, 4, 20, 2, 0.02f, 200 };

struct STAR
{
    double	x;
    int		y;
    double	step;
    int		color;
};

STAR star[MAXSTAR];

RECT tplayr, texitr; // 矩形结构体
LPCTSTR title = _T("飞机大战");
LPCTSTR tplay = _T("开始游戏");
LPCTSTR texit = _T("退出游戏");

// 判断鼠标位置
bool PointInRect(int x, int y, RECT& r)
{
    return (r.left <= x && x <= r.right && r.top <= y && y <= r.bottom);
}

bool RectDuangRect(RECT& r1, RECT& r2)
{
    RECT r;
    r.left = r1.left - (r2.right - r2.left);
    r.right = r1.right;
    r.top = r1.top - (r2.bottom - r2.top);
    r.bottom = r1.bottom;

    return (r.left < r2.left && r2.left <= r.right && r.top <= r2.top && r2.top <= r.bottom);
}

// 初始化星星
void InitStar(int i)
{
    star[i].x = 0;
    star[i].y = rand() % height;
    star[i].step = (rand() % 5000) / 1000.0 + 1;
    star[i].color = (int)(star[i].step * 255 / 6.0 + 0.5);	// 速度越快，颜色越亮
    star[i].color = RGB(star[i].color, star[i].color, star[i].color);
}

// 移动星星
void MoveStar(int i)
{
    // 擦掉原来的星星
    putpixel((int)star[i].x, star[i].y, 0);

    // 计算新位置
    star[i].x += star[i].step;
    if (star[i].x > width)	InitStar(i);

    // 画新星星
    putpixel((int)star[i].x, star[i].y, star[i].color);
}

// 星星移动线程函数
void moveStars(std::atomic<bool>& running) {
    BeginBatchDraw();
    while (running) {
        for (int i = 0; i < MAXSTAR; i++) {
            MoveStar(i);
        }
        FlushBatchDraw();
        Sleep(20);
    }
    EndBatchDraw();
}

// 读取分数记录
void LoadScoreRecords() {
    std::ifstream file("scores.txt");
    if (!file) return; // 文件不存在则返回

    ScoreRecord record;
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = 0;
        pos = line.find(',');
        record.playerName = line.substr(0, pos);
        line.erase(0, pos + 1);

        pos = line.find(',');
        record.score = std::stoull(line.substr(0, pos));
        line.erase(0, pos + 1);

        pos = line.find(',');
        record.difficulty = line.substr(0, pos);
        line.erase(0, pos + 1);

        record.timestamp = std::stoll(line);

        scoreRecords.push_back(record);
    }
    file.close();
}

// 保存分数记录
void SaveScoreRecords() {
    std::ofstream file("scores.txt");
    if (!file) return;

    for (const auto& record : scoreRecords) {
        file << record.playerName << ","
            << record.score << ","
            << record.difficulty << ","
            << record.timestamp << std::endl;
    }
    file.close();
}

// 添加新分数记录
void AddScoreRecord(const std::string& playerName, unsigned long long score) {
    ScoreRecord record;
    record.playerName = playerName;
    record.score = score;
    record.difficulty = currentDifficulty.name;
    record.timestamp = time(NULL);

    scoreRecords.push_back(record);

    // 排序分数记录（按分数降序）
    std::sort(scoreRecords.begin(), scoreRecords.end(),
        [](const ScoreRecord& a, const ScoreRecord& b) {
            return a.score > b.score;
        });

    // 最多保存10条记录
    if (scoreRecords.size() > 10) {
        scoreRecords.resize(10);
    }

    SaveScoreRecords();
}

// 显示排行榜
void ShowLeaderboard() {
    cleardevice();

    LPCTSTR title = _T("排行榜");
    settextstyle(40, 0, _T("黑体"));
    outtextxy(width / 2 - textwidth(title) / 2, 50, title);

    settextstyle(20, 0, _T("黑体"));
    int y = 120;
    TCHAR str[128];

    for (size_t i = 0; i < scoreRecords.size(); i++) {
        // 转换时间戳为本地时间
        char timeStr[30];
        time_t t = scoreRecords[i].timestamp;
        struct tm tm;
        localtime_s(&tm, &t);
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", &tm);

        // 准备显示字符串
        _stprintf_s(str, 128, _T("%d. %s - %llu分 - %s - %s"),
            static_cast<int>(i + 1),
            scoreRecords[i].playerName.c_str(),
            scoreRecords[i].score,
            scoreRecords[i].difficulty.c_str(),
            timeStr);

        outtextxy(width / 2 - textwidth(str) / 2, y, str);
        y += 30;
    }

    LPCTSTR info = _T("按Enter返回");
    outtextxy(width - textwidth(info), height - textheight(info), info);

    while (true) {
        ExMessage mess;
        getmessage(&mess, EM_KEY);
        if (mess.vkcode == 0x0D) {  // Enter键
            return;
        }
    }
}

// 选择难度界面
void SelectDifficulty() {
    cleardevice();

    LPCTSTR title = _T("选择难度");
    settextstyle(40, 0, _T("黑体"));
    outtextxy(width / 2 - textwidth(title) / 2, 100, title);

    LPCTSTR easyText = _T("简单");
    LPCTSTR normalText = _T("中等");
    LPCTSTR hardText = _T("困难");

    settextstyle(30, 0, _T("黑体"));

    RECT easyRect, normalRect, hardRect;

    easyRect.left = width / 2 - textwidth(easyText) / 2;
    easyRect.right = width / 2 + textwidth(easyText) / 2;
    easyRect.top = height / 5 * 2;
    easyRect.bottom = easyRect.top + textheight(easyText);

    normalRect.left = width / 2 - textwidth(normalText) / 2;
    normalRect.right = width / 2 + textwidth(normalText) / 2;
    normalRect.top = height / 5 * 2.5;
    normalRect.bottom = normalRect.top + textheight(normalText);

    hardRect.left = width / 2 - textwidth(hardText) / 2;
    hardRect.right = width / 2 + textwidth(hardText) / 2;
    hardRect.top = height / 5 * 3;
    hardRect.bottom = hardRect.top + textheight(hardText);

    outtextxy(easyRect.left, easyRect.top, easyText);
    outtextxy(normalRect.left, normalRect.top, normalText);
    outtextxy(hardRect.left, hardRect.top, hardText);

    EndBatchDraw();

    while (true) {
        ExMessage mess;
        getmessage(&mess, EM_MOUSE);
        if (mess.lbutton) {
            if (PointInRect(mess.x, mess.y, easyRect)) {
                currentDifficulty = EASY;
                return;
            }
            else if (PointInRect(mess.x, mess.y, normalRect)) {
                currentDifficulty = NORMAL;
                return;
            }
            else if (PointInRect(mess.x, mess.y, hardRect)) {
                currentDifficulty = HARD;
                return;
            }
        }
    }
}

// 显示游戏帮助界面
void ShowHelp() {
    cleardevice();

    LPCTSTR title = _T("游戏帮助");
    settextstyle(40, 0, _T("黑体"));
    outtextxy(width / 2 - textwidth(title) / 2, 50, title);

    settextstyle(20, 0, _T("黑体"));
    int y = 120;

    const TCHAR* helpText[] = {
        _T("1. 使用W/A/S/D键控制飞机移动"),
        _T("2. 按空格键暂停/继续游戏"),
        _T("3. 按B键使用炸弹，清除所有敌机"),
        _T("4. 击败敌机可能掉落道具："),
        _T("   - 炸弹：使用B键触发，消灭全屏敌机"),
        _T("   - 生命值：自动增加一条生命"),
        _T("5. 不同敌机有不同血量和分数:"),
        _T("   - 小型敌机：1点血，100分"),
        _T("   - 中型敌机：2点血，300分"),
        _T("   - 大型敌机：5点血，500分"),
        _T("   - BOSS：20点血，1000分"),
        _T("6. 收集分数达到一定值后会出现BOSS"),
        _T("祝您游戏愉快！")
    };

    for (const auto& line : helpText) {
        outtextxy(width / 2 - textwidth(line) / 2, y, line);
        y += 30;
    }

    LPCTSTR info = _T("按Enter返回");
    outtextxy(width - textwidth(info), height - textheight(info), info);

    while (true) {
        ExMessage mess;
        getmessage(&mess, EM_KEY);
        if (mess.vkcode == 0x0D) {  // Enter键
            return;
        }
    }
}

// 更新游戏结束界面
void Over(unsigned long long& score) {
    cleardevice();

    TCHAR str[128];
    _stprintf_s(str, 128, _T("得分：%llu"), score);

    settextcolor(RED);
    settextstyle(40, 0, _T("黑体"));
    outtextxy(width / 2 - textwidth(str) / 2, height / 5, str);

    // 显示游戏时间
    TCHAR timeStr[50];
    _stprintf_s(timeStr, 50, _T("游戏时间：%d秒"), gameTime);
    outtextxy(width / 2 - textwidth(timeStr) / 2, height / 5 + 50, timeStr);

    // 输入姓名保存分数
    TCHAR namePrompt[50] = _T("输入您的名字：");
    outtextxy(width / 2 - textwidth(namePrompt) / 2, height / 2, namePrompt);

    char playerName[50] = "";
    int nameLength = 0;

    // 创建输入框
    RECT inputRect;
    inputRect.left = width / 2 - 100;
    inputRect.right = width / 2 + 100;
    inputRect.top = height / 2 + 40;
    inputRect.bottom = inputRect.top + 30;

    rectangle(inputRect.left, inputRect.top, inputRect.right, inputRect.bottom);

    settextcolor(BLACK);
    settextstyle(20, 0, _T("黑体"));

    // 按Enter保存
    LPCTSTR savePrompt = _T("按Enter保存");
    outtextxy(width / 2 - textwidth(savePrompt) / 2, inputRect.bottom + 20, savePrompt);

    // 清空输入缓冲区
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
    while (_kbhit()) {
        _getch();
    }

    bool inputting = true;
    while (inputting) {
        // 显示背景(白色)，避免黑色矩形问题
        setfillcolor(WHITE);
        fillrectangle(inputRect.left + 1, inputRect.top + 1, inputRect.right - 1, inputRect.bottom - 1);
        
        // 显示当前输入
        settextcolor(BLACK);
        outtextxy(inputRect.left + 10, inputRect.top + 5, playerName);

        // 强制刷新画面
        FlushBatchDraw();

        // 使用Windows API获取键盘输入
        if (GetAsyncKeyState(VK_RETURN) & 0x0001) {  // Enter键
                inputting = false;
            }
        else if (GetAsyncKeyState(VK_BACK) & 0x0001 && nameLength > 0) {  // Backspace键
                playerName[--nameLength] = '\0';
            }
        else {
            // 检查所有可打印字符
            for (int key = 32; key <= 126; key++) {
                if ((GetAsyncKeyState(key) & 0x0001) && nameLength < 49) {
                    playerName[nameLength++] = static_cast<char>(key);
                playerName[nameLength] = '\0';
                    break;
                }
            }
        }

        Sleep(50);  // 给系统更多时间处理输入
    }

    // 如果没有输入姓名，使用默认名称
    if (nameLength == 0) {
        strcpy_s(playerName, "Player");
    }

    // 保存分数
    AddScoreRecord(playerName, score);

    // 显示排行榜
    ShowLeaderboard();
}

// 更新开始界面，添加排行榜按钮
void start() {
    cleardevice();

    // 显示背景
    putimage(0, 0, &backgroundImage);

    settextstyle(60, 0, title);
    outtextxy(width / 2 - textwidth(title) / 2, 100, title);

    settextstyle(40, 0, tplay);
    tplayr.left = width / 2 - textwidth(tplay) / 2;
    tplayr.right = width / 2 + textwidth(tplay) / 2;
    tplayr.top = height / 5 * 2;
    tplayr.bottom = tplayr.top + textheight(tplay);

    LPCTSTR tleaderboard = _T("排行榜");
    RECT tleaderboardr;
    tleaderboardr.left = width / 2 - textwidth(tleaderboard) / 2;
    tleaderboardr.right = width / 2 + textwidth(tleaderboard) / 2;
    tleaderboardr.top = height / 5 * 2.5;
    tleaderboardr.bottom = tleaderboardr.top + textheight(tleaderboard);

    LPCTSTR thelp = _T("帮助");
    RECT thelpr;
    thelpr.left = width / 2 - textwidth(thelp) / 2;
    thelpr.right = width / 2 + textwidth(thelp) / 2;
    thelpr.top = height / 5 * 2.8;
    thelpr.bottom = thelpr.top + textheight(thelp);

    settextstyle(40, 0, texit);
    texitr.left = width / 2 - textwidth(texit) / 2;
    texitr.right = width / 2 + textwidth(texit) / 2;
    texitr.top = height / 5 * 3.1;
    texitr.bottom = texitr.top + textheight(texit);

    outtextxy(tplayr.left, tplayr.top, tplay);
    outtextxy(tleaderboardr.left, tleaderboardr.top, tleaderboard);
    outtextxy(thelpr.left, thelpr.top, thelp);
    outtextxy(texitr.left, texitr.top, texit);

    EndBatchDraw();

    while (true) {
        ExMessage mess;
        getmessage(&mess, EM_MOUSE);
        if (mess.lbutton) {
            if (PointInRect(mess.x, mess.y, tplayr)) {
                SelectDifficulty();  // 选择难度
                return;
            }
            else if (PointInRect(mess.x, mess.y, tleaderboardr)) {
                ShowLeaderboard();  // 显示排行榜
                start();  // 显示完排行榜后重新显示开始界面
                return;
            }
            else if (PointInRect(mess.x, mess.y, thelpr)) {
                ShowHelp();  // 显示帮助
                start();  // 显示完帮助后重新显示开始界面
                return;
            }
            else if (PointInRect(mess.x, mess.y, texitr)) {
                exit(0);
            }
        }
    }
}

class Hero {
public:
    Hero(vector<IMAGE>& imgs)
        :imgs(imgs), lives(3), isInvincible(false), invincibleTime(0), 
        bombCount(0), currentFrame(0), frameDelay(5), frameCounter(0), isDying(false)
    {
        rect.left = width / 2 - imgs[0].getwidth() / 2;
        rect.top = height - imgs[0].getheight() / 2;
        rect.right = rect.left + imgs[0].getwidth();
        rect.bottom = height;
    }

    void Show()
    {
        if (isDying) return; // 如果正在死亡动画中，不显示飞机

        // 更新动画帧
        frameCounter++;
        if (frameCounter >= frameDelay) {
            frameCounter = 0;
            currentFrame = (currentFrame + 1) % imgs.size();
        }

        // 如果处于无敌状态，实现闪烁效果
        if (isInvincible) {
            invincibleTime--;
            if (invincibleTime <= 0) {
                isInvincible = false;
            }
            // 每隔几帧闪烁一次
            if ((invincibleTime / 5) % 2 == 0) {
                putimage(rect.left, rect.top, &imgs[currentFrame]);
            }
        } else {
            putimage(rect.left, rect.top, &imgs[currentFrame]);
        }

        // 显示生命值
        TCHAR lifeText[20];
        _stprintf_s(lifeText, 20, _T("生命: %d"), lives);
        settextcolor(RED);
        settextstyle(20, 0, _T("黑体"));
        outtextxy(10, height - 30, lifeText);

        // 显示炸弹数量
        TCHAR bombText[20];
        _stprintf_s(bombText, 20, _T("炸弹: %d"), bombCount);
        outtextxy(100, height - 30, bombText);
    }

    void Control()  // 控制飞机移动
    {
        if (isDying) return; // 死亡动画中不可控制

        int moveSpeed = 5;
        if (GetAsyncKeyState('W') & 0x8000) {
            if (rect.top - moveSpeed >= 0) {
                rect.top -= moveSpeed;
                rect.bottom -= moveSpeed;
            }
        }
        if (GetAsyncKeyState('S') & 0x8000) {
            if (rect.bottom + moveSpeed <= height) {
                rect.top += moveSpeed;
                rect.bottom += moveSpeed;
            }
        }
        if (GetAsyncKeyState('A') & 0x8000) {
            if (rect.left - moveSpeed >= 0) {
                rect.left -= moveSpeed;
                rect.right -= moveSpeed;
            }
        }
        if (GetAsyncKeyState('D') & 0x8000) {
            if (rect.right + moveSpeed <= width) {
                rect.left += moveSpeed;
                rect.right += moveSpeed;
            }
        }
        // 使用炸弹（按B键）
        if (GetAsyncKeyState('B') & 0x8001 && bombCount > 0) {
            bombCount--;
            return;
        }
    }

    RECT& GetRect() { return rect; }

    // 减少生命值
    bool TakeDamage(int damage) {
        if (isInvincible || isDying) return false; // 无敌状态或死亡动画中不受伤害

        lives -= damage;
        if (lives <= 0) {
            isDying = true;
            return true;  // 生命值为0，游戏结束
        }
        
        // 受伤后短暂无敌
        isInvincible = true;
        invincibleTime = 120; // 约2秒无敌
        return false;
    }

    // 获取当前生命值
    int GetLives() const { return lives; }

    // 增加生命值
    void AddLife() {
        lives++;
    }

    // 增加炸弹
    void AddBomb() {
        bombCount++;
    }

    // 使用炸弹并返回是否成功
    bool UseBomb() {
        if (bombCount > 0) {
            bombCount--;
            return true;
        }
        return false;
    }

    // 检查是否有炸弹
    bool HasBomb() const {
        return bombCount > 0;
    }

    // 检查是否处于无敌状态
    bool IsInvincible() const {
        return isInvincible;
    }

    // 检查是否正在播放死亡动画
    bool IsDying() const {
        return isDying;
    }

    // 设置死亡动画状态
    void SetDying(bool dying) {
        isDying = dying;
    }

private:
    vector<IMAGE>& imgs;
    RECT rect;
    int lives;            // 生命值
    bool isInvincible;    // 是否无敌
    int invincibleTime;   // 无敌时间计数
    int bombCount;        // 炸弹数量
    int currentFrame;     // 当前动画帧
    int frameDelay;       // 帧延迟
    int frameCounter;     // 帧计数器
    bool isDying;         // 是否正在死亡动画
};

// 添加敌人子弹类
class EnemyBullet {
public:
    EnemyBullet(IMAGE& img, int x, int y, int damage = 1)
        : img(img), damage(damage)
    {
        rect.left = x - img.getwidth() / 2;
        rect.right = rect.left + img.getwidth();
        rect.top = y;
        rect.bottom = rect.top + img.getheight();
    }

    bool Show()
    {
        if (rect.top >= height)
        {
            return false;
        }
        rect.top += 2;  // 敌人子弹速度
        rect.bottom += 2;
        putimage(rect.left, rect.top, &img);

        return true;
    }

    RECT& GetRect() { return rect; }
    int GetDamage() const { return damage; }

private:
    IMAGE& img;
    RECT rect;
    int damage;
};

// 在Enemy类中添加射击方法
class Enemy {
public:
    Enemy(EnemyType t, int x, int y)
    {
        type = t;
        rect.left = x;
        rect.top = y;
        rect.right = x + enemyImages[type][0].getwidth();
        rect.bottom = y + enemyImages[type][0].getheight();
        
        isDying = false;
        currentFrame = 0;
        frameCounter = 0;
        frameDelay = 5;  // 动画帧的延迟
        
        // 根据敌人类型设置不同的血量和速度
        switch (type) {
            case SMALL:
                health = 1;  // 小型敌机只需要1发子弹
                speed = 2;
                score = 100;
                dropPropChance = 10;  // 10%几率掉落道具
                damage = 1;  // 小型敌机伤害
                break;
            case MEDIUM:
                health = 2;  // 中型敌机需要2发子弹
                speed = 1;
                score = 300;
                dropPropChance = 20;  // 20%几率掉落道具
                damage = 1;  // 中型敌机伤害
                break;
            case LARGE:
                health = 5;  // 大型敌机需要5发子弹
                speed = 1;
                score = 500;
                dropPropChance = 30;  // 30%几率掉落道具
                damage = 2;  // 大型敌机伤害
                break;
            case BOSS:
                health = 20;  // BOSS需要20发子弹
                speed = 0.5;
                score = 1000;
                dropPropChance = 100;  // 100%几率掉落道具
                damage = 3;  // BOSS伤害最高
                break;
            default:
                health = 1;
                speed = 1;
                score = 1;
                dropPropChance = 0;
                damage = 1;
                break;
        }
        
        shootCounter = 0;  // 初始化射击计数器
        shootInterval = 120;  // 默认发射间隔
        
        // 根据敌人类型设置不同的射击间隔
        if (type == MEDIUM) {
            shootInterval = 100;
        } else if (type == LARGE) {
            shootInterval = 80;
        } else if (type == BOSS) {
            shootInterval = 40;  // BOSS发射更频繁
        }
    }

    bool Show()
    {
        if (rect.top >= height)  // 敌机到屏幕外面
        {
            return false;
        }

        // 更新动画帧，仅在非受伤状态时
        if (enemyImages[type].size() <= 1 || (frameCounter >= frameDelay && currentFrame != 1)) {
            frameCounter++;
            if (frameCounter >= frameDelay) {
                frameCounter = 0;
                currentFrame = (currentFrame + 1) % enemyImages[type].size();
            }
        } else if (currentFrame == 1) {
            // 如果是受伤状态，只保持几帧然后恢复
            frameCounter++;
            if (frameCounter >= frameDelay * 3) {  // 受伤状态持续时间更长
                frameCounter = 0;
                currentFrame = 0;  // 恢复到正常状态
            }
        }

        rect.top += speed;  // 使用设定的速度
        rect.bottom += speed;
        
        // 显示敌机
        putimage(rect.left, rect.top, &enemyImages[type][currentFrame]);

        return true;
    }

    // 检查是否应该射击
    bool ShouldShoot() {
        if (rect.top < 0) return false;  // 敌机还没完全进入屏幕
        
        shootCounter++;
        if (shootCounter >= shootInterval) {
            shootCounter = 0;
            return true;
        }
        return false;
    }

    // 创建子弹
    EnemyBullet* Shoot() {
        int bulletX = rect.left + (rect.right - rect.left) / 2;
        int bulletY = rect.bottom;
        
        // 根据敌机类型选择不同子弹
        int bulletDamage = damage;
        
        // 不同类型敌机有不同攻击方式
        switch (type) {
            case SMALL:
                // 小型敌机直接射击
                return new EnemyBullet(bulletImages[0], bulletX, bulletY, bulletDamage);
            case MEDIUM:
                // 中型敌机有20%几率发射双子弹
                if (rand() % 100 < 20) {
                    // 创建两个子弹，稍微错开位置
                    EnemyBullet* b1 = new EnemyBullet(bulletImages[0], bulletX - 10, bulletY, bulletDamage);
                    EnemyBullet* b2 = new EnemyBullet(bulletImages[0], bulletX + 10, bulletY, bulletDamage);
                    // 返回第一个子弹，第二个子弹需要在调用处单独添加
                    return b1;
                } else {
                    return new EnemyBullet(bulletImages[0], bulletX, bulletY, bulletDamage);
                }
            case LARGE:
                // 大型敌机有30%几率发射强力子弹
                if (rand() % 100 < 30) {
                    return new EnemyBullet(bulletImages[1], bulletX, bulletY, bulletDamage + 1);
                } else {
                    return new EnemyBullet(bulletImages[0], bulletX, bulletY, bulletDamage);
                }
            case BOSS:
                // BOSS有多种攻击方式
                int attackType = rand() % 3;
                if (attackType == 0) {
                    // 发射三连发子弹
                    EnemyBullet* b1 = new EnemyBullet(bulletImages[1], bulletX, bulletY, bulletDamage);
                    return b1;
                } else if (attackType == 1) {
                    // 发射散弹
                    EnemyBullet* b1 = new EnemyBullet(bulletImages[0], bulletX - 20, bulletY, bulletDamage);
                    return b1;
                } else {
                    // 发射强力子弹
                    return new EnemyBullet(bulletImages[2], bulletX, bulletY, bulletDamage * 2);
                }
        }
        
        // 默认子弹
        return new EnemyBullet(bulletImages[0], bulletX, bulletY, bulletDamage);
    }

    RECT& GetRect() { return rect; }
    int GetDamage() const { return damage; }
    EnemyType GetType() const { return type; }
    int GetScore() const { return score; }

    // 受到伤害并返回是否死亡
    bool TakeDamage(int dmg) {
        health -= dmg;
        // 如果敌人还有血量，但受伤了，显示受伤动画（如果有的话）
        if (health > 0 && enemyImages[type].size() > 1) {
            // 对多于一帧的敌机，设置当前帧为第二帧（受伤状态）
            currentFrame = 1;
        }
        
        if (health <= 0) {
            isDying = true;
            return true;  // 敌人死亡
        }
        return false;  // 敌人受伤但没死
    }

    // 获取健康值
    int GetHealth() const { return health; }

    // 检查是否正在死亡
    bool IsDying() const { return isDying; }

    // 生成道具的概率
    bool ShouldDropProp() const {
        // 根据难度和敌机类型确定掉落概率
        float baseRate = currentDifficulty.propDropRate;
        
        // 越高级的敌机掉落道具的概率越高
        switch (type) {
        case SMALL: return (rand() % 100) < int(baseRate * 100);
        case MEDIUM: return (rand() % 100) < int(baseRate * 150);
        case LARGE: return (rand() % 100) < int(baseRate * 200);
        case BOSS: return true;  // BOSS必定掉落道具
        default: return false;
        }
    }

    // 确定掉落的道具类型
    PropType GetDropPropType() const {
        // 简单随机，可以根据敌机类型和游戏局势调整概率
        return (rand() % 2) == 0 ? BOMB : LIFE;
    }

    // 获取中心位置（用于生成道具和爆炸效果）
    void GetCenter(int& x, int& y) const {
        x = rect.left + (rect.right - rect.left) / 2;
        y = rect.top + (rect.bottom - rect.top) / 2;
    }

private:
    EnemyType type;
    RECT rect;
    int speed;          // 移动速度
    int damage;         // 伤害值
    int health;         // 健康值
    int score;          // 击败后获得的分数
    int currentFrame;   // 当前动画帧
    int frameDelay;     // 帧延迟
    int frameCounter;   // 帧计数器
    bool isDying;       // 是否正在死亡
    int shootCounter;   // 射击计数器
    int shootInterval;  // 射击间隔
    int dropPropChance; // 掉落道具的概率
};

class Bullet {
public:
    Bullet(IMAGE& img, RECT pr)
        :img(img)
    {
        rect.left = pr.left + (pr.right - pr.left) / 2 - img.getwidth() / 2;
        rect.right = rect.left + img.getwidth();
        rect.top = pr.top;
        rect.bottom = rect.top + img.getheight();
    }

    bool Show()
    {
        if (rect.bottom <= 0)
        {
            return false;
        }
        rect.top -= 3;
        rect.bottom -= 3;
        putimage(rect.left, rect.top, &img);

        return true;
    }

    RECT& GetRect() { return rect; }

private:
    IMAGE& img;
    RECT rect;
};

// 道具类实现
class Prop {
public:
    Prop(PropType type, int x, int y)
        : type(type), speedY(2), isActive(true)
    {
        rect.left = x - propImages[type].getwidth() / 2;
        rect.right = rect.left + propImages[type].getwidth();
        rect.top = y - propImages[type].getheight() / 2;
        rect.bottom = rect.top + propImages[type].getheight();
    }

    bool Show() {
        if (!isActive || rect.top >= height) return false;

        rect.top += speedY;
        rect.bottom += speedY;
        putimage(rect.left, rect.top, &propImages[type]);
        return true;
    }

    RECT& GetRect() { return rect; }
    PropType GetType() const { return type; }
    void Deactivate() { isActive = false; }

private:
    PropType type;
    RECT rect;
    int speedY;
    bool isActive;
};

// 爆炸效果管理类
class ExplosionManager {
public:
    // 添加爆炸效果
    void AddExplosion(int x, int y, EnemyType type) {
        explosions.push_back(Explosion(x - explosionImages[type][0].getwidth() / 2, 
                                       y - explosionImages[type][0].getheight() / 2, 
                                       explosionImages[type]));
    }

    // 添加英雄爆炸效果
    void AddHeroExplosion(int x, int y) {
        explosions.push_back(Explosion(x - heroExplosionImages[0].getwidth() / 2, 
                                       y - heroExplosionImages[0].getheight() / 2, 
                                       heroExplosionImages));
    }

    // 清除全部爆炸效果
    void ClearAll() {
        explosions.clear();
    }

    // 更新并显示所有爆炸效果
    void Update() {
        auto it = explosions.begin();
        while (it != explosions.end()) {
            if (it->Update()) {
                it->Draw();
                ++it;
            } else {
                it = explosions.erase(it);
            }
        }
    }

    // 获取爆炸效果列表，用于暂停时绘制但不更新
    const vector<Explosion>& GetExplosions() const {
        return explosions;
    }

private:
    vector<Explosion> explosions;
};

// 游戏资源加载函数
bool LoadGameResources() {
    bool success = true;

    // 加载背景图像
    loadimage(&backgroundImage, "img\\bg.png", width, height);
    if (backgroundImage.getwidth() <= 0 || backgroundImage.getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载背景图像"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // 加载暂停菜单图像
    loadimage(&pauseMenuImage, "img\\game_pause_nor.png");
    loadimage(&resumeButtonNor, "img\\resume_nor.png");
    loadimage(&resumeButtonSel, "img\\resume_sel.png");
    loadimage(&restartButtonNor, "img\\restart_nor.png");
    loadimage(&restartButtonSel, "img\\restart_sel.png");
    loadimage(&quitButtonNor, "img\\quit_nor.png");
    loadimage(&quitButtonSel, "img\\quit_sel.png");

    // 加载英雄飞机图像
    heroImages.resize(2);
    loadimage(&heroImages[0], "img\\hero1.png");
    loadimage(&heroImages[1], "img\\hero2.png");
    if (heroImages[0].getwidth() <= 0 || heroImages[0].getheight() <= 0 ||
        heroImages[1].getwidth() <= 0 || heroImages[1].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载英雄图像"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // 加载英雄爆炸效果
    heroExplosionImages.resize(4);
    loadimage(&heroExplosionImages[0], "img\\hero_blowup_n1.png");
    loadimage(&heroExplosionImages[1], "img\\hero_blowup_n2.png");
    loadimage(&heroExplosionImages[2], "img\\hero_blowup_n3.png");
    loadimage(&heroExplosionImages[3], "img\\hero_blowup_n4.png");
    if (heroExplosionImages[0].getwidth() <= 0 || heroExplosionImages[0].getheight() <= 0 ||
        heroExplosionImages[1].getwidth() <= 0 || heroExplosionImages[1].getheight() <= 0 ||
        heroExplosionImages[2].getwidth() <= 0 || heroExplosionImages[2].getheight() <= 0 ||
        heroExplosionImages[3].getwidth() <= 0 || heroExplosionImages[3].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载英雄爆炸效果"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // 加载敌机图像
    // 小型敌机
    enemyImages[SMALL].resize(1);
    loadimage(&enemyImages[SMALL][0], "img\\enemy0.png");
    if (enemyImages[SMALL][0].getwidth() <= 0 || enemyImages[SMALL][0].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载小型敌机图像"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // 中型敌机
    enemyImages[MEDIUM].resize(2);
    loadimage(&enemyImages[MEDIUM][0], "img\\enemy1.png");
    loadimage(&enemyImages[MEDIUM][1], "img\\enemy1_hit.png");
    if (enemyImages[MEDIUM][0].getwidth() <= 0 || enemyImages[MEDIUM][0].getheight() <= 0 ||
        enemyImages[MEDIUM][1].getwidth() <= 0 || enemyImages[MEDIUM][1].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载中型敌机图像"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // 大型敌机
    enemyImages[LARGE].resize(2);
    loadimage(&enemyImages[LARGE][0], "img\\enemy2.png");
    loadimage(&enemyImages[LARGE][1], "img\\enemy2_hit.png");
    if (enemyImages[LARGE][0].getwidth() <= 0 || enemyImages[LARGE][0].getheight() <= 0 ||
        enemyImages[LARGE][1].getwidth() <= 0 || enemyImages[LARGE][1].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载大型敌机图像"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // BOSS敌机
    enemyImages[BOSS].resize(2);
    loadimage(&enemyImages[BOSS][0], "img\\enemy2_n2.png");
    loadimage(&enemyImages[BOSS][1], "img\\enemy2_hit.png");
    if (enemyImages[BOSS][0].getwidth() <= 0 || enemyImages[BOSS][0].getheight() <= 0 ||
        enemyImages[BOSS][1].getwidth() <= 0 || enemyImages[BOSS][1].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载BOSS敌机图像"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // 加载敌机爆炸效果
    // 小型敌机爆炸
    explosionImages[SMALL].resize(4);
    loadimage(&explosionImages[SMALL][0], "img\\enemy0_down1.png");
    loadimage(&explosionImages[SMALL][1], "img\\enemy0_down2.png");
    loadimage(&explosionImages[SMALL][2], "img\\enemy0_down3.png");
    loadimage(&explosionImages[SMALL][3], "img\\enemy0_down4.png");
    if (explosionImages[SMALL][0].getwidth() <= 0 || explosionImages[SMALL][0].getheight() <= 0 ||
        explosionImages[SMALL][1].getwidth() <= 0 || explosionImages[SMALL][1].getheight() <= 0 ||
        explosionImages[SMALL][2].getwidth() <= 0 || explosionImages[SMALL][2].getheight() <= 0 ||
        explosionImages[SMALL][3].getwidth() <= 0 || explosionImages[SMALL][3].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载小型敌机爆炸效果"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // 中型敌机爆炸
    explosionImages[MEDIUM].resize(4);
    loadimage(&explosionImages[MEDIUM][0], "img\\enemy1_down1.png");
    loadimage(&explosionImages[MEDIUM][1], "img\\enemy1_down2.png");
    loadimage(&explosionImages[MEDIUM][2], "img\\enemy1_down3.png");
    loadimage(&explosionImages[MEDIUM][3], "img\\enemy1_down4.png");
    if (explosionImages[MEDIUM][0].getwidth() <= 0 || explosionImages[MEDIUM][0].getheight() <= 0 ||
        explosionImages[MEDIUM][1].getwidth() <= 0 || explosionImages[MEDIUM][1].getheight() <= 0 ||
        explosionImages[MEDIUM][2].getwidth() <= 0 || explosionImages[MEDIUM][2].getheight() <= 0 ||
        explosionImages[MEDIUM][3].getwidth() <= 0 || explosionImages[MEDIUM][3].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载中型敌机爆炸效果"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // 大型敌机爆炸
    explosionImages[LARGE].resize(6);
    loadimage(&explosionImages[LARGE][0], "img\\enemy2_down1.png");
    loadimage(&explosionImages[LARGE][1], "img\\enemy2_down2.png");
    loadimage(&explosionImages[LARGE][2], "img\\enemy2_down3.png");
    loadimage(&explosionImages[LARGE][3], "img\\enemy2_down4.png");
    loadimage(&explosionImages[LARGE][4], "img\\enemy2_down5.png");
    loadimage(&explosionImages[LARGE][5], "img\\enemy2_down6.png");
    if (explosionImages[LARGE][0].getwidth() <= 0 || explosionImages[LARGE][0].getheight() <= 0 ||
        explosionImages[LARGE][1].getwidth() <= 0 || explosionImages[LARGE][1].getheight() <= 0 ||
        explosionImages[LARGE][2].getwidth() <= 0 || explosionImages[LARGE][2].getheight() <= 0 ||
        explosionImages[LARGE][3].getwidth() <= 0 || explosionImages[LARGE][3].getheight() <= 0 ||
        explosionImages[LARGE][4].getwidth() <= 0 || explosionImages[LARGE][4].getheight() <= 0 ||
        explosionImages[LARGE][5].getwidth() <= 0 || explosionImages[LARGE][5].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载大型敌机爆炸效果"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // BOSS爆炸效果与大型敌机相同
    explosionImages[BOSS] = explosionImages[LARGE];

    // 加载子弹图像
    bulletImages.resize(3);
    loadimage(&bulletImages[0], "img\\bullet1.png");
    loadimage(&bulletImages[1], "img\\bullet2.png");
    loadimage(&bulletImages[2], "img\\bullet.png");
    if (bulletImages[0].getwidth() <= 0 || bulletImages[0].getheight() <= 0 ||
        bulletImages[1].getwidth() <= 0 || bulletImages[1].getheight() <= 0 ||
        bulletImages[2].getwidth() <= 0 || bulletImages[2].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载子弹图像"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    // 加载道具图像
    loadimage(&propImages[BOMB], "img\\prop_type_0.png");
    loadimage(&propImages[LIFE], "img\\prop_type_1.png");
    if (propImages[BOMB].getwidth() <= 0 || propImages[BOMB].getheight() <= 0 ||
        propImages[LIFE].getwidth() <= 0 || propImages[LIFE].getheight() <= 0) {
        MessageBox(GetHWnd(), _T("无法加载道具图像"), _T("错误"), MB_OK | MB_ICONERROR);
        success = false;
    }

    return success;
}

// 添加处理暂停菜单的函数
enum PauseMenuResult {
    RESUME,     // 继续游戏
    RESTART,    // 重新开始
    QUIT        // 退出到主菜单
};

PauseMenuResult ShowPauseMenu() {
    // 创建半透明背景，使用RGB代替RGBA，因为EasyX不直接支持RGBA
    setfillcolor(RGB(0, 0, 0));  // 黑色
    settextstyle(40, 0, _T("黑体"));
    setbkmode(TRANSPARENT);  // 设置文字背景为透明

    // 使用半透明效果绘制背景矩形
    setfillstyle(BS_SOLID);
   // settransparent(50);  // 设置透明度为50%
    fillrectangle(0, 0, width, height);
   // settransparent(255);  // 重置透明度

    // 显示暂停菜单图片
    int pauseMenuX = width / 2 - pauseMenuImage.getwidth() / 2;
    int pauseMenuY = height / 2 - pauseMenuImage.getheight() / 2 - 80;
    putimage(pauseMenuX, pauseMenuY, &pauseMenuImage);

    // 设置按钮位置
    int buttonSpacing = 20;  // 按钮间距
    int buttonY = pauseMenuY + pauseMenuImage.getheight() + buttonSpacing;

    // 继续按钮位置
    RECT resumeRect;
    resumeRect.left = width / 2 - resumeButtonNor.getwidth() / 2;
    resumeRect.right = resumeRect.left + resumeButtonNor.getwidth();
    resumeRect.top = buttonY;
    resumeRect.bottom = resumeRect.top + resumeButtonNor.getheight();

    // 重新开始按钮位置
    RECT restartRect;
    restartRect.left = width / 2 - restartButtonNor.getwidth() / 2;
    restartRect.right = restartRect.left + restartButtonNor.getwidth();
    restartRect.top = resumeRect.bottom + buttonSpacing;
    restartRect.bottom = restartRect.top + restartButtonNor.getheight();

    // 退出按钮位置
    RECT quitRect;
    quitRect.left = width / 2 - quitButtonNor.getwidth() / 2;
    quitRect.right = quitRect.left + quitButtonNor.getwidth();
    quitRect.top = restartRect.bottom + buttonSpacing;
    quitRect.bottom = quitRect.top + quitButtonNor.getheight();

    // 显示菜单标题
    settextcolor(WHITE);
    settextstyle(40, 0, _T("黑体"));
    LPCTSTR pauseTitle = _T("游戏暂停");
    outtextxy(width / 2 - textwidth(pauseTitle) / 2, pauseMenuY - 60, pauseTitle);

    // 鼠标状态
    bool resumeHover = false;
    bool restartHover = false;
    bool quitHover = false;

    // 绘制初始按钮状态
    putimage(resumeRect.left, resumeRect.top, &resumeButtonNor);
    putimage(restartRect.left, restartRect.top, &restartButtonNor);
    putimage(quitRect.left, quitRect.top, &quitButtonNor);

    FlushBatchDraw();

    while (true) {
        ExMessage msg;
        if (peekmessage(&msg, EM_MOUSE)) {
            // 检查鼠标位置，更新按钮状态
            bool newResumeHover = PointInRect(msg.x, msg.y, resumeRect);
            bool newRestartHover = PointInRect(msg.x, msg.y, restartRect);
            bool newQuitHover = PointInRect(msg.x, msg.y, quitRect);

            // 如果状态改变，更新按钮显示
            if (newResumeHover != resumeHover) {
                resumeHover = newResumeHover;
                putimage(resumeRect.left, resumeRect.top, resumeHover ? &resumeButtonSel : &resumeButtonNor);
                FlushBatchDraw();
            }

            if (newRestartHover != restartHover) {
                restartHover = newRestartHover;
                putimage(restartRect.left, restartRect.top, restartHover ? &restartButtonSel : &restartButtonNor);
                FlushBatchDraw();
            }

            if (newQuitHover != quitHover) {
                quitHover = newQuitHover;
                putimage(quitRect.left, quitRect.top, quitHover ? &quitButtonSel : &quitButtonNor);
                FlushBatchDraw();
            }

            // 检查鼠标点击
            if (msg.message == WM_LBUTTONDOWN) {
                if (resumeHover) {
                    return RESUME;
                }
                else if (restartHover) {
                    return RESTART;
                }
                else if (quitHover) {
                    return QUIT;
                }
            }
        }

        // 检查键盘 - 如果按Esc或空格则继续游戏
        if (GetAsyncKeyState(VK_ESCAPE) & 0x0001 || GetAsyncKeyState(VK_SPACE) & 0x0001) {
            return RESUME;
        }

        Sleep(10);
    }

    return RESUME;  // 默认继续游戏
}

bool play() {
    settextcolor(BLACK);
    outtextxy(tplayr.left, tplayr.top, tplay);
    outtextxy(texitr.left, texitr.top, texit);

    bool is_play = true;
    bool should_restart = false;  // 标记是否应该重启游戏
    isPaused = false;

    // 加载游戏资源
    if (!LoadGameResources()) {
        MessageBox(GetHWnd(), _T("游戏资源加载失败，游戏可能无法正常运行"), _T("警告"), MB_OK | MB_ICONWARNING);
        return false;
    }

    Hero hp(heroImages);
    vector<Enemy*> es;
    vector<Bullet*> bs;
    vector<EnemyBullet*> ebs;  // 添加敌人子弹容器
    vector<Prop*> props;
    ExplosionManager explosionMgr;
    int bsing = 0;

    unsigned long long kill = 0;
    unsigned long long score = 0;
    bool bossExists = false;

    // 根据难度生成初始敌机
    for (int i = 0; i < currentDifficulty.enemyCount; i++) {
        EnemyType type = static_cast<EnemyType>(rand() % 3); // 初始不生成BOSS
        es.push_back(new Enemy(type, abs(rand()) % (width - enemyImages[type][0].getwidth()), -enemyImages[type][0].getheight()));
    }

    // 游戏开始时间
    gameStartTime = clock();
    gameTime = 0;
    showedBossWarning = false;  // 重置BOSS警告标志

    while (is_play && !should_restart) {  // 当should_restart为true时退出循环
        // 更新游戏时间
        if (!isPaused) {
            gameTime = (clock() - gameStartTime) / CLOCKS_PER_SEC;
        }

        // 处理子弹发射
        if (!isPaused) {
            bsing++;
            if (bsing >= currentDifficulty.bulletInterval) {
                bs.push_back(new Bullet(bulletImages[rand() % 3], hp.GetRect()));
                bsing = 0;
            }
        }

        // 处理玩家输入
        if (_kbhit()) {
            char v = _getch();
            if (v == ' ') {  // 空格键 - 修复为字符，不用virtual key code
                isPaused = !isPaused;
                
                if (isPaused) {
                    // 显示暂停菜单
                    PauseMenuResult result = ShowPauseMenu();
                    
                    switch (result) {
                        case RESUME:
                            isPaused = false;  // 继续游戏
                            break;
                        case RESTART:
                            should_restart = true;  // 设置重启标志
                            break;
                        case QUIT:
                            is_play = false;  // 退出游戏
                            break;
                    }
                }
                
                Sleep(200);  // 防止连续触发
            }
            else if (v == 'b' || v == 'B') {  // B键使用炸弹
                if (!isPaused && hp.HasBomb()) {
                    // 使用炸弹，清除所有敌机
                    if (hp.UseBomb()) {
                        // 为每个敌机添加爆炸效果
                        for (auto e : es) {
                            if (e) {
                                int x, y;
                                e->GetCenter(x, y);
                                explosionMgr.AddExplosion(x, y, e->GetType());
                                score += e->GetScore() * 100; // 使用炸弹击败敌人获得更多分数
                            }
                        }
                        
                        // 清除所有敌机
                        for (auto e : es) {
                            if (e) delete e;
                        }
                        es.clear();
                        bossExists = false; // 如果有BOSS也会被消灭
                        
                        // 清除所有敌人子弹
                        for (auto eb : ebs) {
                            if (eb) delete eb;
                        }
                        ebs.clear();
                        
                        // 显示炸弹特效
                        setfillcolor(RGB(255, 200, 100));
                        fillrectangle(0, 0, width, height);
                        FlushBatchDraw();
                        Sleep(50);
                    }
                }
            }
        }

        // 使用GetAsyncKeyState检测ESC键，也触发暂停
        if (GetAsyncKeyState(VK_ESCAPE) & 0x0001) {  // ESC键
            isPaused = true;
            
            // 显示暂停菜单
            PauseMenuResult result = ShowPauseMenu();
            
            switch (result) {
                case RESUME:
                    isPaused = false;  // 继续游戏
                    break;
                case RESTART:
                    should_restart = true;  // 设置重启标志
                    break;
                case QUIT:
                    is_play = false;  // 退出游戏
                    break;
            }
            
            Sleep(200);  // 防止连续触发
        }

        BeginBatchDraw();
        cleardevice();

        // 显示游戏背景
        putimage(0, 0, &backgroundImage);

        // 显示计分和计时
        TCHAR scoreText[50];
        _stprintf_s(scoreText, 50, _T("分数: %llu"), score);
        settextcolor(BLACK);
        settextstyle(20, 0, _T("黑体"));
        outtextxy(width - textwidth(scoreText) - 10, 10, scoreText);

        TCHAR timeText[50];
        _stprintf_s(timeText, 50, _T("时间: %d秒"), gameTime);
        outtextxy(width - textwidth(timeText) - 10, 40, timeText);

        // 如果没有暂停，则更新游戏状态
        if (!isPaused) {
            hp.Control();
            hp.Show();

            // 检查分数，如果达到BOSS出现分数且当前没有BOSS，则生成BOSS
            if (score >= currentDifficulty.bossAppearScore && !bossExists) {
                es.push_back(new Enemy(BOSS, width / 2 - enemyImages[BOSS][0].getwidth() / 2, -enemyImages[BOSS][0].getheight()));
                bossExists = true;
                
                // 只有在第一次显示BOSS警告
                if (!showedBossWarning) {
                    // 显示BOSS警告
                    settextcolor(RED);
                    settextstyle(40, 0, _T("黑体"));
                    LPCTSTR bossWarning = _T("警告：BOSS来袭！");
                    outtextxy(width / 2 - textwidth(bossWarning) / 2, height / 2, bossWarning);
                    FlushBatchDraw();
                    Sleep(2000);  // 显示警告2秒
                    showedBossWarning = true;  // 设置标志，避免重复显示
                }
            }

            // 更新子弹
            auto bit = bs.begin();
            while (bit != bs.end()) {
                if (!(*bit)->Show()) {
                    delete (*bit);
                    (*bit) = nullptr;
                    bit = bs.erase(bit);
                }
                else {
                    bit++;
                }
            }

            // 处理敌人射击
            for (auto e : es) {
                if (e && e->ShouldShoot()) {
                    EnemyBullet* bullet = e->Shoot();
                    if (bullet) {
                        ebs.push_back(bullet);
                        
                        // 处理中型敌机的双子弹
                        if (e->GetType() == MEDIUM && rand() % 100 < 20) {
                            int bulletX = e->GetRect().left + (e->GetRect().right - e->GetRect().left) / 2;
                            int bulletY = e->GetRect().bottom;
                            ebs.push_back(new EnemyBullet(bulletImages[0], bulletX + 10, bulletY, e->GetDamage()));
                        }
                        
                        // 处理BOSS的多发子弹
                        if (e->GetType() == BOSS) {
                            int bulletX = e->GetRect().left + (e->GetRect().right - e->GetRect().left) / 2;
                            int bulletY = e->GetRect().bottom;
                            
                            // BOSS的散弹攻击
                            if (rand() % 3 == 1) {
                                ebs.push_back(new EnemyBullet(bulletImages[0], bulletX - 20, bulletY, e->GetDamage()));
                                ebs.push_back(new EnemyBullet(bulletImages[0], bulletX + 20, bulletY, e->GetDamage()));
                            }
                            
                            // BOSS的三连发攻击
                            if (rand() % 3 == 0) {
                                // 延迟发射第二、第三发子弹
                                static int bossShootDelay = 0;
                                bossShootDelay = 10; // 设置延迟计数
                            }
                        }
                    }
                }
            }
            
            // 处理BOSS的连发子弹延迟
            static int bossShootDelay = 0;
            if (bossShootDelay > 0) {
                bossShootDelay--;
                if (bossShootDelay % 5 == 0) { // 每5帧发射一颗
                    // 寻找场上的BOSS
                    for (auto e : es) {
                        if (e && e->GetType() == BOSS) {
                            int bulletX = e->GetRect().left + (e->GetRect().right - e->GetRect().left) / 2;
                            int bulletY = e->GetRect().bottom;
                            ebs.push_back(new EnemyBullet(bulletImages[1], bulletX, bulletY, e->GetDamage()));
                            break;
                        }
                    }
                }
            }

            // 更新敌人子弹
            auto ebit = ebs.begin();
            while (ebit != ebs.end()) {
                // 检测子弹与玩家碰撞
                if (RectDuangRect((*ebit)->GetRect(), hp.GetRect())) {
                    if (hp.TakeDamage((*ebit)->GetDamage())) {
                        // 玩家死亡，添加爆炸效果
                        int x = hp.GetRect().left + (hp.GetRect().right - hp.GetRect().left) / 2;
                        int y = hp.GetRect().top + (hp.GetRect().bottom - hp.GetRect().top) / 2;
                        explosionMgr.AddHeroExplosion(x, y);
                        is_play = false;  // 生命值为0，游戏结束
                    }
                    
                    delete (*ebit);
                    (*ebit) = nullptr;
                    ebit = ebs.erase(ebit);
                }
                else if (!(*ebit)->Show()) {
                    delete (*ebit);
                    (*ebit) = nullptr;
                    ebit = ebs.erase(ebit);
                }
                else {
                    ebit++;
                }
            }

            // 更新道具
            auto pit = props.begin();
            while (pit != props.end()) {
                bool collected = false;

                // 检测道具与玩家碰撞
                if (RectDuangRect((*pit)->GetRect(), hp.GetRect())) {
                    // 根据道具类型给予效果
                    switch ((*pit)->GetType()) {
                    case BOMB:
                        hp.AddBomb();
                        break;
                    case LIFE:
                        hp.AddLife();
                        break;
                    }
                    
                    collected = true;
                    delete (*pit);
                    (*pit) = nullptr;
                    pit = props.erase(pit);
                }
                else if (!(*pit)->Show()) {
                    delete (*pit);
                    (*pit) = nullptr;
                    pit = props.erase(pit);
                }
                else {
                    pit++;
                }
            }

            // 更新敌机
            auto it = es.begin();
            while (it != es.end()) {
                bool hit = false;

                // 检测敌机与玩家碰撞
                if (RectDuangRect((*it)->GetRect(), hp.GetRect())) {
                    if (hp.TakeDamage((*it)->GetDamage())) {
                        // 玩家死亡，添加爆炸效果
                        int x = hp.GetRect().left + (hp.GetRect().right - hp.GetRect().left) / 2;
                        int y = hp.GetRect().top + (hp.GetRect().bottom - hp.GetRect().top) / 2;
                        explosionMgr.AddHeroExplosion(x, y);
                        is_play = false;  // 生命值为0，游戏结束
                    }

                    // 添加敌机爆炸效果
                    int x, y;
                    (*it)->GetCenter(x, y);
                    explosionMgr.AddExplosion(x, y, (*it)->GetType());
                    
                    // 如果是BOSS，更新状态
                    if ((*it)->GetType() == BOSS) {
                        bossExists = false;
                    }

                    delete (*it);
                    (*it) = nullptr;
                    it = es.erase(it);
                    continue;
                }

                // 检测敌机与子弹碰撞
                bit = bs.begin();
                while (bit != bs.end() && !hit) {
                    if (RectDuangRect((*bit)->GetRect(), (*it)->GetRect())) {
                        delete (*bit);
                        (*bit) = nullptr;
                        bit = bs.erase(bit);

                        // 敌机受到伤害，根据敌机类型可能需要多次命中才会被摧毁
                        if ((*it)->TakeDamage(1)) {  // 传入固定伤害值1
                            // 敌机死亡，添加爆炸效果
                            int x, y;
                            (*it)->GetCenter(x, y);
                            explosionMgr.AddExplosion(x, y, (*it)->GetType());
                            
                            // 更新分数
                            score += (*it)->GetScore();
                            kill++;

                            // 检查是否需要生成道具
                            if ((*it)->ShouldDropProp()) {
                                PropType propType = (*it)->GetDropPropType();
                                props.push_back(new Prop(propType, x, y));
                            }
                            
                            // 如果是BOSS，更新状态
                            if ((*it)->GetType() == BOSS) {
                                bossExists = false;
                            }

                        delete (*it);
                        (*it) = nullptr;
                        it = es.erase(it);

                        hit = true;
                        }
                    }
                    else {
                        bit++;
                    }
                }

                if (!hit) {
                    if (!(*it)->Show()) {
                        // 如果是BOSS，更新状态
                        if ((*it)->GetType() == BOSS) {
                            bossExists = false;
                        }
                        
                        delete (*it);
                        (*it) = nullptr;
                        it = es.erase(it);
                    }
                    else {
                        it++;
                    }
                }
            }

            // 生成新敌机，保持敌机数量符合难度设置
            while (es.size() < currentDifficulty.enemyCount) {
                // 生成随机敌机类型，但BOSS只有在达到特定分数时才会生成
                EnemyType type;
                if (score >= currentDifficulty.bossAppearScore && !bossExists && (rand() % 100 < 10)) {
                    type = BOSS;
                    bossExists = true;
                } else {
                    // 根据游戏进度调整敌机类型概率
                    int r = rand() % 100;
                    if (r < 60) type = SMALL;
                    else if (r < 90) type = MEDIUM;
                    else type = LARGE;
                }
                
                es.push_back(new Enemy(type, abs(rand()) % (width - enemyImages[type][0].getwidth()), -enemyImages[type][0].getheight()));
            }

            // 更新爆炸效果
            explosionMgr.Update();
        }
        else {
            // 暂停状态下仍然显示飞机和敌机，但不更新位置
            hp.Show();

            for (auto b : bs) {
                if (b) b->Show();
            }
            
            for (auto eb : ebs) {
                if (eb) eb->Show();
            }

            for (auto e : es) {
                if (e) e->Show();
            }

            for (auto p : props) {
                if (p) p->Show();
            }

            // 显示爆炸效果，但不更新动画
            for (auto& explosion : explosionMgr.GetExplosions()) {
                explosion.Draw();
            }
        }

        EndBatchDraw();

        Sleep(20);
    }

    // 释放资源
    for (auto e : es) {
        if (e) delete e;
    }

    for (auto b : bs) {
        if (b) delete b;
    }

    for (auto eb : ebs) {
        if (eb) delete eb;
    }

    for (auto p : props) {
        if (p) delete p;
    }

    if (!is_play && !should_restart) {
        // 只有真正游戏结束时才显示结束界面
        Over(score);
    }
    
    return should_restart;  // 返回是否需要重新开始
}

// 主函数
int main() {
    srand((unsigned)time(NULL));
    initgraph(width, height, EX_NOMINIMIZE | EX_SHOWCONSOLE);

    // 初始化星星
    for (int i = 0; i < MAXSTAR; i++) {
        InitStar(i);
        star[i].x = rand() % width;
    }

    // 加载分数记录
    LoadScoreRecords();

    // 设置默认难度
    currentDifficulty = NORMAL;

    std::atomic<bool> running(true);
    std::thread starThread(moveStars, std::ref(running));

    bool is_live = true;
    while (is_live) {
        start();
        is_live = play();
    }

    running = false;
    starThread.join();

    closegraph();
    return 0;
}