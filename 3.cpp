#pragma execution_character_set("utf-8")

//【关键1：头文件顺序强制前置 winsock2.h】
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")
#include <windows.h>
#include <conio.h>
#include <ctime>
#include <io.h>
#include <shellapi.h>
#undef bind
#include <bits/stdc++.h>
using namespace std;

//===========================================================================
// 全局常量 & 变量
//===========================================================================
const int N = 15;
int nx, ny;
int x_, y_;
string name1, name2;
char oc = '.';
bool color = 1;
string Sudoku_board[10];
char table[N][N];
bool ff, fff;
bool kfz = 0;
// 1=方向键模式；2=WASD模式
int controlMode = 1;

const int Size = 15;
const int BOARD_SIZE = 3;

//===========================================================================
// 用户数据结构
//===========================================================================
struct UserData {
	char username[32];
	char passwordHash[33];
	int score = 0;
	double scoreRate = 1.0;
	
	__time64_t vipEndTime = 0;
	__time64_t boostEndTime = 0;
	__time64_t fastPassEndTime = 0;
	
	char tip[128];
	bool hasPermanentVIP = false;
};

// ✅ 全局用户变量（必须放在所有函数之前）
UserData curUser;
bool loggedIn = false;

// ✅ 开发者模式（密码：无，SHA-256 + Base64 双加密验证）
bool isDeveloper = false;

//===========================================================================
// 函数声明（防止重定义和作用域错误）
//===========================================================================
int jz(int i = 2);
void wzq();
void Gobang();
void Tic_Tac_Toe();
void Tic_Tac_Toe_1();
void NetGobang();
void NetTTT();
void minx_cjwt();
void shop();
void loginMenu();
void settingsMenu();

// ---- 用户系统 ----
void ensureUserDir();
bool saveUser(const UserData& u);
bool loadUser(const char* username, const char* password, UserData& out);
void checkExpire();

// ---- AI 评分系统 ----
void countLine(char board[Size][Size], int x, int y, int dx, int dy, char player, int& count, int& openEnds);
int evaluatePoint(char board[Size][Size], int x, int y, char player);
int aiEvaluate(char board[Size][Size], int x, int y, char aiPlayer);
int getGameScore(char board[Size][Size], char player);

// ---- 积分系统 ----
void settleScore(int aiRawScore, int winStatus);
bool buyItem(int cost);

// ---- 商店系统 ----
void showShopStatus();
void buyBoostCard();       // 积分翻倍卡
void buyFastPass();        // VIP加速卡
void buyDayVIP();          // 1天VIP权限卡
void buyPermVIP();         // 永久VIP（已移除密码激活，保留接口）
void sellScore();          // 出售积分换经验值
void shopHelp();           // 商店说明

// ---- 开发者模式 ----
bool verifyDevPassword(const char* input);
void activateDevMode();
bool isDevMode();

//===========================================================================
// 网络 & 哈希全局定义（预加载对象）
//===========================================================================
#define PORT 9999
#define BUF_LEN 256
SOCKET serverSock, clientSock, curSock;
SOCKADDR_IN serverAddr, clientAddr;
WSADATA wsaData;
bool netInitOk = false;

//===========================================================================
// 用户系统实现
//===========================================================================
void ensureUserDir() {
	CreateDirectoryA("User", NULL);
}

// 简易8位哈希函数
unsigned char simpleHash(const char* data, int len) {
	unsigned char res = 0x37;
	for (int i = 0; i < len; i++) {
		res ^= data[i];
		res = (res << 1) | (res >> 7);
	}
	return res;
}

bool saveUser(const UserData& u) {
	ensureUserDir();
	char path[MAX_PATH];
	int id = 0;
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA("User/User*.key", &fd);
	bool found = false;
	
	do {
		if (strstr(fd.cFileName, u.username)) {
			sscanf(fd.cFileName, "User%02d.key", &id);
			found = true;
			break;
		}
	} while (FindNextFileA(h, &fd));
	
	if (!found) {
		while (true) {
			sprintf(path, "User/User%02d.key", id);
			if (_access(path, 0) == -1) break;
			++id;
		}
	}
	
	sprintf(path, "User/User%02d.key", id);
	FILE* fp = fopen(path, "wb");
	if (!fp) return false;
	
	fwrite(&u, sizeof(UserData), 1, fp);
	unsigned char hash = simpleHash((const char*)&u, sizeof(UserData));
	fwrite(&hash, 1, 1, fp);
	fclose(fp);
	return true;
}

bool loadUser(const char* username, const char* password, UserData& out) {
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA("User/User*.key", &fd);
	
	do {
		FILE* fp = fopen(("User/" + string(fd.cFileName)).c_str(), "rb");
		if (!fp) continue;
		
		UserData u;
		fread(&u, sizeof(UserData), 1, fp);
		
		unsigned char storedHash;
		fread(&storedHash, 1, 1, fp);
		fclose(fp);
		
		unsigned char calcHash = simpleHash((const char*)&u, sizeof(UserData));
		
		if (storedHash != calcHash) {
			MessageBoxA(NULL, "用户数据损坏，已自动回档", "警告", MB_OK | MB_ICONWARNING);
			DeleteFileA(("User/" + string(fd.cFileName)).c_str());
			continue;
		}
		
		if (strcmp(u.username, username) == 0 &&
			strcmp(u.passwordHash,
				to_string(simpleHash(password, (int)strlen(password))).c_str()) == 0) {
			out = u;
			return true;
		}
	} while (FindNextFileA(h, &fd));
	
	return false;
}

void checkExpire() {
	// 开发者模式：所有特权永不过期
	if (isDeveloper) {
		curUser.scoreRate = 2.0;
		curUser.vipEndTime = LLONG_MAX;
		curUser.boostEndTime = LLONG_MAX;
		curUser.fastPassEndTime = LLONG_MAX;
		curUser.score = 999999999;
		kfz = 1;
		return;
	}
	
	__time64_t now = _time64(nullptr);
	
	// VIP 过期
	if (now > curUser.vipEndTime && !curUser.hasPermanentVIP) {
		curUser.scoreRate = 1.0;
		strcpy(curUser.tip, "你知道吗？VIP 的密码是发布文章的网址");
	}
	// 翻倍卡过期
	if (now > curUser.boostEndTime) {
		curUser.boostEndTime = 0;
	}
	// VIP加速卡过期
	if (now > curUser.fastPassEndTime) {
		kfz = 0;
	}
	saveUser(curUser);
}

//===========================================================================
// 登录 / 注册
//===========================================================================
void loginMenu() {
	system("cls");
	ensureUserDir();
	
	cout << "==== 用户系统 ====\n";
	cout << "1. 登录\n";
	cout << "2. 注册\n>> ";
	
	int choice;
	cin >> choice;
	
	string username, password;
	
	if (choice == 2) {
		UserData u{};
		cout << "请输入用户名：";
		cin >> u.username;
		
		// 防重名检查
		WIN32_FIND_DATAA fd;
		HANDLE h = FindFirstFileA("User/User*.key", &fd);
		do {
			FILE* fp = fopen(("User/" + string(fd.cFileName)).c_str(), "rb");
			if (!fp) continue;
			UserData tmp;
			fread(&tmp, sizeof(UserData), 1, fp);
			fclose(fp);
			if (strcmp(tmp.username, u.username) == 0) {
				MessageBoxA(NULL, "用户名已存在", "注册失败", MB_OK | MB_ICONHAND);
				loginMenu();
				return;
			}
		} while (FindNextFileA(h, &fd));
		
		cout << "请输入密码：";
		cin >> password;
		
		strcpy(u.passwordHash,
			to_string(simpleHash(password.c_str(), password.size())).c_str());
		strcpy(u.tip, "你知道吗？VIP 的密码是发布文章的网址");
		
		saveUser(u);
		cout << "注册成功！\n";
		Sleep(800);
		loginMenu();
		return;
	}
	
	// 登录
	cout << "用户名：";
	cin >> username;
	cout << "密码：";
	cin >> password;
	
	if (loadUser(username.c_str(), password.c_str(), curUser)) {
		loggedIn = true;
		checkExpire();
		if (curUser.hasPermanentVIP) kfz = 1;
		cout << "欢迎回来，" << curUser.username << "！\n";
		Sleep(800);
	} else {
		MessageBoxA(NULL, "用户名或密码错误", "登录失败", MB_OK | MB_ICONHAND);
		loginMenu();
	}
}

//===========================================================================
// 商店系统（全面升级版）
//===========================================================================

// ---- 辅助：格式化剩余时间 ----
string formatTimeLeft(__time64_t expireTime) {
	__time64_t now = _time64(nullptr);
	if (expireTime <= now) return "已过期";
	__time64_t left = expireTime - now;
	int min = (int)(left / 60);
	int sec = (int)(left % 60);
	char buf[64];
	sprintf(buf, "%d分%d秒", min, sec);
	return string(buf);
}

// ---- 显示当前道具状态 ----
void showShopStatus() {
	__time64_t now = _time64(nullptr);
	
	cout << "========================================\n";
	cout << "  当前用户：" << curUser.username << "\n";
	
	// 开发者模式显示
	if (isDeveloper) {
		cout << "  当前积分：∞ (无限) \n";
		cout << "  积分倍率：2.0x \n";
		cout << "  [开发者模式]：已激活 \n";
		cout << "  [永久VIP]：已激活 \n";
		cout << "  [翻倍卡]：永久生效 \n";
		cout << "  [VIP加速]：永久生效 \n";
	} else {
		cout << "  当前积分：" << curUser.score << "\n";
		cout << "  积分倍率：" << curUser.scoreRate << "x\n";
		
		// 永久VIP
		if (curUser.hasPermanentVIP) {
			cout << "  [永久VIP]：已激活 \n";
		} else if (now < curUser.vipEndTime) {
			cout << "  [1天VIP]：剩余 " << formatTimeLeft(curUser.vipEndTime) <<'\n';
		} else {
			cout << "  [VIP]：未激活 \n";
		}
		
		// 翻倍卡
		if (now < curUser.boostEndTime) {
			cout << "  [翻倍卡]：剩余 " << formatTimeLeft(curUser.boostEndTime) << "\n";
		} else {
			cout << "  [翻倍卡]：未激活 \n";
		}
		
		// 加速卡
		if (now < curUser.fastPassEndTime) {
			cout << "  [VIP加速]：剩余 " << formatTimeLeft(curUser.fastPassEndTime) << "\n";
		} else {
			cout << "  [VIP加速]：未激活 \n";
		}
	}
	
	cout << "========================================\n\n";
}

