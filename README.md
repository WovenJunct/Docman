# 中作业说明
## 一、项目概述

`docman` 是一个命令行工具，用于处理含有引用标记的文本文件。它读取一个文献合集（JSON 格式）和一篇正文文本，找出正文中所有的 `[id]` 引用标记，从文献合集中匹配对应条目（必要时通过网络补全信息），最后将正文和格式化好的参考文献列表一并输出。

**命令行格式：**
```
./docman -c citations.json [-o output.txt] input.txt
./docman -c citations.json [-o output.txt] -      # "-" 表示从 stdin 读取
```

## 二、文件结构

```
docman/
├── main.cpp        ← 主流程：参数解析、加载、处理、输出
├── citation.h      ← Citation 类族的声明（多态设计核心）
├── citation.cpp    ← Citation 类族的实现
└── utils.hpp       ← 工具函数：URL 编码、文件读取
```

---

## 三、各文件详细说明

### 3.1 `utils.hpp` — 工具函数

这是一个纯头文件工具库，所有函数均为 `inline`，直接在头文件中实现，无需单独编译。

#### `API_ENDPOINT`
```cpp
const std::string API_ENDPOINT{"http://docman.zhuof.wang"};
```
全局常量，存储后端 API 的根地址。`BookCitation` 和 `WebpageCitation` 的网络请求均以此为基础拼接路径。

#### `encodeUriComponent(s)`
将任意字符串编码为合法的 URL 组成部分：
- 字母、数字及 `-_~.` 保持原样
- 空格编码为 `+`
- 其他字符编码为 `%XX`（十六进制）

这是因为 ISBN 或 URL 中可能含有特殊字符，直接拼入请求路径会导致请求失败。

#### `readFromFile(path)`
将整个文件内容读入一个 `std::string` 并返回：
1. 用 `std::ifstream` 打开文件
2. 检查是否打开成功，失败则 `exit(1)`
3. 用 `ostringstream` + `rdbuf()` 一次性读入全部内容
4. 返回字符串

---

### 3.2 `citation.h` — 类族声明

#### 设计思想：继承 + 多态

三种文献类型（书籍、网页、文章）在结构和行为上都不同，但 `main.cpp` 只需要统一地操作它们。为此，设计了一个**抽象基类** `Citation`，三个子类分别处理各自的逻辑。这样 `main.cpp` 只需要持有 `Citation*` 指针，通过虚函数调用自动分发到正确的实现，不需要任何 `if-else` 类型判断。

```
Citation（抽象基类）
├── BookCitation    — 书籍，ISBN 查询网络
├── WebpageCitation — 网页，URL 查询网络
└── ArticleCitation — 文章，全部字段来自 JSON
```

#### 基类 `Citation`

| 成员 | 类型 | 说明 |
|------|------|------|
| `id` | `std::string` | 文献唯一标识符，只允许字母和数字 |
| `Citation(id)` | 构造函数 | 验证 id 合法性，非法则 `exit(1)` |
| `~Citation()` | 虚析构函数 | 必须声明为 virtual，保证通过基类指针 `delete` 时正确释放子类内存 |
| `format()` | 纯虚函数 | 返回格式化好的引用字符串 |
| `fetchInfo(client)` | 纯虚函数 | 通过网络补全信息；`ArticleCitation` 实现为空操作 |
| `fromJson(obj)` | 静态工厂方法 | 读取 JSON 中的 `type` 字段，创建并返回对应子类实例 |
| `requireString(obj, key, id)` | protected 辅助方法 | 检查 JSON 字段存在且为字符串类型 |
| `requireInt(obj, key, id)` | protected 辅助方法 | 检查 JSON 字段存在且为整数类型 |

**为什么基类没有 `type` 字段？**

`type` 字符串只在 `fromJson` 中用于决定创建哪个子类，一旦对象创建完毕，类型信息就已经被编码进对象的实际 C++ 类型里了。之后所有依赖类型的行为都通过虚函数多态来处理，不需要再用字符串字段做 `if-else` 判断。

#### 子类 `BookCitation`

| 字段 | 来源 |
|------|------|
| `isbn` | JSON 构造时读入 |
| `author, title, publisher, year` | `fetchInfo` 网络请求后填入 |

