<div align="center">

[English](../README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [简体中文](README.zh-CN.md) | **繁體中文**

<h1><img src="icon.png" width="64" align="absmiddle" alt="TinyPoly 圖示"> TinyPoly</h1>

**小巧輕量的低多邊形 3D 建模工具 + 像素貼圖繪製工具**

在四分割視圖中點擊、拖曳來製作網格，親手繪製 128×128 的像素貼圖（最大 1024×1024），<br>
再匯出為 OBJ / glTF / GLB。

![version](https://img.shields.io/badge/version-0.5.4-f8c040)
![language](https://img.shields.io/badge/C23-raylib%205.5-3ca0fa)
![platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-8474be)
![license](https://img.shields.io/badge/license-MIT-50d250)

![建模模式](model.png)

</div>

---

## ✦ 功能

### 建模
- **四分割視圖** — top / 3d / front / side。按 `Space` 將滑鼠下方的視圖切換為全螢幕
- **7 種基本形狀** — plane, cube, prism, pyramid, cylinder, cone, sphere。在空白處按右鍵即可朝向該視圖建立，也可以用頂端列的 `+` 按鈕
- **點擊拖曳編輯** — 拖曳網格 / 頂點來移動，`R`+拖曳旋轉，`T`+拖曳縮放，拖曳時按住 `Shift` 移動原點，`Ctrl` 反轉格線貼齊
- **框選** — 2D 視圖在空白處拖曳，3D 視圖用 `Ctrl`+拖曳（`Shift` 加選）
- **編輯** — 擠出面、刪除面 / 頂點、合併與貼齊頂點、鏡像、再製、複製 / 貼上網格
- **選取列** — 選取面、頂點或整個網格後，視圖下方會出現一個小列，顯示選取了什麼以及可用的操作
  - 面：擠出、刪除 │ 面顏色、選項
  - 頂點：合併、貼齊格線、刪除（選取 2 個以上頂點才能合併）
  - 網格：原點置中、再製、刪除
- **右鍵選單** — 選單會依游標下的對象（頂點 / 面 / 網格 / 空白處）而不同，最上方的標題會說明是哪種選單
- **面屬性（`F`）** — 顏色、雙面、明暗、貼圖、在後方繪製
- **專案視窗（`O`）** — 每列都有隱藏（眼睛）、鎖定（鎖頭）、重新命名（雙擊）、刪除（垃圾桶），`+` 新增網格，在名稱上按右鍵開啟網格選單（重新命名、原點置中、鏡像、再製、複製、刪除）
- 三階平面著色、透明像素、貼圖 / 單色 / 線框算繪（`M`）、明暗切換（`L`）

### 貼圖
![貼圖模式](texture.png)

- **RGBA 貼圖，128 到 1024** — 用工具列的尺寸按鈕變更（保留左上角的像素）
- **工具** — UV（`V`）、畫筆（`B`）、滴管（`I`）、填滿（`G`）、直線、矩形、圓形、範圍選取。再按一次矩形 / 圓形切換為實心版本
- **UV 編輯** — 拖曳、移動角點、旋轉（`R`）、翻轉（`H`）、縮放（`T`+拖曳）、方向鍵逐像素移動、複製 / 貼上
- **範圍選取** — 移動、複製 / 貼上（`Ctrl+C/V`）、刪除（`Del`）
- **顏色視窗（`P`）** — 飽和度 / 明度方塊、色相條、透明度、十六進位輸入。左鍵和右鍵各用一種顏色（`X` 交換）
- **調色盤** — 最多 256 個常用顏色。新增、覆寫、刪除。第一格是透明（橡皮擦）
- 縮放 12.5% 到 3200%，或符合視窗（`1` 符合、`2` 100%、`3` 200%、`4` 400%）
- 匯入 PNG（最大 1024×1024，維持原始尺寸）

### 檔案
![檔案瀏覽器](file-browser.png)

- 內建檔案瀏覽器 — 在所有作業系統上外觀和操作都相同
- 自有格式 `.tpm`（文字：網格 + UV + 貼圖（PNG）+ 調色盤）。也能開啟 0.4.x 的 16 色檔案
- 把 `.tpm` / `.png` 檔案拖放到視窗上也能開啟

### 匯出
![匯出視窗](export.png)

點擊 File → **export...** 開啟匯出視窗。選擇 3D 格式和儲存方式（資料夾 / zip），視窗會準確列出將要產生的檔案。

| 格式 | 資料夾或 zip 中的檔案 |
|---|---|
| OBJ + MTL | `名稱.obj` + `名稱.mtl` + `名稱.png` |
| glTF | `名稱.gltf` + `名稱.bin` + `名稱.png` |
| GLB | `名稱.glb`（包含貼圖） |

未使用貼圖的單色面，其顏色會存放在貼圖 PNG 底部的色帶中，因此在任何引擎中開啟顏色都一樣。

### 介面
- 所有按鈕都是像素圖示 — 滑鼠停留片刻會以工具提示顯示名稱和快捷鍵
- 像素風圓角視窗，標題列右側有關閉按鈕，可調整大小的視窗右下角有拖曳控點
- 儲存、錯誤等通知會在左下角短暫出現後淡出
- 文字輸入框 — 閃爍的插入點，`←` `→` `Home` `End` 移動（按住 `Shift` 選取），拖曳選取，雙擊選取單字、三擊全選，`Ctrl+A/C/X/V`

### 像素游標
![游標](cursors.png)

游標會隨目前的操作改變。鉛筆筆尖和油漆桶顯示目前的顏色，滴管中的液體顯示游標下方的顏色。

---

## ✦ 建置

### 準備
| 工具 | 版本 / 說明 |
|---|---|
| CMake | 3.25 或更新 |
| Ninja | 預設組態使用 `Ninja Multi-Config` |
| C 編譯器 | 支援 C23（GCC 13+、Clang 17+） |
| raylib 5.5 | 已安裝就直接使用，否則由 CMake 自動下載並建置 |
| Git | 自動下載 raylib 時需要 |

<details>
<summary><b>Windows</b>: MinGW-w64 (GCC)</summary>

使用 [Scoop](https://scoop.sh) 安裝（PowerShell）：

```powershell
scoop install git cmake ninja mingw
```

或使用 winget：

```powershell
winget install Git.Git Kitware.CMake Ninja-build.Ninja BrechtSanders.WinLibs.POSIX.UCRT
```

開啟新的終端機，確認 `gcc --version` 為 13 或更新，然後直接執行下方的[指令](#指令)。
執行檔位於 `build\Windows\bin\Release\TinyPoly.exe`。

> 尚未測試 MSVC（Visual Studio）。請使用 MinGW GCC 建置。
</details>

<details>
<summary><b>Linux</b>: 建置 raylib 所需的套件</summary>

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

### 指令
```sh
cmake --preset default
cmake --build --preset release      # 或 debug
```

執行檔位於 `build/<OS>/bin/Release/TinyPoly`（`<OS>` 為 `Windows`、`Darwin` 或 `Linux`）。

### VS Code
內附的 `.vscode` 設定可以按 **F5** 建置並偵錯。在 **執行與偵錯** 中選擇對應你作業系統的組態。
偵錯需要 Microsoft 的 **C/C++ 擴充功能**（`ms-vscode.cpptools`），開啟資料夾時 VS Code 會建議安裝。

---

## ✦ 操作

### 建模
| 輸入 | 動作 |
|---|---|
| 點擊 / 拖曳 | 選取、移動 |
| 拖曳空白處（2D）/ `Ctrl`+拖曳（3D） | 框選（`Shift` 加選） |
| `R` / `T` + 拖曳 | 旋轉 / 縮放 |
| 拖曳時按 `Shift` | 移動原點 |
| 右鍵 | 右鍵選單 |
| 右鍵拖曳 | 環繞（3D）/ 平移（2D） |
| 滾輪 / 中鍵拖曳 | 縮放 / 平移 |
| `E` / `X` / `Del` | 擠出 / 刪除面 / 刪除頂點 |
| `F` / `G` | 面 / 網格屬性 |
| `Ctrl+C` / `Ctrl+V` / `Ctrl+X` | 複製 / 貼上 / 刪除網格 |
| 方向鍵、WASD | 將選取的對象移動一格（`Ctrl`：¼ 格） |
| `Space` | 將滑鼠下方的視圖全螢幕 |
| `M` / `L` | 算繪模式 / 明暗 |
| `Tab` | 貼圖模式 |

### 貼圖
| 輸入 | 動作 |
|---|---|
| `V` `B` `I` `G` | UV、畫筆、滴管、填滿 |
| `Ctrl`+點擊 | 吸取顏色 |
| 右鍵 | 用第二種顏色繪製（UV 工具下為選單） |
| `X` | 交換左右鍵顏色 |
| `R` / `H` / `T`+拖曳 | 旋轉 / 翻轉 / 縮放 UV |
| 方向鍵、WASD | UV 移動一個像素 |
| `Ctrl+C` / `Ctrl+V` / `Del` | 複製 / 貼上 / 刪除（UV 或選取範圍） |
| `Space`+拖曳、中鍵拖曳 | 平移 |
| 滾輪 / `1`–`4` | 縮放 / 符合、100、200、400% |
| `Tab` | 建模模式 |

**共通：** `Ctrl+Z/Y` 復原 / 重做，`Ctrl+S` 儲存，`Ctrl+Shift+S` 另存新檔，`Ctrl+O` 開啟，`O` 專案視窗，`P` 顏色視窗

---

## ✦ 未來規劃

- [ ] 動畫時間軸
- [ ] 匯出 GIF / 精靈圖
- [ ] 專案視窗中的資料夾
- [ ] 未儲存就關閉時的警告
- [ ] 顯示中文等非拉丁字元的檔名（UI 字型）

---

## ✦ 授權

TinyPoly 以 [MIT 授權](../LICENSE)釋出。

使用的函式庫：

| 函式庫 | 用途 | 授權 | 引入方式 |
|---|---|---|---|
| [raylib](https://github.com/raysan5/raylib) 5.5 | 視窗、輸入、算繪 | zlib | 建置時由 CMake 下載 |
| [microui](https://github.com/rxi/microui) 2.02 | UI | MIT | 未經修改放在 `src/microui/` 中 |

各函式庫的版權聲明請見 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)。