// ---- 积分不足提示 ----
void notEnoughScore(int need) {
	char msg[128];
	sprintf(msg, "积分不足！需要 %d 积分，当前 %d", need, curUser.score);
	MessageBoxA(NULL, msg, "提示", MB_OK | MB_ICONHAND);
}

// ---- 购买确认对话框 ----
bool confirmBuy(const char* itemName, int cost) {
	char msg[256];
	sprintf(msg, "确定要购买 [%s] 吗？\n消耗：%d 积分\n当前积分：%d",
		itemName, cost, curUser.score);
	int ret = MessageBoxA(NULL, msg, "确认购买", MB_YESNO | MB_ICONQUESTION);
	return (ret == IDYES);
}

// ---- 购买成功提示 ----
void buySuccess(const char* itemName) {
	char msg[128];
	sprintf(msg, "[%s] 购买成功！", itemName);
	MessageBoxA(NULL, msg, "提示", MB_OK | MB_ICONASTERISK);
}

// ---- 1. 积分翻倍卡（300积分 / 10分钟）----
void buyBoostCard() {
	const int COST = 300;
	if (curUser.score < COST) { notEnoughScore(COST); return; }
	
	__time64_t now = _time64(nullptr);
	// 如果还在有效期内，叠加时间；否则重新计时
	__time64_t baseTime = (now < curUser.boostEndTime) ? curUser.boostEndTime : now;
	curUser.boostEndTime = baseTime + 600; // 10分钟
	
	curUser.score -= COST;
	saveUser(curUser);
	buySuccess("积分翻倍卡");
	cout << "积分翻倍卡已激活！10分钟内积分×2\n";
	Sleep(1000);
}

// ---- 2. VIP加速卡（100积分 / 10分钟）----
void buyFastPass() {
	const int COST = 100;
	if (curUser.score < COST) { notEnoughScore(COST); return; }
	
	__time64_t now = _time64(nullptr);
	__time64_t baseTime = (now < curUser.fastPassEndTime) ? curUser.fastPassEndTime : now;
	curUser.fastPassEndTime = baseTime + 600;
	
	curUser.score -= COST;
	kfz = 1;
	saveUser(curUser);
	buySuccess("VIP加速卡");
	cout << "VIP加速卡已激活！10分钟加载动画加速\n";
	Sleep(1000);
}

// ---- 3. 1天VIP权限卡（5000积分 / 24小时）----
void buyDayVIP() {
	const int COST = 5000;
	if (curUser.score < COST) { notEnoughScore(COST); return; }
	
	__time64_t now = _time64(nullptr);
	__time64_t baseTime = (now < curUser.vipEndTime && !curUser.hasPermanentVIP)
	? curUser.vipEndTime : now;
	curUser.vipEndTime = baseTime + 86400; // 24小时
	curUser.scoreRate = 1.5;
	
	curUser.score -= COST;
	saveUser(curUser);
	buySuccess("1天VIP权限卡");
	cout << "1天VIP已激活！24小时积分1.5倍 + 可修改小贴士\n";
	Sleep(1000);
}

// ---- 4. 永久VIP（已移除密码激活，仅开发者模式拥有）----
void buyPermVIP() {
	if (isDeveloper) {
		MessageBoxA(NULL, "开发者模式已拥有所有特权！", "提示", MB_OK | MB_ICONASTERISK);
		return;
	}
	// 引导用户使用开发者模式
	cout << "\n==== 永久VIP ====\n";
	cout << "永久VIP已停止密码激活方式\n";
	cout << "请使用开发者模式获取全部特权\n";
	cout << "在菜单输入 0 可进入开发者验证\n";
	Sleep(1500);
}

//===========================================================================
// 开发者模式（密码：保密）
// 验证方式：先 Base64 编码，再 SHA-256 哈希，与预存值比对
//===========================================================================

// 预存的双加密哈希值（Python 生成）：
//   Base64("保密") = "xGdtMTExMTFp"
//   SHA-256("xGdtMTExMTFp") = "a6a5f9e8ee55f372aadaeff9c8of8fca8f8aaa1bl477171f4831f31ef6501692"
const char* DEV_DOUBLE_HASH = "a6a5f9e8ee55f372aadaeff9c80f8fca8f8aaa1b1477171f4831f31ef6501692";

//===========================================================================
// SHA-256 纯 C++ 实现（无外部依赖，跨平台可编译）
// 输出：64字符十六进制字符串
//===========================================================================
static const uint32_t SHA_K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t ROTR(uint32_t x, uint32_t n) {
	return (x >> n) | (x << (32 - n));
}
static inline uint32_t SHA_CH(uint32_t x, uint32_t y, uint32_t z) {
	return (x & y) ^ (~x & z);
}
static inline uint32_t SHA_MAJ(uint32_t x, uint32_t y, uint32_t z) {
	return (x & y) ^ (x & z) ^ (y & z);
}
static inline uint32_t EP0(uint32_t x) {
	return ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22);
}
static inline uint32_t EP1(uint32_t x) {
	return ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25);
}
static inline uint32_t SIG0(uint32_t x) {
	return ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3);
}
static inline uint32_t SIG1(uint32_t x) {
	return ROTR(x, 17) ^ ROTR(x, 19) ^ (x >> 10);
}

string sha256(const string& input) {
	// 1. Padding
	uint64_t bitLen = (uint64_t)input.size() * 8;
	string msg = input;
	msg.push_back(0x80);
	while ((msg.size() % 64) != 56) {
		msg.push_back(0x00);
	}
	for (int i = 7; i >= 0; i--) {
		msg.push_back((char)((bitLen >> (i * 8)) & 0xFF));
	}
	
	// 2. Init hash values
	uint32_t H[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};
	
	// 3. Process chunks
	for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
		uint32_t w[64];
		for (int i = 0; i < 16; i++) {
			w[i] = ((uint32_t)(uint8_t)msg[chunk + i * 4] << 24) |
			((uint32_t)(uint8_t)msg[chunk + i * 4 + 1] << 16) |
			((uint32_t)(uint8_t)msg[chunk + i * 4 + 2] << 8)  |
			((uint32_t)(uint8_t)msg[chunk + i * 4 + 3]);
		}
		for (int i = 16; i < 64; i++) {
			w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
		}
		
		uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
		uint32_t e = H[4], f = H[5], g = H[6], h = H[7];
		
		for (int i = 0; i < 64; i++) {
			uint32_t t1 = h + EP1(e) + SHA_CH(e, f, g) + SHA_K[i] + w[i];
			uint32_t t2 = EP0(a) + SHA_MAJ(a, b, c);
			h = g; g = f; f = e; e = d + t1;
			d = c; c = b; b = a; a = t1 + t2;
		}
		
		H[0] += a; H[1] += b; H[2] += c; H[3] += d;
		H[4] += e; H[5] += f; H[6] += g; H[7] += h;
	}
	
	// 4. Output hex
	char hex[65];
	for (int i = 0; i < 8; i++) {
		sprintf(hex + i * 8, "%08x", H[i]);
	}
	return string(hex, 64);
}

// Base64 编码（纯 C++ 实现）
const char* B64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

string base64Encode(const string& input) {
	string output;
	int i = 0;
	int padding = 0;
	while (i < (int)input.size()) {
		int b1 = input[i++];
		int b2 = (i < (int)input.size()) ? input[i++] : 0;
		int b3 = (i < (int)input.size()) ? input[i++] : 0;
		if (i > (int)input.size()) padding = (int)input.size() - (i - 3);
		
		int triple = (b1 << 16) | (b2 << 8) | b3;
		
		output += B64_CHARS[(triple >> 18) & 63];
		output += B64_CHARS[(triple >> 12) & 63];
		output += (padding >= 1) ? '=' : B64_CHARS[(triple >> 6) & 63];
		output += (padding >= 2) ? '=' : B64_CHARS[triple & 63];
	}
	return output;
}

// 验证开发者密码（双加密：先 Base64 再 SHA-256）
bool verifyDevPassword(const char* input) {
	string pwd = string(input);
	string b64 = base64Encode(pwd);
	string hash = sha256(b64);
	return (hash == DEV_DOUBLE_HASH);
}

// 激活开发者模式
void activateDevMode() {
	cout << "==== 开发者模式 ====\n";
	cout << "请输入开发者密码：";
	string pwd;
	cin >> pwd;
	
	if (verifyDevPassword(pwd.c_str())) {
		isDeveloper = true;
		// 开发者特权：积分设为无限（用一个特殊标记）
		curUser.score = 999999999;
		curUser.scoreRate = 2.0;       // 2倍积分
		curUser.hasPermanentVIP = true;
		curUser.vipEndTime = LLONG_MAX;
		curUser.boostEndTime = LLONG_MAX;
		curUser.fastPassEndTime = LLONG_MAX;
		kfz = 1;
		saveUser(curUser);
		MessageBoxA(NULL, "开发者模式已激活！\n积分无限 + 所有VIP特权已开启", "开发者模式", MB_OK | MB_ICONASTERISK);
		cout << "\n 开发者模式激活成功！\n";
		cout << "  · 积分无限\n";
		cout << "  · 积分倍率 ×2.0\n";
		cout << "  · 所有VIP特权永久开启\n";
		Sleep(2000);
	} else {
		MessageBoxA(NULL, "密码错误！", "开发者模式", MB_OK | MB_ICONHAND);
	}
}

