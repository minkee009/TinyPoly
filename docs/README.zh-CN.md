<div align="center">

[English](../README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | **简体中文** | [繁體中文](README.zh-TW.md)

<h1><img src="icon.png" width="64" align="absmiddle" alt="TinyPoly 图标"> TinyPoly</h1>

**小巧轻量的低多边形 3D 建模器 + 像素贴图绘制工具**

在四分视图中点击、拖动来制作网格，亲手绘制 128×128 的像素贴图（最大 1024×1024），<br>
再导出为 OBJ / glTF / GLB。

![version](https://img.shields.io/badge/version-0.5.4-f8c040)
![language](https://img.shields.io/badge/C23-raylib%205.5-3ca0fa)
![platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-8474be)
![license](https://img.shields.io/badge/license-MIT-50d250)

![建模模式](model.png)

</div>

---

## ✦ 功能

### 建模
- **四分视图** — top / 3d / front / side。按 `Space` 将鼠标下方的视图切换为全屏
- **7 种基本形状** — plane, cube, prism, pyramid, cylinder, cone, sphere。在空白处右键即可朝向该视图创建，也可以用顶栏的 `+` 按钮
- **点击拖动编辑** — 拖动网格 / 顶点来移动，`R`+拖动旋转，`T`+拖动缩放，拖动时按住 `Shift` 移动原点，`Ctrl` 反转网格吸附
- **框选** — 2D 视图在空白处拖动，3D 视图用 `Ctrl`+拖动（`Shift` 追加选择）
- **编辑** — 挤出面、删除面 / 顶点、合并与吸附顶点、镜像、克隆、复制 / 粘贴网格
- **选择栏** — 选中面、顶点或整个网格后，视图下方会出现一个小栏，显示选中了什么以及可用的操作
  - 面：挤出、删除 │ 面颜色、选项
  - 顶点：合并、吸附到网格、删除（选中 2 个以上顶点后才能合并）
  - 网格：原点居中、克隆、删除
- **右键菜单** — 菜单随光标下的对象（顶点 / 面 / 网格 / 空白处）而变化，顶部标题会说明是哪种菜单
- **面属性（`F`）** — 颜色、双面、明暗、贴图、在后方绘制
- **项目窗口（`O`）** — 每行都有隐藏（眼睛）、锁定（锁）、重命名（双击）、删除（垃圾桶），`+` 添加网格，右键名称打开网格菜单（重命名、原点居中、镜像、克隆、复制、删除）
- 三级平面着色、透明像素、贴图 / 纯色 / 线框渲染（`M`）、明暗开关（`L`）

### 贴图
![贴图模式](texture.png)

- **RGBA 贴图，128 到 1024** — 用工具栏的尺寸按钮修改（保留左上角的像素）
- **工具** — UV（`V`）、画笔（`B`）、取色器（`I`）、填充（`G`）、直线、矩形、圆形、区域选择。再点一次矩形 / 圆形切换为实心版本
- **UV 编辑** — 拖动、移动角点、旋转（`R`）、翻转（`H`）、缩放（`T`+拖动）、方向键逐像素移动、复制 / 粘贴
- **区域选择** — 移动、复制 / 粘贴（`Ctrl+C/V`）、删除（`Del`）
- **颜色窗口（`P`）** — 饱和度 / 明度方块、色相条、透明度、十六进制输入。左键和右键各用一种颜色（`X` 交换）
- **调色板** — 最多 256 个常用颜色。添加、覆盖、删除。第一格是透明（橡皮擦）
- 缩放 12.5% 到 3200%，或适应窗口（`1` 适应、`2` 100%、`3` 200%、`4` 400%）
- 导入 PNG（最大 1024×1024，保持原始尺寸）

### 文件
![文件浏览器](file-browser.png)

- 内置文件浏览器 — 在所有系统上外观和操作都相同
- 自有格式 `.tpm`（文本：网格 + UV + 贴图（PNG）+ 调色板）。也能打开 0.4.x 的 16 色文件
- 把 `.tpm` / `.png` 文件拖放到窗口上也能打开

### 导出
![导出窗口](export.png)

点击 File → **export...** 打开导出窗口。选择 3D 格式和保存方式（文件夹 / zip），窗口会准确列出将要生成的文件。

| 格式 | 文件夹或 zip 中的文件 |
|---|---|
| OBJ + MTL | `名称.obj` + `名称.mtl` + `名称.png` |
| glTF | `名称.gltf` + `名称.bin` + `名称.png` |
| GLB | `名称.glb`（包含贴图） |

未使用贴图的纯色面的颜色会存放在贴图 PNG 底部的色带中，因此在任何引擎中打开颜色都一样。

### 界面
- 所有按钮都是像素图标 — 鼠标悬停片刻会以提示框显示名称和快捷键
- 像素风圆角窗口，标题栏右侧有关闭按钮，可调整大小的窗口右下角有拖动手柄
- 保存、错误等通知会在左下角短暂弹出后淡出
- 文本框 — 闪烁的光标，`←` `→` `Home` `End` 移动（按住 `Shift` 选择），拖动选择，双击选中单词、三击全选，`Ctrl+A/C/X/V`

### 像素光标
![光标](cursors.png)

光标会随当前操作变化。铅笔笔尖和油漆桶显示当前颜色，取色器中的液体显示光标下方的颜色。

---

## ✦ 构建

### 准备
| 工具 | 版本 / 说明 |
|---|---|
| CMake | 3.25 或更高 |
| Ninja | 预设使用 `Ninja Multi-Config` |
| C 编译器 | 支持 C23（GCC 13+、Clang 17+） |
| raylib 5.5 | 已安装则直接使用，否则由 CMake 自动下载并构建 |
| Git | 自动下载 raylib 时需要 |

<details>
<summary><b>Windows</b>: MinGW-w64 (GCC)</summary>

使用 [Scoop](https://scoop.sh) 安装（PowerShell）：

```powershell
scoop install git cmake ninja mingw
```

或使用 winget：

```powershell
winget install Git.Git Kitware.CMake Ninja-build.Ninja BrechtSanders.WinLibs.POSIX.UCRT
```

打开新的终端，确认 `gcc --version` 为 13 或更高，然后直接运行下方的[命令](#命令)。
可执行文件位于 `build\Windows\bin\Release\TinyPoly.exe`。

> 尚未测试 MSVC（Visual Studio）。请使用 MinGW GCC 构建。
</details>

<details>
<summary><b>Linux</b>: 构建 raylib 所需的软件包</summary>

**Debian / Ubuntu**
```sh
sudo apt install build-essential cmake ninja-build gdb \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
```

**Fedora**
```sh
sudo dnf install gcc-c++ cmake ninja-build gdb \
    libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel
```

**Arch**
```sh
sudo pacman -S --needed base-devel cmake ninja gdb \
    libx11 libxrandr libxinerama libxcursor libxi mesa
```
</details>

<details>
<summary><b>macOS</b></summary>

```sh
xcode-select --install
brew install cmake ninja
```
</details>

### 命令
```sh
cmake --preset default
cmake --build --preset release      # 或 debug
```

可执行文件位于 `build/<OS>/bin/Release/TinyPoly`（`<OS>` 为 `Windows`、`Darwin` 或 `Linux`）。

### VS Code
自带的 `.vscode` 配置可以按 **F5** 构建并调试。在 **运行和调试** 中选择与你的系统对应的配置。
调试需要 Microsoft 的 **C/C++ 扩展**（`ms-vscode.cpptools`），打开文件夹时 VS Code 会提示安装。

---

## ✦ 操作

### 建模
| 输入 | 动作 |
|---|---|
| 点击 / 拖动 | 选择、移动 |
| 拖动空白处（2D）/ `Ctrl`+拖动（3D） | 框选（`Shift` 追加） |
| `R` / `T` + 拖动 | 旋转 / 缩放 |
| 拖动时按 `Shift` | 移动原点 |
| 右键 | 右键菜单 |
| 右键拖动 | 环绕（3D）/ 平移（2D） |
| 滚轮 / 中键拖动 | 缩放 / 平移 |
| `E` / `X` / `Del` | 挤出 / 删除面 / 删除顶点 |
| `F` / `G` | 面 / 网格属性 |
| `Ctrl+C` / `Ctrl+V` / `Ctrl+X` | 复制 / 粘贴 / 删除网格 |
| 方向键、WASD | 将选中对象移动一个网格（`Ctrl`：¼ 格） |
| `Space` | 将鼠标下方的视图全屏 |
| `M` / `L` | 渲染模式 / 明暗 |
| `Tab` | 贴图模式 |

### 贴图
| 输入 | 动作 |
|---|---|
| `V` `B` `I` `G` | UV、画笔、取色器、填充 |
| `Ctrl`+点击 | 拾取颜色 |
| 右键 | 用第二种颜色绘制（UV 工具下为菜单） |
| `X` | 交换左右键颜色 |
| `R` / `H` / `T`+拖动 | 旋转 / 翻转 / 缩放 UV |
| 方向键、WASD | UV 移动一个像素 |
| `Ctrl+C` / `Ctrl+V` / `Del` | 复制 / 粘贴 / 删除（UV 或选中区域） |
| `Space`+拖动、中键拖动 | 平移 |
| 滚轮 / `1`–`4` | 缩放 / 适应、100、200、400% |
| `Tab` | 建模模式 |

**通用：** `Ctrl+Z/Y` 撤销 / 重做，`Ctrl+S` 保存，`Ctrl+Shift+S` 另存为，`Ctrl+O` 打开，`O` 项目窗口，`P` 颜色窗口

---

## ✦ 路线图

- [ ] 动画时间轴
- [ ] 导出 GIF / 精灵图
- [ ] 项目窗口中的文件夹
- [ ] 未保存就关闭时的提醒
- [ ] 显示中文等非拉丁字符的文件名（UI 字体）

---

## ✦ 许可证

TinyPoly 以 [MIT 许可证](../LICENSE)发布。

使用的库：

| 库 | 用途 | 许可证 | 引入方式 |
|---|---|---|---|
| [raylib](https://github.com/raysan5/raylib) 5.5 | 窗口、输入、渲染 | zlib | 构建时由 CMake 下载 |
| [microui](https://github.com/rxi/microui) 2.02 | UI | MIT | 未经修改放在 `src/microui/` 中 |

各库的版权声明见 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)。
