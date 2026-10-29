<div align="center">

[English](../README.md) | [한국어](README.ko.md) | **日本語** | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md)

<h1><img src="icon.png" width="64" align="absmiddle" alt="TinyPoly アイコン"> TinyPoly</h1>

**小さくて軽いローポリ 3D モデラー + ドット絵テクスチャペインター**

4 分割ビューでクリック・ドラッグしてメッシュを作り、128×128 のドット絵テクスチャ（最大 1024×1024）を手で描いて<br>
OBJ / glTF / GLB に書き出します。

![version](https://img.shields.io/badge/version-0.5.5-f8c040)
![language](https://img.shields.io/badge/C23-raylib%205.5-3ca0fa)
![platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-8474be)
![license](https://img.shields.io/badge/license-MIT-50d250)

![モデリングモード](model.png)

</div>

---

## ✦ 機能

### モデリング
- **4 分割ビュー** — top / 3d / front / side。`Space` でマウスの下のビューを全画面に切り替え
- **基本図形 7 種** — plane, cube, prism, pyramid, cylinder, cone, sphere。何もない所を右クリックするとそのビューの向きで作成、または上部バーの `+` ボタン
- **クリック・ドラッグ編集** — メッシュ / 頂点をドラッグで移動、`R`+ドラッグで回転、`T`+ドラッグで拡大縮小、ドラッグ中に `Shift` で原点を移動、`Ctrl` でグリッドスナップを反転
- **矩形選択** — 2D ビューは何もない所からドラッグ、3D ビューは `Ctrl`+ドラッグ（`Shift` で追加選択）
- **クリック前のハイライト** — クリックすると選ばれるものが水色で先に表示される（面、またはメッシュ全体）。頂点は周りに円が出て、3D ビューではその円の中をクリックすると頂点を選択
- **編集** — 面の押し出し、面 / 頂点の削除、頂点の結合・スナップ、ミラー、複製、メッシュのコピー / 貼り付け
- **選択バー** — 面・頂点・メッシュを選ぶとビューの下に小さなバーが現れ、何を選んでいるかと使える操作を表示
  - 面: 押し出し、削除 │ 面の色、オプション
  - 頂点: 結合、スナップ、削除（結合は頂点を 2 つ以上選ぶと有効）
  - メッシュ: 原点を中央へ、複製、削除
- **右クリックメニュー** — カーソルの下にあるもの（頂点 / 面 / メッシュ / 何もない所）でメニューが変わり、一番上のタイトルで何のメニューかがわかる
- **面のプロパティ（`F`）** — 色、両面、陰影、テクスチャ、背面に描画
- **プロジェクトウィンドウ（`O`）** — 行ごとに非表示（目）、ロック（南京錠）、名前変更（ダブルクリック）、削除（ゴミ箱）、`+` でメッシュ追加、名前を右クリックでメッシュメニュー（名前変更、原点を中央へ、ミラー、複製、コピー、削除）
- 3 段階のフラットシェーディング、透明ピクセル、テクスチャ / 単色 / ワイヤーフレーム表示（`M`）、陰影の切り替え（`L`）

### テクスチャ
![テクスチャモード](texture.png)

- **RGBA テクスチャ、128〜1024** — ツールバーのサイズボタンで変更（左上のピクセルは保持）
- **ツール** — UV（`V`）、ペン（`B`）、スポイト（`I`）、塗りつぶし（`G`）、直線、四角形、円、範囲選択。四角形 / 円はもう一度押すと塗りつぶし版
- **UV 編集** — ドラッグ、角の移動、回転（`R`）、反転（`H`）、拡大縮小（`T`+ドラッグ）、矢印キーで 1 ピクセルずつ移動、コピー / 貼り付け
- **範囲選択** — 移動、コピー / 貼り付け（`Ctrl+C/V`）、削除（`Del`）
- **カラーウィンドウ（`P`）** — 彩度 / 明度の四角、色相バー、アルファ、16 進数入力。左クリックと右クリックで別々の色（`X` で入れ替え）
- **パレット** — よく使う色を最大 256 個。追加・上書き・削除。先頭は透明（消しゴム）
- 拡大 12.5%〜3200%、ウィンドウに合わせる（`1` 合わせる、`2` 100%、`3` 200%、`4` 400%）
- PNG の読み込み（最大 1024×1024、元のサイズのまま）

### ファイル
![ファイルブラウザ](file-browser.png)

- アプリ内蔵のファイルブラウザ — どの OS でも同じ見た目と操作
- 独自形式 `.tpm`（テキスト: メッシュ + UV + テクスチャ（PNG）+ パレット）。0.4.x の 16 色ファイルも開ける
- `.tpm` / `.png` ファイルをウィンドウにドラッグ & ドロップしても開ける

### 書き出し
![書き出しウィンドウ](export.png)

File → **export...** を押すと書き出しウィンドウが開きます。3D 形式と保存方法（フォルダ / zip）を選ぶと、作られるファイルがそのまま表示されます。

| 形式 | フォルダまたは zip に入るファイル |
|---|---|
| OBJ + MTL | `名前.obj` + `名前.mtl` + `名前.png` |
| glTF | `名前.gltf` + `名前.bin` + `名前.png` |
| GLB | `名前.glb`（テクスチャ込み） |

テクスチャを使わない単色の面の色はテクスチャ PNG の下端の色帯に入るので、どのエンジンで開いても同じ色に見えます。

### 画面
- すべてのボタンがドット絵アイコン — マウスを少し乗せると名前とショートカットがツールチップで出る
- ドット風に角を丸めたウィンドウ、タイトルバー右の閉じるボタン、サイズ変更できるウィンドウは右下につまみ
- 保存・エラーなどの通知は左下に少し表示されて消える
- テキスト入力欄 — 点滅するキャレット、`←` `→` `Home` `End` で移動（`Shift` で選択）、ドラッグで選択、ダブルクリックで単語・トリプルクリックで全選択、`Ctrl+A/C/X/V`

### ドット絵カーソル
![カーソル](cursors.png)

操作に合わせて変わるドット絵カーソル。鉛筆の芯と塗りつぶしのバケツは今の色、スポイトの液体はカーソルの下の色を表示します。

---

## ✦ ビルド

### 必要なもの
| ツール | バージョン / 説明 |
|---|---|
| CMake | 3.25 以上 |
| Ninja | プリセットが `Ninja Multi-Config` を使用 |
| C コンパイラ | C23 対応（GCC 13+, Clang 17+） |
| raylib 5.5 | インストール済みならそれを使い、なければ CMake が自動で取得してビルド |
| Git | raylib を自動取得するときに必要 |

<details>
<summary><b>Windows</b>: MinGW-w64 (GCC)</summary>

[Scoop](https://scoop.sh) でインストールする場合（PowerShell）:

```powershell
scoop install git cmake ninja mingw
```

または winget:

```powershell
winget install Git.Git Kitware.CMake Ninja-build.Ninja BrechtSanders.WinLibs.POSIX.UCRT
```

新しいターミナルで `gcc --version` が 13 以上であることを確認し、下の[コマンド](#コマンド)をそのまま実行してください。
実行ファイルは `build\Windows\bin\Release\TinyPoly.exe` にできます。

> MSVC（Visual Studio）はまだ試していません。MinGW GCC でビルドしてください。
</details>

<details>
<summary><b>Linux</b>: raylib のビルドに必要なパッケージ</summary>

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

### コマンド
```sh
cmake --preset default
cmake --build --preset release      # または debug
```

実行ファイルは `build/<OS>/bin/Release/TinyPoly` にできます（`<OS>` は `Windows`、`Darwin`、`Linux`）。

### VS Code
同梱の `.vscode` 設定で **F5** を押すとビルドとデバッグ実行ができます。**実行とデバッグ**で OS に合った構成を選んでください。
デバッグには Microsoft の **C/C++ 拡張機能**（`ms-vscode.cpptools`）が必要です。フォルダを開くと VS Code がインストールを勧めます。

---

## ✦ 操作

### モデリング
| 入力 | 動作 |
|---|---|
| クリック / ドラッグ | 選択、移動 |
| 何もない所をドラッグ（2D）/ `Ctrl`+ドラッグ（3D） | 矩形選択（`Shift` で追加） |
| `R` / `T` + ドラッグ | 回転 / 拡大縮小 |
| ドラッグ中に `Shift` | 原点を移動 |
| 右クリック | メニュー |
| 右ドラッグ | 回転（3D）/ 画面移動（2D） |
| ホイール / 中ドラッグ | ズーム / 画面移動 |
| `E` / `X` / `Del` | 押し出し / 面の削除 / 頂点の削除 |
| `F` / `G` | 面 / メッシュのプロパティ |
| `Ctrl+C` / `Ctrl+V` / `Ctrl+X` | メッシュのコピー / 貼り付け / 削除 |
| 矢印キー、WASD | 選択中のものをグリッド 1 マス移動（`Ctrl`: ¼ マス） |
| `Space` | マウスの下のビューを全画面 |
| `M` / `L` | 表示方式 / 陰影 |
| `Tab` | テクスチャモード |

### テクスチャ
| 入力 | 動作 |
|---|---|
| `V` `B` `I` `G` | UV、ペン、スポイト、塗りつぶし |
| `Ctrl`+クリック | 色を拾う |
| 右クリック | 2 つ目の色で描く（UV ツールではメニュー） |
| `X` | 左右の色を入れ替え |
| `R` / `H` / `T`+ドラッグ | UV の回転 / 反転 / 拡大縮小 |
| 矢印キー、WASD | UV を 1 ピクセル移動 |
| `Ctrl+C` / `Ctrl+V` / `Del` | コピー / 貼り付け / 削除（UV または選択範囲） |
| `Space`+ドラッグ、中ドラッグ | 画面移動 |
| ホイール / `1`–`4` | ズーム / 合わせる・100・200・400% |
| `Tab` | モデリングモード |

**共通:** `Ctrl+Z/Y` 元に戻す・やり直し、`Ctrl+S` 保存、`Ctrl+Shift+S` 名前を付けて保存、`Ctrl+O` 開く、`O` プロジェクトウィンドウ、`P` カラーウィンドウ

---

## ✦ 今後の予定

- [ ] アニメーションタイムライン
- [ ] GIF / スプライトシートの書き出し
- [ ] プロジェクトウィンドウのフォルダ
- [ ] 保存せずに閉じるときの警告
- [ ] 日本語などラテン文字以外のファイル名表示（UI フォント）

---

## ✦ ライセンス

TinyPoly は [MIT ライセンス](../LICENSE)で公開しています。

使用しているライブラリ:

| ライブラリ | 用途 | ライセンス | 同梱方法 |
|---|---|---|---|
| [raylib](https://github.com/raysan5/raylib) 5.5 | ウィンドウ、入力、描画 | zlib | ビルド時に CMake が取得 |
| [microui](https://github.com/rxi/microui) 2.02 | UI | MIT | `src/microui/` に無改変で同梱 |

各ライブラリの著作権表示は [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) にあります。