// 检查是否开发者模式
bool isDevMode() {
	return isDeveloper;
}

// ---- 5. 出售积分（100积分 → 1个经验值，纯娱乐功能）----
void sellScore() {
	cout << "请输入要出售的积分数量（100积分起，100的倍数）：";
	int amount;
	cin >> amount;
	
	if (amount < 100 || amount % 100 != 0) {
		MessageBoxA(NULL, "数量必须是100的倍数！", "提示", MB_OK | MB_ICONHAND);
		return;
	}
	if (curUser.score < amount) {
		notEnoughScore(amount);
		return;
	}
	
	int exp = amount / 100;
	char msg[256];
	sprintf(msg, "确定出售 %d 积分（获得 %d 经验值）？\n当前积分：%d",
		amount, exp, curUser.score);
	int ret = MessageBoxA(NULL, msg, "确认出售", MB_YESNO | MB_ICONQUESTION);
	if (ret == IDYES) {
		curUser.score -= amount;
		saveUser(curUser);
		sprintf(msg, "出售成功！获得 %d 经验值 ", exp);
		MessageBoxA(NULL, msg, "提示", MB_OK | MB_ICONASTERISK);
	}
}

// ---- 6. 商店说明 ----
void shopHelp() {
	system("cls");
	cout << "========================================\n";
	cout << "         商 店 说 明\n";
	cout << "========================================\n";
	cout << "1. 积分翻倍卡（300积分）\n";
	cout << "   → 10分钟内所有积分×2\n";
	cout << "   → 可叠加购买，时间累加\n\n";
	cout << "2. VIP加速卡（100积分）\n";
	cout << "   → 10分钟加载动画加速\n";
	cout << "   → 可叠加购买，时间累加\n\n";
	cout << "3. 1天VIP权限卡（5000积分）\n";
	cout << "   → 24小时内积分1.5倍\n";
	cout << "   → 可修改小贴士\n";
	cout << "   → 加载动画加速\n\n";
	cout << "4. 永久VIP（开发者模式）\n";
	cout << "   → 积分无限 + 所有VIP特权\n";
	cout << "   → 通过菜单0输入密码激活\n";
	cout << "   → 密码采用SHA-256+Base64双加密\n\n";
	cout << "5. 出售积分\n";
	cout << "   → 100积分 = 1经验值\n";
	cout << "   → 纯娱乐功能\n\n";
	cout << "========================================\n";
	cout << "按任意键返回...";
	_getch();
}

// ---- 商店主入口（完整版）----
void shop() {
	checkExpire();
	
	while (true) {
		system("cls");
		showShopStatus();
		
		cout << "==== 商店菜单 ====\n";
		cout << "  1. 积分翻倍卡（300积分 / 10分钟）\n";
		cout << "  2. VIP加速卡（100积分 / 10分钟）\n";
		cout << "  3. 1天VIP权限卡（5000积分 / 24小时）\n";
		cout << "  4. 永久VIP（密码激活）\n";
		cout << "  5. 出售积分（100积分=1经验值）\n";
		cout << "  6. 商店说明\n";
		cout << "  0. 返回主菜单\n";
		cout << "========================================\n";
		cout << "请选择：";
		
		int choice;
		cin >> choice;
		
		switch (choice) {
			case 1: buyBoostCard(); break;
			case 2: buyFastPass(); break;
			case 3: buyDayVIP(); break;
			case 4: buyPermVIP(); break;
			case 5: sellScore(); break;
			case 6: shopHelp(); break;
			case 0: return;
		default:
			MessageBoxA(NULL, "输入无效", "提示", MB_OK);
			break;
		}
		
		// 每次操作后刷新
		if (choice != 0 && choice != 6) {
			cout << "\n按任意键继续...";
			_getch();
		}
	}
}

//===========================================================================
// 积分系统
//===========================================================================
bool buyItem(int cost) {
	checkExpire();
	// 开发者模式：积分无限，不扣积分
	if (isDeveloper) {
		return true;
	}
	if (curUser.score < cost) {
		MessageBoxA(NULL, "积分不足", "提示", MB_OK | MB_ICONHAND);
		return false;
	}
	curUser.score -= cost;
	saveUser(curUser);
	return true;
}

//
// winStatus:
//   1 = 胜利（获得全额积分）
//   0 = 平局（获得保底积分）
//  -1 = 失败（获得少量安慰积分）
//
void settleScore(int aiRawScore, int winStatus) {
	checkExpire();
	
	// 开发者模式：积分无限，不结算
	if (isDeveloper) {
		cout << "[开发者模式] 积分无限，本局不结算\n";
		Sleep(800);
		return;
	}
	
	double baseScore = aiRawScore / 10.0;
	
	// ---- 胜者 / 败者 / 平局 不同倍率 ----
	double winMultiplier = 1.0;
	if (winStatus == 1) {
		winMultiplier = 1.0;       // 胜利：全额
	} else if (winStatus == 0) {
		winMultiplier = 0.3;       // 平局：30%
	} else if (winStatus == -1) {
		winMultiplier = 0.1;       // 失败：10% 安慰分
	}
	
	double rate = curUser.scoreRate * winMultiplier;
	
	// 翻倍卡
	__time64_t now = _time64(nullptr);
	if (now < curUser.boostEndTime) {
		rate *= 2.0;
	}
	
	int finalScore = (int)(baseScore * rate);
	if (finalScore < 0) finalScore = 0;
	
	curUser.score += finalScore;
	saveUser(curUser);
	
	// 提示
	if (winStatus == 1) cout << "[胜利] ";
	else if (winStatus == 0) cout << "[平局] ";
	else cout << "[失败] ";
	
	cout << "本局获得积分：" << finalScore << endl;
	cout << "当前总积分：" << curUser.score << endl;
	Sleep(1500);
}

//===========================================================================
// AI 评分系统
//===========================================================================

// 获取某一方向上连续棋子数量 + 两端空位
void countLine(char board[Size][Size],
	int x, int y,
	int dx, int dy,
	char player,
	int& count,
	int& openEnds) {
		count = 1;
		openEnds = 0;
		
		// 正向
		int nx = x + dx;
		int ny = y + dy;
		while (nx >= 0 && nx < Size && ny >= 0 && ny < Size && board[nx][ny] == player) {
			count++;
			nx += dx;
			ny += dy;
		}
		if (nx >= 0 && nx < Size && ny >= 0 && ny < Size && board[nx][ny] == ' ')
			openEnds++;
		
		// 反向
		nx = x - dx;
		ny = y - dy;
		while (nx >= 0 && nx < Size && ny >= 0 && ny < Size && board[nx][ny] == player) {
			count++;
			nx -= dx;
			ny -= dy;
		}
		if (nx >= 0 && nx < Size && ny >= 0 && ny < Size && board[nx][ny] == ' ')
			openEnds++;
	}

// AI 单点评分（0~100000）
int evaluatePoint(char board[Size][Size], int x, int y, char player) {
	if (board[x][y] != ' ')
		return 0;
	
	int score = 0;
	int dx[] = {1, 0, 1, 1};
	int dy[] = {0, 1, 1, -1};
	
	for (int d = 0; d < 4; d++) {
		int cnt = 0, open = 0;
		countLine(board, x, y, dx[d], dy[d], player, cnt, open);
		
		if (cnt >= 5) score += 100000;          // 连五
		else if (cnt == 4) {
			if (open == 2) score += 10000;       // 活四
			else if (open == 1) score += 5000;    // 冲四
		} else if (cnt == 3) {
			if (open == 2) score += 2000;         // 活三
			else if (open == 1) score += 500;     // 眠三
		} else if (cnt == 2) {
			if (open == 2) score += 100;          // 活二
			else if (open == 1) score += 30;      // 眠二
		} else if (cnt == 1 && open > 0) {
			score += 5;
		}
	}
	return score;
}

// AI 综合评分（进攻 + 防守）
int aiEvaluate(char board[Size][Size], int x, int y, char aiPlayer) {
	char humanPlayer = (aiPlayer == 'O') ? 'X' : 'O';
	return evaluatePoint(board, x, y, aiPlayer)
	+ evaluatePoint(board, x, y, humanPlayer) * 2;
}

// 游戏总评分（0~100）
int getGameScore(char board[Size][Size], char player) {
	int maxScore = 0;
	for (int i = 0; i < Size; i++) {
		for (int j = 0; j < Size; j++) {
			maxScore = max(maxScore, evaluatePoint(board, i, j, player));
		}
	}
	return min(100, maxScore / 100);
}

//===========================================================================
// 网络工具函数
//===========================================================================
int packData(char* buf, const char* raw, int rawLen) {
	buf[0] = rawLen;
	memcpy(buf + 1, raw, rawLen);
	buf[rawLen + 1] = simpleHash(raw, rawLen);
	return rawLen + 2;
}

int unpackData(char* buf, char* out) {
	int len = buf[0];
	if (len <= 0 || len > BUF_LEN - 2) return -1;
	unsigned char h = simpleHash(buf + 1, len);
	if (h != buf[len + 1]) return -1;
	memcpy(out, buf + 1, len);
	return len;
}

void closeNetSocket() {
	if (curSock != INVALID_SOCKET) closesocket(curSock);
	if (serverSock != INVALID_SOCKET) closesocket(serverSock);
	if (clientSock != INVALID_SOCKET) closesocket(clientSock);
	WSACleanup();
	netInitOk = false;
}