- `fetchInfo`：请求 `/isbn/{isbn}`，解析返回 JSON 填充四个字段
- `format`：返回 `[id] book: author, title, publisher, year`

#### 子类 `WebpageCitation`

| 字段 | 来源 |
|------|------|
| `url` | JSON 构造时读入 |
| `title` | `fetchInfo` 网络请求后填入 |

- `fetchInfo`：请求 `/title/{url}`，解析返回 JSON 填充 `title`
- `format`：返回 `[id] webpage: title. Available at url`

#### 子类 `ArticleCitation`

| 字段 | 来源 |
|------|------|
| `author, title, journal, year, volume, issue` | 全部从 JSON 构造时读入 |

- `fetchInfo`：空函数体，无需网络请求
- `format`：返回 `[id] article: author, title, journal, year, volume, issue`

---

### 3.3 `citation.cpp` — 类族实现

#### 基类构造函数

```
验证 id 非空
→ 用 std::all_of + std::isalnum 检查每个字符
→ 不合法则输出错误信息并 exit(1)
```

注意：`isalnum` 的参数必须转为 `unsigned char`，否则负值字符在某些平台上会导致未定义行为。

#### `requireString` / `requireInt`

两个 `protected` 静态辅助方法，被三个子类的构造函数和 `fetchInfo` 反复调用，避免重复的字段检查代码。签名中额外带了 `id` 参数，使报错信息能指出是哪条文献出了问题。

#### 工厂方法 `fromJson`

```
检查 obj 是 JSON 对象
→ 检查 "id" 和 "type" 字段存在且为字符串
→ 根据 type 值分支：
    "book"    → new BookCitation(obj)
    "webpage" → new WebpageCitation(obj)
    "article" → new ArticleCitation(obj)
    其他      → exit(1)
```

所有子类的构造都在这里统一发起，调用方不需要知道具体类型。

#### `BookCitation::fetchInfo` 的特殊处理

服务器返回的 `year` 字段可能是整数也可能是字符串（不同数据源格式不一），因此做了兼容处理：

```
year 是整数 → 直接 get<int>()
year 是字符串 → std::stoi() 转换
其他类型 → exit(1)
```

---

### 3.4 `main.cpp` — 主流程

`main.cpp` 是整个程序的骨架，由五个独立函数和一个主函数组成。

#### `struct Args` — 参数容器

```cpp
struct Args {
    std::string citationPath;   // -c 指定的文献文件路径（必填）
    std::string outputPath;     // -o 指定的输出文件路径（选填，空则输出到 stdout）
    std::string inputPath;      // 位置参数，输入文件路径（"-" 表示 stdin）
};
```

用结构体封装解析结果，比直接操作 `argv` 下标更安全、更易读。

#### `parseArgs` — 命令行参数解析

**核心逻辑：** 用一个 `while` 风格的 `for` 循环遍历 `argv`，根据当前 token 决定消耗一个还是两个参数（`i += 1` 或 `i += 2`）：

```
token == "-c" → 下一个参数是文献路径，i += 2
token == "-o" → 下一个参数是输出路径，i += 2
token 以 "-" 开头但不是已知选项 → 未知选项，exit(1)
其他 → 位置参数（inputPath），i += 1
```

**错误检测：**
- `-c` 或 `-o` 重复出现
- `-c` 或 `-o` 后面没有参数（`i+1 >= argc`）
- 出现多个位置参数
- 未知选项（如 `-x`）
- `-c` 或 `inputPath` 最终为空

#### `loadCitations` — 加载文献合集

```
打开 JSON 文件
→ 检查根结构为对象，包含 "citations" 数组
→ 遍历数组，对每个元素调用 Citation::fromJson(obj)
→ 用 std::set<string> seenIds 检测重复 id
→ 将所有 Citation* 存入 vector 返回
```

返回 `vector<Citation*>` 而非 `map`，是为了保留原有代码的内存管理结构（最后 `for (auto c : citations) delete c`）。后续需要按 id 查找时，在 `main` 里临时构建一个 `map` 作为视图，不转移所有权。

#### `readInput` — 读取正文

```
path == "-" → 从 std::cin 读取（支持管道输入）
其他       → 调用 utils.hpp 中的 readFromFile(path)
```

#### `extractIds` — 提取引用 id

这是一个简单的**单遍扫描状态机**，用 `inBracket` 标志位跟踪当前是否在括号内：