//===========================================================================
// 网页跳转
//===========================================================================
string URL[] = {
	"",
	"https://www.acgo.cn/discuss/rest/88744",
	"https://www.luogu.com.cn/article/jguyqy95/",
	"https://www.acgo.cn/discuss/rest/89213",
	"https://www.acgo.cn/discuss/rest/89211"
};

string ts[] = {
	"",
	"你知道吗?这款娱乐软件源于<bits/stdc++.h>(ACGO OJ) 1~2年前随手编写的游戏代码:棋类游戏大全",
	"你知道吗?这款娱乐软件在1~2年前就已经诞生,于 2026/08/06 日重启项目",
	"你知道吗?<bits/stdc++.h>和xgy111111的CSDN号分别叫这代码保熟吗和小西瓜学编程",
	"你去原文章点赞了吗?如果点了可以找作者要开发者密码呦！",
	"感谢游玩,祝您游玩愉快!"
};

void get_URL(bool x, int a) {
	if (x == false) {
		URL[1] = " ";
		URL[2] = " ";
	} else if (a == 0) {
		URL[1] = "https://www.acgo.cn/discuss/rest/88744";
		URL[2] = "https://www.luogu.com.cn/article/jguyqy95/";
	}
}

void ask(char com[256]) {
	string msg = "正在跳转至";
	msg += com;
	msg += ",是否打开该网页？";
	int ret1 = MessageBoxA(0, msg.c_str(), "确认", MB_YESNO | MB_ICONASTERISK);
	if (ret1 == IDYES) {
		ShellExecuteA(0, "open", com, NULL, NULL, SW_SHOW);
	}
}

//===========================================================================
// 加载动画
//===========================================================================
int jz(int i) {
	get_URL(false, 0);
	fff = 0;
	closeNetSocket();
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
		netInitOk = true;
		serverSock = socket(AF_INET, SOCK_STREAM, 0);
		clientSock = socket(AF_INET, SOCK_STREAM, 0);
		curSock = INVALID_SOCKET;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		ZeroMemory(&clientAddr, sizeof(clientAddr));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(PORT);
		serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	}
	nx = 6; ny = 6;
	x_ = 0; y_ = 0;
	oc = '.';
	color = 1;
	ff = false;
	for (int p = 0; p < N; p++) {
		for (int q = 0; q < N; q++) {
			if (p == 6 && q == 6) table[p][q] = '+';
			else if (!p || !q || p == N - 1 || q == N - 1) table[p][q] = '#';
			else table[p][q] = '.';
		}
	}
	char aiBoardPre[Size][Size];
	char tttSinglePre[3][3], tttDoublePre[3][3];
	for (int p = 0; p < Size; p++)
		for (int q = 0; q < Size; q++) aiBoardPre[p][q] = ' ';
	for (int p = 0; p < 3; p++)
		for (int q = 0; q < 3; q++) tttSinglePre[p][q] = tttDoublePre[p][q] = ' ';
	static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	system("cls");
	
	if (i == 1) {
		int a = rand() % 5 + 1;
		for (int k = 0; k < 3; k++) {
			cout << "\n正在返回主界面...\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n" << ts[a] << "\n\n\n加载中";
			Sleep(200);
			for (int t = 1; t <= 3; t++) { Sleep(150); cout << '.'; }
			system("cls");
		}
	} else if (i == 2 && kfz == 1) {
		int a = rand() % 5 + 1;
		for (int k = 0; k < 3; k++) {
			cout << "\n正在预加载资源...\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n" << ts[a] << "\n\n\n加载中";
			Sleep(40);
			for (int t = 1; t <= 3; t++) { Sleep(40); cout << '.'; }
			system("cls");
		}
	} else if (i == 2) {
		int a = rand() % 5 + 1;
		for (int k = 0; k < 3; k++) {
			cout << "\n正在预加载资源...\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n" << ts[a] << "\n\n\n加载中";
			Sleep(200);
			for (int t = 1; t <= 3; t++) { Sleep(200); cout << '.'; }
			system("cls");
		}
	}
	fff = 1;
	get_URL(true, 0);
	get_URL(true, 1);
	return 1;
}

//===========================================================================
// 五子棋棋盘打印 & 胜负判断
//===========================================================================
void printBoard(char board[Size][Size]) {
	cout << "    ";
	for (int j = 1; j <= Size; j++) cout << setw(2) << j;
	cout << endl;
	cout << "   ";
	for (int j = 0; j < Size; j++) cout << "# ";
	cout << endl;
	for (int i = 0; i < Size; i++) {
		cout << setw(2) << (i + 1) << " ";
		cout << "# ";
		for (int j = 0; j < Size; j++) {
			if (board[i][j] == ' ') cout << ". ";
			else cout << board[i][j] << " ";
		}
		cout << "#" << endl;
	}
	cout << "   ";
	for (int j = 0; j < Size; j++) cout << "# ";
	cout << endl;
}

bool checkWin(char board[Size][Size], int x, int y, char player) {
	if (x < 0 || x >= Size || y < 0 || y >= Size) return false;
	if (board[x][y] != player) return false;
	int dx[] = {0, 1, 1, 1};
	int dy[] = {1, 0, 1, -1};
	for (int dir = 0; dir < 4; dir++) {
		int count = 1;
		for (int k = 1; k <= 4; k++) {
			int nx = x + dx[dir] * k;
			int ny = y + dy[dir] * k;
			if (nx >= 0 && nx < Size && ny >= 0 && ny < Size && board[nx][ny] == player) count++;
			else break;
		}
		for (int k = 1; k <= 4; k++) {
			int nx = x - dx[dir] * k;
			int ny = y - dy[dir] * k;
			if (nx >= 0 && nx < Size && ny >= 0 && ny < Size && board[nx][ny] == player) count++;
			else break;
		}
		if (count >= 5) return true;
	}
	return false;
}

bool isBoardFull(char board[Size][Size]) {
	int cnt = 0;
	for (int i = 0; i < Size; i++)
		for (int j = 0; j < Size; j++)
			if (board[i][j] == ' ') cnt++;
	return cnt == 0;
}

//===========================================================================
// AI 五子棋 - AI 落子
//===========================================================================
void aiPlay(char board[Size][Size], int& aiX, int& aiY) {
	aiX = 7;
	aiY = 7;
	int bestScore = -1;
	
	for (int i = 0; i < Size; i++) {
		for (int j = 0; j < Size; j++) {
			if (board[i][j] != ' ') continue;
			
			int total = aiEvaluate(board, i, j, 'O');
			
			// 中心区域偏好
			if (i >= 6 && i <= 8 && j >= 6 && j <= 8) total += 100;
			// 边缘惩罚
			if (i == 0 || i == Size - 1 || j == 0 || j == Size - 1) total -= 20;
			if ((i == 0 || i == Size - 1) && (j == 0 || j == Size - 1)) total -= 50;
			
			if (total > bestScore) {
				bestScore = total;
				aiX = i;
				aiY = j;
			}
		}
	}
	
	// 兜底
	if (board[aiX][aiY] != ' ') {
		for (int a = 0; a < Size; a++)
			for (int b = 0; b < Size; b++)
				if (board[a][b] == ' ') { aiX = a; aiY = b; goto ai_end; }
	}
	ai_end:
	board[aiX][aiY] = 'O';
}

//===========================================================================
// 1. 单人 AI 五子棋（含积分结算）
//===========================================================================
void wzq() {
	char board[Size][Size];
	for (int i = 0; i < Size; i++)
		for (int j = 0; j < Size; j++) board[i][j] = ' ';
	
	string inputx, inputy;
	int x, y, aiX, aiY;
	int inputX, inputY;
	bool gameOver = false;
	int winStatus = 0; // 0=平局 1=玩家胜 -1=AI胜
	
	cout << "===== 单人五子棋 =====\n";
	cout << "输入行 列落子，范围1~15，例如：8 8\n";
	cout << "玩家：X   AI：O\n\n";
	
	while (!gameOver) {
		printBoard(board);
		if (isBoardFull(board)) {
			cout << "\n棋盘已满！平局！\n";
			winStatus = 0;
			gameOver = true;
			break;
		}
		
		cout << "\n【玩家X】请输入行和列：";
		cin >> inputx >> inputy;
		
		if (inputx.size() == 1 || inputx.size() == 2) {
			if (inputx.size() == 1) inputX = inputx[0] - '0';
			else inputX = (inputx[0] - '0') * 10 + (inputx[1] - '0');
		} else { cout << "坐标超出范围1~15！\n"; continue; }
		
		if (inputy.size() == 1 || inputy.size() == 2) {
			if (inputy.size() == 1) inputY = inputy[0] - '0';
			else inputY = (inputy[0] - '0') * 10 + (inputy[1] - '0');
		} else { cout << "坐标超出范围1~15！\n"; continue; }
		
		if (inputX < 1 || inputX > 15 || inputY < 1 || inputY > 15) {
			cout << "坐标超出范围1~15！\n"; continue;
		}
		x = inputX - 1; y = inputY - 1;
		if (board[x][y] != ' ') {
			cout << "该位置已经有棋子！\n"; continue;
		}
		
		board[x][y] = 'X';
		if (checkWin(board, x, y, 'X')) {
			printBoard(board);
			cout << "\n恭喜[玩家X]获胜！\n";
			winStatus = 1;
			gameOver = true;
			break;
		}
		if (isBoardFull(board)) {
			printBoard(board);
			cout << "\n平局！\n";
			winStatus = 0;
			gameOver = true;
			break;
		}
		
		aiPlay(board, aiX, aiY);
		cout << "\nAI落子：" << (aiX + 1) << " " << (aiY + 1) << endl;
		if (checkWin(board, aiX, aiY, 'O')) {
			printBoard(board);
			cout << "\nAI获胜！\n";
			winStatus = -1;
			gameOver = true;
			break;
		}
		system("pause");
		system("CLS");
	}
	
	// ✅ 积分结算
	int rawScore = getGameScore(board, 'X');
	settleScore(rawScore, winStatus);
	
	system("pause");
	system("CLS");
	jz();
}

//===========================================================================
// 2. 双人本地五子棋（含积分结算）
//===========================================================================
char f(int x, int y) {
	char co = table[x][y];
	if (co == '.') { ff = 0; return '.'; }
	int cnt = 0;
	for (int i = y; i <= y + 4 && i < N; i++) if (table[x][i] == co) cnt++;
	if (cnt == 5) { ff = 1; return co; }
	cnt = 0;
	for (int i = x; i <= x + 4 && i < N; i++) if (table[i][y] == co) cnt++;
	if (cnt == 5) { ff = 1; return co; }
	cnt = 0;
	for (int i = 0; i <= 4; i++) { int nx = x + i, ny = y + i; if (nx < N && ny < N && table[nx][ny] == co) cnt++; else break; }
	if (cnt == 5) { ff = 1; return co; }
	cnt = 0;
	for (int i = 0; i <= 4; i++) { int nx = x - i, ny = y + i; if (nx >= 0 && ny < N && table[nx][ny] == co) cnt++; else break; }
	if (cnt == 5) { ff = 1; return co; }
	ff = 0; return co;
}

bool dw() {
	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++) {
			char dd = f(i, j);
			if (ff) {
				if (dd == 'O') {
					cout << "\nPlayer1:" << name1 << "赢了\n";
					int raw = getGameScore(table, 'O');
					settleScore(raw, 1);
				} else {
					cout << "\nPlayer2:" << name2 << "赢了\n";
					int raw = getGameScore(table, 'X');
					settleScore(raw, 1);
				}
				cout << "\n按下任意键继续......";
				_getch();
				system("cls");
				return false;
			}
		}
	return true;
}

void cgoto(short x, short y) {
	COORD gxy = {x, y};
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleCursorPosition(hOut, gxy);
}

void init() {
	color = 1; nx = ny = 6;
	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++) {
			if (i == nx && j == ny) table[i][j] = '+';
			else if (!i || !j || i == N - 1 || j == N - 1) table[i][j] = '#';
			else table[i][j] = '.';
		}
}

void print() {
	cgoto(0, 0);
	for (int i = 0; i < N; i++) {
		for (int j = 0; j < N; j++) cout << table[i][j] << " ";
		cout << endl;
	}
	cout << "#(" << nx << "," << ny << ")";
	if (controlMode == 1) cout << " 【模式1：方向键】";
	else cout << " 【模式2：WASD】";
	cout << "\nPlayer1:" << name1 << "，棋子形状为O" << "\nPlayer2:" << name2 << "，棋子形状为X" << "\n'+'为落子点，'!'为不可落子点，按Z切换控制模式";
}

void move(int px, int py) {
	table[nx][ny] = oc; oc = '.';
	nx += px; ny += py;
	if (nx < 1 || ny < 1 || nx > 13 || ny > 13) { nx -= px; ny -= py; }
	if (table[nx][ny] != '.') { oc = table[nx][ny]; table[nx][ny] = '!'; }
	else table[nx][ny] = '+';
}

void down() {
	if (table[nx][ny] != '+') return;
	if (color) { oc = 'O'; table[nx][ny] = 'O'; }
	else { table[nx][ny] = 'X'; oc = 'X'; }
	color = !color;
}

void Gobang() {
	cout << "# # # # # # # # # #\n#                   #\n#      五子棋           #\n#      小游戏           #\n# # # # # # # # # # #\n按下任意键继续......";
	_getch();
	system("cls");
	cout << "请输入Player1的名字，他的棋子形状是O" << endl; cin >> name1;
	cout << "请输入Player2的名字，他的棋子形状是X" << endl; cin >> name2;
	system("cls");
	
	bool end = 1;
	while (end) {
		init(); print(); oc = '.';
		while (dw()) {
			char fkey; fkey = _getch();
			bool isArrowKey = false;
			if (fkey == 0 || fkey == (char)0xE0) { isArrowKey = true; fkey = _getch(); }
			if (isArrowKey) {
				if (controlMode == 1) {
					if (fkey == 75) { move(0, -1); print(); }
					else if (fkey == 72) { move(-1, 0); print(); }
					else if (fkey == 77) { move(0, 1); print(); }
					else if (fkey == 80) { move(1, 0); print(); }
				}
			} else {
				if (fkey == 'z' || fkey == 'Z') { controlMode = (controlMode == 1) ? 2 : 1; print(); }
				else if (controlMode == 2) {
					if (fkey == 'a') { move(0, -1); print(); }
					else if (fkey == 's') { move(1, 0); print(); }
					else if (fkey == 'd') { move(0, 1); print(); }
					else if (fkey == 'w') { move(-1, 0); print(); }
				}
				if (fkey == ' ') { down(); print(); }
			}
		}
		while (1) {
			cout << "按下c键进行下一轮 e键结束\n";
			char input = _getch();
			if (input == 'c') { jz(); fff = 0; break; }
			else if (input == 'e') { end = 0; break; }
			else MessageBoxA(NULL, "输入错误", "提示", MB_OK | MB_ICONHAND);
		}
	}
}

//===========================================================================
// 3. 单人井字棋（含积分结算）
//===========================================================================
char board3[BOARD_SIZE][BOARD_SIZE];

void drawBoard3() {
	cout << "   1   2   3" << endl;
	cout << "1  " << board3[0][0] << " | " << board3[0][1] << " | " << board3[0][2] << endl;
	cout << "   --+---+--" << endl;
	cout << "2  " << board3[1][0] << " | " << board3[1][1] << " | " << board3[1][2] << endl;
	cout << "   --+---+--" << endl;
	cout << "3  " << board3[2][0] << " | " << board3[2][1] << " | " << board3[2][2] << endl << endl;
}

bool checkWin3(char player) {
	for (int i = 0; i < BOARD_SIZE; i++)
		if (board3[i][0] == player && board3[i][1] == player && board3[i][2] == player) return true;
	for (int j = 0; j < BOARD_SIZE; j++)
		if (board3[0][j] == player && board3[1][j] == player && board3[2][j] == player) return true;
	if (board3[0][0] == player && board3[1][1] == player && board3[2][2] == player) return true;
	if (board3[0][2] == player && board3[1][1] == player && board3[2][0] == player) return true;
	return false;
}

bool checkDraw3() {
	for (int i = 0; i < BOARD_SIZE; i++)
		for (int j = 0; j < BOARD_SIZE; j++)
			if (board3[i][j] == ' ') return false;
	return true;
}

void playerMove3() {
	int row, col;
	while (true) {
		cout << "请输入落子位置（行 列）：";
		string Row, Col;
		cin >> Row >> Col;
		if (Row.size() == 1) row = Row[0] - '0';
		else { cout << "坐标超出范围1~3！\n"; continue; }
		if (Col.size() == 1) col = Col[0] - '0';
		else { cout << "坐标超出范围1~3！\n"; continue; }
		row--; col--;
		if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE)
			cout << "坐标无效\n";
		else if (board3[row][col] != ' ')
			cout << "位置占用\n";
		else { board3[row][col] = 'X'; break; }
	}
}

// 井字棋 AI 评分
int evaluatePoint3(int row, int col, char player) {
	int score = 0;
	char opponent = (player == 'X') ? 'O' : 'X';
	
	board3[row][col] = player;
	if (checkWin3(player)) score += 1000;
	board3[row][col] = ' ';
	
	// 防守：阻止对手
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (board3[i][j] == ' ') {
				board3[i][j] = opponent;
				if (checkWin3(opponent)) score += 500;
				board3[i][j] = ' ';
			}
		}
	}
	
	if (row == 1 && col == 1) score += 30;
	if ((row == 0 && col == 0) || (row == 0 && col == 2) ||
		(row == 2 && col == 0) || (row == 2 && col == 2)) score += 10;
	
	return score;
}

void computerMove3() {
	cout << "电脑落子中..." << endl;
	int bestRow = 0, bestCol = 0, bestScore = -1;
	
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (board3[i][j] == ' ') {
				int s = evaluatePoint3(i, j, 'O');
				if (s > bestScore) {
					bestScore = s;
					bestRow = i;
					bestCol = j;
				}
			}
		}
	}
	
	if (bestScore == -1) {
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				if (board3[i][j] == ' ') { bestRow = i; bestCol = j; break; }
	}
	
	board3[bestRow][bestCol] = 'O';
}

void Tic_Tac_Toe() {
	int end = 1;
	while (1) {
		for (int i = 0; i < BOARD_SIZE; i++)
			for (int j = 0; j < BOARD_SIZE; j++) board3[i][j] = ' ';
		
		cout << "=== 井字棋单人 ===" << endl;
		cout << "玩家：X | 电脑：O" << endl << endl;
		drawBoard3();
		
		int winStatus = 0;
		bool over = false;
		
		while (!over) {
			playerMove3();
			if (checkWin3('X')) { drawBoard3(); cout << "你赢了\n"; winStatus = 1; over = true; break; }
			if (checkDraw3()) { drawBoard3(); cout << "平局\n"; winStatus = 0; over = true; break; }
			computerMove3();
			if (checkWin3('O')) { drawBoard3(); cout << "电脑获胜\n"; winStatus = -1; over = true; break; }
			if (checkDraw3()) { drawBoard3(); cout << "平局\n"; winStatus = 0; over = true; break; }
			system("cls");
			cout << "=== 井字棋单人 ===\n玩家X 电脑O\n\n";
			drawBoard3();
		}
		
		// ✅ 积分结算
		int rawScore = min(100, max(1, winStatus * 50 + 30));
		settleScore(rawScore, winStatus);
		
		if (over) {
			Sleep(2000);
			system("cls");
			while (1) {
				cout << "c下一轮 e退出\n";
				char input = _getch();
				if (input == 'c') { jz(); fff = 0; system("cls"); break; }
				else if (input == 'e') return;
				else MessageBoxA(NULL, "输入错误", "提示", MB_OK | MB_ICONHAND);
			}
		}
	}
}