```
状态：inBracket = false（初始）

遇到 '[':
    若 inBracket == true  → 嵌套括号，非法，exit(1)
    否则 inBracket = true，清空 current

遇到 ']':
    若 inBracket == false → 多余右括号，非法，exit(1)
    否则 inBracket = false，将 current 加入结果（去重）

遇到其他字符:
    若 inBracket == true → 追加到 current

扫描结束:
    若 inBracket == true → 未闭合左括号，exit(1)
```

**去重且保序：** 用 `std::set<string> seen` 判断是否已出现，用 `std::vector<string> ordered` 按顺序存储首次出现的 id。

#### `main` — 主流程（九步走）

```
第一步  parseArgs          解析命令行参数
第二步  loadCitations      加载文献合集到 vector
第三步  readInput          读取正文到 string
第四步  extractIds         提取正文中所有 [id]
第五步  构建 citationMap   将 vector 转为 map 方便查找（不转移所有权）
        验证               每个引用的 id 都必须存在于 citationMap 中
第六步  排序               将 usedIds 排序，构建 printedCitations（按字典序）
第七步  fetchInfo          对每条文献发起网络请求（Article 是空操作）
第八步  组装输出           先写正文，再写 References 列表，全部写入 ostringstream
第九步  写出               确认无误后才创建输出文件 / 写到 stdout
        释放内存           遍历 citations vector，delete 所有指针
```

**关键设计：输出延迟到最后一步**

所有验证（id 合法性、引用存在性）和网络请求全部完成后，组装的字符串才被写入文件。这保证了：如果任何中间步骤 `exit(1)`，输出文件不会被创建，不会留下空文件或残缺文件。

---

## 四、数据流图

```
argv
  │
  ▼
parseArgs ──────────────────────────────────────────────┐
  │ Args{citationPath, outputPath, inputPath}            │
  │                                                      │
  ├──citationPath──► loadCitations                       │
  │                      │                              │
  │                      ▼                              │
  │               vector<Citation*>                     │
  │               + citationMap                         │
  │                      │                              │
  ├──inputPath───► readInput                             │
  │                      │                              │
  │                      ▼                              │
  │               string (正文)                         │
  │                      │                              │
  │               extractIds                            │
  │                      │                              │
  │                      ▼                              │
  │               usedIds → 验证 → 排序                  │
  │                      │                              │
  │               fetchInfo (网络请求)                   │
  │                      │                              │
  │               format() → ostringstream               │
  │                                                      │
  └──outputPath──────────────────────────► 写出
```

---

## 五、错误处理策略

| 错误类型 | 检测位置 | 处理方式 |
|----------|----------|----------|
| 未知命令行选项 | `parseArgs` | `exit(1)` |
| `-c` / `-o` 重复或缺少参数 | `parseArgs` | `exit(1)` |
| 文献文件无法打开 | `loadCitations` | `exit(1)` |
| JSON 结构不合法 | `loadCitations` / `fromJson` | `exit(1)` |
| 重复的文献 id | `loadCitations` | 释放内存后 `exit(1)` |
| id 含非字母数字字符 | `Citation` 基类构造函数 | `exit(1)` |
| 文献类型未知 | `fromJson` | `exit(1)` |
| 字段缺失或类型错误 | 各子类构造函数 / `fetchInfo` | `exit(1)` |
| 正文括号不配对或嵌套 | `extractIds` | `exit(1)` |
| 引用 id 不存在于文献合集 | `main` 验证步骤 | 释放内存后 `exit(1)` |
| HTTP 请求失败 | `fetchInfo` | `exit(1)` |
| 服务器返回非 200 状态码 | `fetchInfo` | `exit(1)` |
| 网络响应 JSON 解析失败 | `fetchInfo` | `exit(1)` |
| 输出文件无法创建 | `main` 写出步骤 | 释放内存后 `exit(1)` |

所有错误均通过 `std::cerr` 输出描述信息，再调用 `std::exit(1)` 终止程序，符合 Unix 命令行工具的惯例（正常退出返回 0，出错返回非零）。

---

## 六、依赖库

| 库 | 用途 |
|----|------|
| `nlohmann/json` | JSON 文件解析和网络响应解析 |
| `cpp-httplib` | HTTP 客户端，用于 `Book` 和 `Webpage` 的网络查询 |