//数独 获取尺寸、子宫行列
void GetModeInfo(string mode, int &size, int &boxR, int &boxC)
{
	if (mode == "1")
	{
		size = 4; boxR = 2; boxC = 2;
	}
	else if (mode == "2")
	{
		size = 6; boxR = 2; boxC = 3;
	}
	else // "3" 九宫
	{
		size = 9; boxR = 3; boxC = 3;
	}
}

// 判断位置填入数字是否合法
bool IsValid(vector<vector<int>> &mp, int r, int c, int num, int size, int boxR, int boxC)
{
	// 行
	for (int j = 0; j < size; j++)
		if (mp[r][j] == num) return false;
	// 列
	for (int i = 0; i < size; i++)
		if (mp[i][c] == num) return false;
	// 子宫
	int sr = r / boxR * boxR;
	int sc = c / boxC * boxC;
	for (int i = sr; i < sr + boxR; i++)
		for (int j = sc; j < sc + boxC; j++)
			if (mp[i][j] == num) return false;
	return true;
}

// 回溯生成完整终盘
bool FillFullMap(vector<vector<int>> &mp, int size, int boxR, int boxC)
{
	for (int r = 0; r < size; r++)
	{
		for (int c = 0; c < size; c++)
		{
			if (mp[r][c] == 0)
			{
				vector<int> nums;
				for (int i = 1; i <= size; i++) nums.push_back(i);
				random_shuffle(nums.begin(), nums.end());
				for (int num : nums)
				{
					if (IsValid(mp, r, c, num, size, boxR, boxC))
					{
						mp[r][c] = num;
						if (FillFullMap(mp, size, boxR, boxC))
							return true;
						mp[r][c] = 0;
					}
				}
				return false;
			}
		}
	}
	return true;
}

// 统计解数量，找到≥2个直接停止
int CountSolution(vector<vector<int>> mp, int size, int boxR, int boxC)
{
	int cnt = 0;
	function<bool()> dfs = [&]() -> bool
	{
		for (int r = 0; r < size; r++)
		{
			for (int c = 0; c < size; c++)
			{
				if (mp[r][c] == 0)
				{
					for (int num = 1; num <= size; num++)
					{
						if (IsValid(mp, r, c, num, size, boxR, boxC))
						{
							mp[r][c] = num;
							if (dfs()) return true;
							mp[r][c] = 0;
						}
					}
					return false;
				}
			}
		}
		cnt++;
		return cnt >= 2;
	};
	dfs();
	return cnt;
}

//====================出题函数(要求:void CreateSudoku(string x))====================
void CreateSudoku(string x)
{
	srand((unsigned)time(NULL));
	int size, boxR, boxC;
	GetModeInfo(x, size, boxR, boxC);
	
	label_regenerate:
	vector<vector<int>> mp(size, vector<int>(size, 0));
	FillFullMap(mp, size, boxR, boxC);
	
	vector<pair<int, int>> pos;
	for (int i = 0; i < size; i++)
		for (int j = 0; j < size; j++)
			pos.emplace_back(i, j);
	random_shuffle(pos.begin(), pos.end());
	
	int delCnt = size * size / 2;
	for (auto [r, c] : pos)
	{
		if (delCnt <= 0) break;
		int bak = mp[r][c];
		mp[r][c] = 0;
		if (CountSolution(mp, size, boxR, boxC) != 1)
		{
			mp[r][c] = bak;
		}
		else
		{
			delCnt--;
		}
	}
	
	// 校验唯一解，不满足重新生成
	if (CountSolution(mp, size, boxR, boxC) != 1)
		goto label_regenerate;
	
	// 存入全局Sudoku_board
	for (int i = 1; i <= size; i++)
	{
		string line;
		for (int j = 0; j < size; j++)
		{
			line += (char)('0' + mp[i - 1][j]);
		}
		Sudoku_board[i] = line;
	}
	for (int i = size + 1; i <= 9; i++) Sudoku_board[i] = "";
}

//====================棋盘格式化打印函数====================
void PrintSudoku(string x)
{
	int size, boxR, boxC;
	GetModeInfo(x, size, boxR, boxC);
	cout << endl;
	
	for (int r = 1; r <= size; r++)
	{
		if ((r - 1) % boxR == 0)
		{
			for (int j = 0; j < size; j++)
			{
				cout << "+---";
				if ((j + 1) % boxC == 0) cout << "+";
			}
			cout << endl;
		}
		string &line = Sudoku_board[r];
		for (int c = 0; c < size; c++)
		{
			char ch = line[c];
			if (ch == '0') ch = ' ';
			cout << "| " << ch << " ";
			if ((c + 1) % boxC == 0) cout << "|";
		}
		cout << endl;
	}
	for (int j = 0; j < size; j++)
	{
		cout << "+---";
		if ((j + 1) % boxC == 0) cout << "+";
	}
	cout << endl << endl;
}

//====================判赢函数====================
bool CheckWin(string x)
{
	int size, boxR, boxC;
	GetModeInfo(x, size, boxR, boxC);
	vector<vector<int>> mp(size, vector<int>(size));
	
	for (int i = 0; i < size; i++)
	{
		string &s = Sudoku_board[i + 1];
		for (int j = 0; j < size; j++)
		{
			mp[i][j] = s[j] - '0';
			if (mp[i][j] == 0) return false;
		}
	}
	
	for (int i = 0; i < size; i++)
		for (int j = 0; j < size; j++)
		{
			int num = mp[i][j];
			mp[i][j] = 0;
			if (!IsValid(mp, i, j, num, size, boxR, boxC))
			{
				mp[i][j] = num;
				return false;
			}
			mp[i][j] = num;
		}
	return true;
}

//====================自动求解函数(拷贝当前盘面并求解，不修改原题目)====================
bool SolveSudoku(vector<vector<int>> &mp, int size, int boxR, int boxC)
{
	for (int r = 0; r < size; r++)
	{
		for (int c = 0; c < size; c++)
		{
			if (mp[r][c] == 0)
			{
				for (int num = 1; num <= size; num++)
				{
					if (IsValid(mp, r, c, num, size, boxR, boxC))
					{
						mp[r][c] = num;
						if (SolveSudoku(mp, size, boxR, boxC))
							return true;
						mp[r][c] = 0;
					}
				}
				return false;
			}
		}
	}
	return true;
}

// 将求解结果输出打印（不改变玩家当前棋盘）
void ShowAnswer(string x)
{
	system("cls");
	int size, boxR, boxC;
	GetModeInfo(x, size, boxR, boxC);
	vector<vector<int>> mp(size, vector<int>(size));
	// 复制当前盘面
	for (int i = 0; i < size; i++)
	{
		string &s = Sudoku_board[i + 1];
		for (int j = 0; j < size; j++)
		{
			mp[i][j] = s[j] - '0';
		}
	}
	if (SolveSudoku(mp, size, boxR, boxC))
	{
		cout << "\n====标准答案====\n";
		// 临时输出答案界面
		for (int r = 0; r < size; r++)
		{
			if (r % boxR == 0)
			{
				for (int j = 0; j < size; j++)
				{
					cout << "+---";
					if ((j + 1) % boxC == 0) cout << "+";
				}
				cout << endl;
			}
			for (int c = 0; c < size; c++)
			{
				cout << "| " << mp[r][c] << " ";
				if ((c + 1) % boxC == 0) cout << "|";
			}
			cout << endl;
		}
		for (int j = 0; j < size; j++)
		{
			cout << "+---";
			if ((j + 1) % boxC == 0) cout << "+";
		}
		cout << endl;
	}
	else
	{
		cout << "无解！\n";
	}
}

// 数 独 //
void Sudoku(){
	string mode;
	while (true)
	{
		cout << "==========数独游戏==========\n";
		cout << "1:4宫 | 2:6宫 | 3:9宫 | 0:退出\n";
		cout << "请选择模式：";
		cin >> mode;
		if (mode == "0") break;
		if (mode != "1" && mode != "2" && mode != "3")
		{
			cout << "输入错误！\n";
			continue;
		}
		
		CreateSudoku(mode);
		int size;
		GetModeInfo(mode, size, size, size); //仅获取size
		cout << "\n====新题目生成完成！====\n";
		
		// 游戏主循环
		while (true)
		{
			PrintSudoku(mode);
			cout << "指令：\n";
			cout << "1.填入数字  2.查看答案  3.换一道新题  4.返回菜单\n";
			int cmd;
			cin >> cmd;
			if (cmd == 4) break;
			if (cmd == 3)
			{
				CreateSudoku(mode);
				continue;
			}
			if (cmd == 2)
			{
				ShowAnswer(mode);
				continue;
			}
			if (cmd == 1)
			{
				int r, c, num;
				cout << "输入行(1~" << size*size << ") 列(1~" << size*size << ") 数字(1~" << size*size << "),输入0 0 0放弃\n";
				cin >> r >> c >> num;
				if (r == 0 && c == 0 && num == 0) continue;
				if (r < 1 || r > size*size || c < 1 || c > size*size || num < 0 || num > 9)
				{
					cout << "输入范围错误！\n";
					continue;
				}
				// 填入操作
				Sudoku_board[r][c - 1] = (char)('0' + num);
				
				// 填完检查是否胜利
				if (CheckWin(mode))
				{
					PrintSudoku(mode);
					cout << "恭喜！你完成数独！\n";
					int jfjs=0;
					if(mode=="1")jfjs=1;
					else if(mode=="2")jfjs=3;
					else if(mode=="3")jfjs=5;
					cout << "获得积分:"<<2+jfjs*curUser.scoreRate;
					curUser.score+=2+jfjs*curUser.scoreRate;
					break;
				}
			}system("cls");
		}
	}
	cout << "游戏结束\n";
}

//===========================================================================
// 4. 双人井字棋（含积分结算）
//===========================================================================
char b[3][3];
char currentPlayer = 'X';

void drawB() {
	cout << "   1   2   3" << endl;
	cout << "1  " << b[0][0] << " | " << b[0][1] << " | " << b[0][2] << endl;
	cout << "   --+---+--" << endl;
	cout << "2  " << b[1][0] << " | " << b[1][1] << " | " << b[1][2] << endl;
	cout << "   --+---+--" << endl;
	cout << "3  " << b[2][0] << " | " << b[2][1] << " | " << b[2][2] << endl << endl;
}

bool Win(char player) {
	for (int i = 0; i < 3; i++)
		if (b[i][0] == player && b[i][1] == player && b[i][2] == player) return true;
	for (int j = 0; j < 3; j++)
		if (b[0][j] == player && b[1][j] == player && b[2][j] == player) return true;
	if (b[0][0] == player && b[1][1] == player && b[2][2] == player) return true;
	if (b[0][2] == player && b[1][1] == player && b[2][0] == player) return true;
	return false;
}

bool cD() {
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			if (b[i][j] == ' ') return false;
	return true;
}

void playerM() {
	int row, col;
	while (true) {
		if (currentPlayer == 'X') cout << "玩家1落子(行 列)：";
		else cout << "玩家2落子(行 列)：";
		string Row, Col;
		cin >> Row >> Col;
		if (Row.size() == 1) row = Row[0] - '0';
		else { cout << "坐标超出范围1~3！\n"; continue; }
		if (Col.size() == 1) col = Col[0] - '0';
		else { cout << "坐标超出范围1~3！\n"; continue; }
		row--; col--;
		if (row < 0 || row >= 3 || col < 0 || col >= 3) cout << "无效\n";
		else if (b[row][col] != ' ') cout << "占用\n";
		else { b[row][col] = currentPlayer; break; }
	}
	currentPlayer = (currentPlayer == 'O') ? 'X' : 'O';
}

void Tic_Tac_Toe_1() {
	int end = 1;
	while (1) {
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) b[i][j] = ' ';
		currentPlayer = 'X';
		end = 1;
		
		int winStatus = 0;
		char winner = ' ';
		bool over = false;
		
		while (!over) {
			system("cls");
			cout << "双人井字棋 X玩家1 O玩家2\n\n";
			drawB();
			playerM();
			if (Win('X')) { winner = 'X'; winStatus = 1; over = true; break; }
			if (cD()) { winStatus = 0; over = true; break; }
			
			system("cls");
			cout << "双人井字棋 X玩家1 O玩家2\n\n";
			drawB();
			playerM();
			if (Win('O')) { winner = 'O'; winStatus = 1; over = true; break; }
			if (cD()) { winStatus = 0; over = true; break; }
		}
		
		drawB();
		if (winner == 'X') { cout << "玩家1胜利\n"; }
		else if (winner == 'O') { cout << "玩家2胜利\n"; }
		else { cout << "平局\n"; }
		
		// ✅ 积分结算
		if (winner == 'X') { settleScore(50, 1); }
		else if (winner == 'O') { settleScore(50, 1); }
		else { settleScore(10, 0); }
		
		Sleep(2000);
		system("cls");
		while (1) {
			cout << "c下一轮 e退出\n";
			char input = _getch();
			if (input == 'c') { jz(); fff = 0; system("cls"); break; }
			else if (input == 'e') return;
			else MessageBoxA(NULL, "输入错误", "提示", MB_OK | MB_ICONHAND);
		}
	}
}

//===========================================================================
// 5. 联机五子棋
//===========================================================================
void NetGobang() {
	if (!netInitOk) { cout << "网络初始化失败！回车返回\n"; _getch(); jz(); return; }
	int mode;
	char ipBuf[20];
	cout << "==== 局域网五子棋 ====\n1 创建房间(主机)\n2 加入房间(客户端)\n请选择："; cin >> mode;
	char board[15][15];
	for (int i = 0; i < 15; i++) for (int j = 0; j < 15; j++) board[i][j] = ' ';
	bool isHost = (mode == 1);
	bool myTurn = isHost;
	SOCKET sock = INVALID_SOCKET;
	
	if (isHost) {
		jz();
		bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
		listen(serverSock, 1);
		cout << "等待客户端连接(端口" << PORT << ")...\n";
		int len = sizeof(clientAddr);
		sock = accept(serverSock, (sockaddr*)&clientAddr, &len);
	} else {
		jz();
		cout << "输入主机IP："; cin >> ipBuf;
		clientAddr.sin_family = AF_INET;
		clientAddr.sin_port = htons(PORT);
		clientAddr.sin_addr.S_un.S_addr = inet_addr(ipBuf);
		connect(clientSock, (sockaddr*)&clientAddr, sizeof(clientAddr));
		sock = clientSock;
	}
	
	if (sock == INVALID_SOCKET) { cout << "连接失败！回车返回\n"; _getch(); jz(); return; }
	curSock = sock;
	char sendBuf[BUF_LEN], recvBuf[BUF_LEN], raw[BUF_LEN];
	cout << "连接成功！你是" << (isHost ? "先手X" : "后手O") << endl;
	Sleep(1000);
	system("cls");
	
	bool over = false;
	int winStatus = 0;
	while (!over) {
		printBoard(board);
		if (isBoardFull(board)) { cout << "平局\n"; winStatus = 0; over = true; break; }
		
		if (myTurn) {
			int x, y;
			cout << "你的回合，请输入行列：";
			string inputx, inputy;
			cin >> inputx >> inputy;
			if (inputx.size() == 1 || inputx.size() == 2) {
				if (inputx.size() == 1) x = inputx[0] - '0';
				else x = (inputx[0] - '0') * 10 + (inputx[1] - '0');
			} else { cout << "坐标超出范围！\n"; continue; }
			if (inputy.size() == 1 || inputy.size() == 2) {
				if (inputy.size() == 1) y = inputy[0] - '0';
				else y = (inputy[0] - '0') * 10 + (inputy[1] - '0');
			} else { cout << "坐标超出范围！\n"; continue; }
			x--; y--;
			if (x < 0 || x >= 15 || y < 0 || y >= 15 || board[x][y] != ' ') { cout << "非法位置\n"; continue; }
			char ch = isHost ? 'X' : 'O';
			board[x][y] = ch;
			char data[2] = { (char)x, (char)y };
			int packLen = packData(sendBuf, data, 2);
			send(sock, sendBuf, packLen, 0);
			if (checkWin(board, x, y, ch)) { printBoard(board); cout << "你获胜！\n"; winStatus = 1; over = true; break; }
			myTurn = false;
		} else {
			cout << "等待对手落子...\n";
			ZeroMemory(recvBuf, BUF_LEN);
			int recvLen = recv(sock, recvBuf, BUF_LEN, 0);
			if (recvLen <= 0) { cout << "对手断开连接\n"; over = true; break; }
			int len = unpackData(recvBuf, raw);
			if (len != 2) { cout << "数据包校验失败\n"; continue; }
			int x = raw[0], y = raw[1];
			char ch = isHost ? 'O' : 'X';
			board[x][y] = ch;
			if (checkWin(board, x, y, ch)) { printBoard(board); cout << "对手获胜\n"; winStatus = -1; over = true; break; }
			myTurn = true;
		}
		system("cls");
	}
	
	// ✅ 积分结算
	char myCh = isHost ? 'X' : 'O';
	int rawScore = getGameScore(board, myCh);
	settleScore(rawScore, winStatus);
	
	closesocket(sock);
	cout << "对局结束，回车返回\n"; _getch(); jz();
}

//===========================================================================
// 6. 联机井字棋
//===========================================================================
void NetTTT() {
	if (!netInitOk) { cout << "网络初始化失败！回车返回\n"; _getch(); jz(); return; }
	int mode; char ipBuf[20];
	cout << "==== 局域网井字棋 ====\n1 创建房间\n2 加入房间\n选择："; cin >> mode;
	char ttt[3][3];
	for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) ttt[i][j] = ' ';
	bool isHost = (mode == 1), myTurn = isHost;
	SOCKET sock = INVALID_SOCKET;
	
	if (isHost) {
		jz();
		bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
		listen(serverSock, 1);
		cout << "等待连接...\n";
		int len = sizeof(clientAddr);
		sock = accept(serverSock, (sockaddr*)&clientAddr, &len);
	} else {
		jz();
		cout << "主机IP："; cin >> ipBuf;
		clientAddr.sin_family = AF_INET;
		clientAddr.sin_port = htons(PORT);
		clientAddr.sin_addr.S_un.S_addr = inet_addr(ipBuf);
		connect(clientSock, (sockaddr*)&clientAddr, sizeof(clientAddr));
		sock = clientSock;
	}
	
	if (sock == INVALID_SOCKET) { cout << "连接失败\n"; _getch(); jz(); return; }
	curSock = sock;
	char sendBuf[BUF_LEN], recvBuf[BUF_LEN], raw[BUF_LEN];
	cout << "连接成功，你" << (isHost ? "先手X" : "后手O") << endl;
	Sleep(1000);
	system("cls");
	
	bool over = false;
	int winStatus = 0;
	while (!over) {
		cout << "   1   2   3" << endl;
		cout << "1  " << ttt[0][0] << " | " << ttt[0][1] << " | " << ttt[0][2] << endl;
		cout << "   --+---+--" << endl;
		cout << "2  " << ttt[1][0] << " | " << ttt[1][1] << " | " << ttt[1][2] << endl;
		cout << "   --+---+--" << endl;
		cout << "3  " << ttt[2][0] << " | " << ttt[2][1] << " | " << ttt[2][2] << endl;
		
		bool full = true;
		for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (ttt[i][j] == ' ') full = false;
		if (full) { cout << "平局\n"; winStatus = 0; over = true; break; }
		
		if (myTurn) {
			int x, y;
			cout << "你的回合，行列：";
			string Row, Col;
			cin >> Row >> Col;
			if (Row.size() == 1) x = Row[0] - '0';
			else { cout << "坐标超出范围！\n"; continue; }
			if (Col.size() == 1) y = Col[0] - '0';
			else { cout << "坐标超出范围！\n"; continue; }
			x--; y--;
			if (x < 0 || x >= 3 || y < 0 || y >= 3 || ttt[x][y] != ' ') { cout << "非法位置\n"; continue; }
			char ch = isHost ? 'X' : 'O';
			ttt[x][y] = ch;
			char data[2] = { (char)x, (char)y };
			int pl = packData(sendBuf, data, 2);
			send(sock, sendBuf, pl, 0);
			bool win = false;
			for (int i = 0; i < 3; i++) if (ttt[i][0] == ch && ttt[i][1] == ch && ttt[i][2] == ch) win = true;
			for (int j = 0; j < 3; j++) if (ttt[0][j] == ch && ttt[1][j] == ch && ttt[2][j] == ch) win = true;
			if (ttt[0][0] == ch && ttt[1][1] == ch && ttt[2][2] == ch) win = true;
			if (ttt[0][2] == ch && ttt[1][1] == ch && ttt[2][0] == ch) win = true;
			if (win) { cout << "你赢了\n"; winStatus = 1; over = true; break; }
			myTurn = false;
		} else {
			cout << "等待对手...\n";
			ZeroMemory(recvBuf, BUF_LEN);
			int rl = recv(sock, recvBuf, BUF_LEN, 0);
			if (rl <= 0) { cout << "连接断开\n"; over = true; break; }
			int len = unpackData(recvBuf, raw);
			if (len != 2) { cout << "校验错误\n"; continue; }
			int x = raw[0], y = raw[1];
			char ch = isHost ? 'O' : 'X';
			ttt[x][y] = ch;
			bool win = false;
			for (int i = 0; i < 3; i++) if (ttt[i][0] == ch && ttt[i][1] == ch && ttt[i][2] == ch) win = true;
			for (int j = 0; j < 3; j++) if (ttt[0][j] == ch && ttt[1][j] == ch && ttt[2][j] == ch) win = true;
			if (ttt[0][0] == ch && ttt[1][1] == ch && ttt[2][2] == ch) win = true;
			if (ttt[0][2] == ch && ttt[1][1] == ch && ttt[2][0] == ch) win = true;
			if (win) { cout << "对手胜利\n"; winStatus = -1; over = true; break; }
			myTurn = true;
		}
		system("cls");
	}
	
	// ✅ 积分结算
	int rawScore = 30;
	settleScore(rawScore, winStatus);
	
	closesocket(sock);
	cout << "对局结束回车返回\n"; _getch(); jz();
}

//===========================================================================
// 鸣谢
//===========================================================================
void minx_cjwt() {
	system("cls");
	cout << "----------- 鸣谢 -----------\n";
	Sleep(50);
	cout << "·ACGO 用户id:4954991\n注:(由于此用户的用户名中带有中文字符，所以只能写用户id)提出的死循环BUG\n";
	Sleep(50);
	cout << "感谢以上用户的支持!\n\n";
	Sleep(50);
	cout << "------ 常见问题及解答 ------\n";
	Sleep(50);
	cout << "·见原码网页\n";
	cout << "感谢游玩，祝您游玩愉快!\n";
	Sleep(1000);
	cout << "按下任意键继续...\n";
	char a = _getch();
	system("cls");
}

//===========================================================================
// 设置菜单（修改用户名 / 小贴士）
//===========================================================================
void settingsMenu() {
	checkExpire();
	system("cls");
	
	cout << "==== 设置 ====\n";
	cout << "1. 修改用户名\n";
	cout << "2. 修改小贴士（VIP专属）\n";
	cout << "0. 返回\n>> ";
	
	int c; cin >> c;
	
	if (c == 1) {
		cout << "请输入新用户名：";
		string newName;
		cin >> newName;
		// 检查重名
		WIN32_FIND_DATAA fd;
		HANDLE h = FindFirstFileA("User/User*.key", &fd);
		bool dup = false;
		do {
			FILE* fp = fopen(("User/" + string(fd.cFileName)).c_str(), "rb");
			if (!fp) continue;
			UserData tmp;
			fread(&tmp, sizeof(UserData), 1, fp);
			fclose(fp);
			if (strcmp(tmp.username, newName.c_str()) == 0) { dup = true; break; }
		} while (FindNextFileA(h, &fd));
		
		if (dup) {
			MessageBoxA(NULL, "用户名已存在", "提示", MB_OK | MB_ICONHAND);
		} else {
			strcpy(curUser.username, newName.c_str());
			saveUser(curUser);
			cout << "用户名修改成功！\n";
			Sleep(800);
		}
	} else if (c == 2) {
		__time64_t now = _time64(nullptr);
		if (isDeveloper) {
			// 开发者直接允许
			cout << "请输入新的小贴士（最多127字符）：\n";
			string newTip;
			cin.ignore();
			getline(cin, newTip);
			strcpy(curUser.tip, newTip.c_str());
			saveUser(curUser);
			cout << "小贴士修改成功！\n";
			Sleep(800);
		} else if (now > curUser.vipEndTime && !curUser.hasPermanentVIP) {
			MessageBoxA(NULL, "修改小贴士是VIP专属功能！", "提示", MB_OK | MB_ICONHAND);
		} else {
			cout << "请输入新的小贴士（最多127字符）：\n";
			string newTip;
			cin.ignore();
			getline(cin, newTip);
			strcpy(curUser.tip, newTip.c_str());
			saveUser(curUser);
			cout << "小贴士修改成功！\n";
			Sleep(800);
		}
	}
}

//===========================================================================
// 主函数
//===========================================================================
int main() {
	srand((unsigned)time(0));
	
	// ✅ 强制登录
	loginMenu();
	if (!loggedIn) return 0;
	
	string in = " ";
	while (1) {
		checkExpire();
		in = " ";
		system("cls");
		
		cout << "========================================\n";
		cout << "  当前用户：" << curUser.username;
		cout << "  |  积分：" << curUser.score;
		if (curUser.hasPermanentVIP) cout << "  |  [永久VIP]";
		else if (_time64(nullptr) < curUser.vipEndTime) cout << "  |  [VIP]";
		cout << "\n========================================\n";
		if (isDeveloper) {
			cout << "  0. 开发者模式 （已激活）\n";
		} else {
			cout << "  0. 开发者模式(密码)\n";
		}
		cout << "  1. 单人AI五子棋\n";
		cout << "  2. 双人本地五子棋\n";
		cout << "  3. 单人井字棋\n";
		cout << "  4. 双人本地井字棋\n";
		cout << "  5. 局域网联机五子棋(部分电脑可能弹出防火墙提示)\n";
		cout << "  6. 局域网联机井字棋(部分电脑可能弹出防火墙提示)\n";
		cout << "  7. 原码网页跳转\n";
		cout << "  8. 国际象棋/象棋(外部链接)\n";
		cout << "  9. 数独游戏\n";
		cout << " 10. 鸣谢及常见问题\n";
		cout << " 11. 商店\n";
		cout << " 12. 设置\n";
		cout << " 13. 退出\n\n";
		cout << "请输入编号：";
		cin >> in;
		system("cls");
		
		if (in == "0") {
			// 开发者模式入口（密码：xgy11111i，SHA-256 + Base64 双加密验证）
			activateDevMode();
			jz();
		} else if (in == "1") { jz(); wzq(); }
		else if (in == "2") { jz(); Gobang(); }
		else if (in == "3") { jz(); Tic_Tac_Toe(); }
		else if (in == "4") { jz(); Tic_Tac_Toe_1(); }
		else if (in == "5") { jz(); NetGobang(); }
		else if (in == "6") { jz(); NetTTT(); }
		else if (in == "7") {
			jz();
			cout << "1:ACGO  2:洛谷  3:返回\n输入："; cin >> in;
			char c[256] = {0};
			if (in == "1") strncpy(c, URL[1].c_str(), 255), ask(c);
			else if (in == "2") strncpy(c, URL[2].c_str(), 255), ask(c);
			else jz();
		} else if (in == "8") {
			char c[256] = {0};
			jz();
			cout << "打开方式:\n1.acgo\n2.洛谷\n";
			cin >> in;
			if (in == "1") {
				cerr << "[警告|warning]所有象棋部分的代码还在公测阶段，可能存在问题\n";
				cout << "请输入游戏编号:\n1.中国象棋\n2.国际象棋\n";
				cin >> in;
				if (in == "1") { strncpy(c, URL[3].c_str(), 255); ask(c); }
				if (in == "2") { strncpy(c, URL[4].c_str(), 255); ask(c); }
				system("cls");
			} else if (in == "2") {
				cout << "由于c++无法渲染棋盘和棋子，所以此项目由python编写，敬请谅解！\n";
				MessageBoxA(NULL, "此项目暂未上传到OJ平台，\n制作不易敬请谅解!", "提示", MB_OK | MB_ICONEXCLAMATION);
				system("cls");
			}
		}
		else if (in == "9") {Sudoku();}
		else if (in == "10") { minx_cjwt(); }
		else if (in == "11") { shop(); }
		else if (in == "12") { settingsMenu(); }
		else if (in == "13") { closeNetSocket(); return 0; }
		else MessageBoxA(NULL, "输入无效，请重新选择", "提示", MB_OK);
	}
	return 0;
}

