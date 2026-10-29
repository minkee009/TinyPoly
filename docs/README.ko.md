<div align="center">

[English](../README.md) | **한국어**

<h1><img src="icon.png" width="64" align="absmiddle" alt="TinyPoly 아이콘"> TinyPoly</h1>

**작고 가벼운 로우폴리 3D 모델러 + 도트 텍스처 페인터**

4분할 뷰에서 클릭·드래그로 메쉬를 만들고, 최대 1024×1024 텍스처를 직접 그려서<br>
OBJ / glTF / GLB로 내보냅니다.

![version](https://img.shields.io/badge/version-0.5.4-f8c040)
![language](https://img.shields.io/badge/C23-raylib%205.5-3ca0fa)
![platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-8474be)
![license](https://img.shields.io/badge/license-MIT-50d250)

![모델링 모드](model.png)

</div>

---

## ✦ 기능

### 모델링
- **4분할 뷰** — top / 3d / front / side. `Space`로 마우스 아래 뷰를 전체화면 전환
- **기본 도형 7종** — plane, cube, prism, pyramid, cylinder, cone, sphere. 빈 곳을 우클릭하면 그 뷰를 향해 생성, 또는 상단바 `+` 버튼
- **클릭·드래그 편집** — 메쉬/정점 드래그로 이동, `R`+드래그 회전, `T`+드래그 크기, 드래그 중 `Shift`로 원점 이동, `Ctrl`로 그리드 스냅 반전
- **박스 선택** — 2D 뷰는 빈 곳에서 드래그, 3D 뷰는 `Ctrl`+드래그 (`Shift`로 추가 선택)
- **편집** — 면 돌출, 면/정점 삭제, 정점 병합·스냅, 미러, 복제, 메쉬 복사/붙여넣기
- **선택 막대** — 면이나 정점, 또는 메쉬를 고르면 뷰 아래쪽에 작은 막대가 나타나서 무엇을 골랐는지와 쓸 수 있는 동작을 보여줌
  - 면: 돌출, 삭제 │ 면 색, 옵션
  - 정점: 병합, 스냅, 삭제 (병합은 정점을 2개 이상 골라야 켜짐)
  - 메쉬: 원점 가운데로, 복제, 삭제
- **우클릭 메뉴** — 커서 아래에 있는 것(정점 / 면 / 메쉬 / 빈 곳)에 따라 메뉴가 달라지고, 메뉴 맨 위 제목으로 어떤 메뉴인지 표시
- **면 속성(`F`)** — 색, 양면, 음영, 텍스처, 뒤에 그리기
- **프로젝트 창(`O`)** — 줄마다 숨김(눈), 잠금(자물쇠), 이름 변경(더블클릭), 삭제(휴지통), `+`로 메쉬 추가, 이름을 우클릭하면 메쉬 메뉴(이름 변경, 원점 가운데로, 미러, 복제, 복사, 삭제)
- 3단계 플랫 음영, 투명 픽셀 처리, 텍스처/단색/와이어 렌더(`M`), 음영 전환(`L`)

### 텍스처
![텍스처 모드](texture.png)

- **RGBA 텍스처, 128 ~ 1024** — 도구줄의 크기 버튼으로 변경 (왼쪽 위 픽셀은 유지)
- **도구** — UV(`V`), 펜(`B`), 스포이드(`I`), 채우기(`G`), 선, 사각형, 원, 영역 선택. 사각형/원은 한 번 더 누르면 채워진 모양
- **UV 편집** — 드래그, 꼭짓점 이동, 회전(`R`), 뒤집기(`H`), 크기(`T`+드래그), 방향키로 한 칸씩 이동, 복사/붙여넣기
- **영역 선택** — 이동, 복사/붙여넣기(`Ctrl+C/V`), 삭제(`Del`)
- **색 선택 창(`P`)** — 채도/명도 사각형, 색상 막대, 알파, 16진수 입력. 좌클릭 색 / 우클릭 색 따로 (`X`로 교환)
- **팔레트** — 자주 쓰는 색 모음 (최대 256칸). 추가·덮어쓰기·삭제, 맨 앞 칸은 투명 (지우개)
- 확대 12.5% ~ 3200%, 창에 맞춤 (`1` 맞춤, `2` 100%, `3` 200%, `4` 400%)
- PNG 가져오기 (최대 1024×1024, 크기 그대로)

### 파일
![파일 탐색 창](file-browser.png)

- 앱 안의 파일 탐색 창 — 모든 OS에서 같은 모양과 조작
- 자체 포맷 `.tpm` (텍스트, 메쉬 + UV + 텍스처(PNG) + 팔레트). 0.4.x의 16색 파일도 열림
- `.tpm` / `.png` 파일을 창에 끌어다 놓아도 열림

### 내보내기
![내보내기 창](export.png)

File → **export...** 를 누르면 내보내기 창이 뜹니다. 3D 형식과 저장 방식(폴더 / zip)을 고르면 어떤 파일이 만들어지는지 미리 보여줍니다.

| 형식 | 폴더 또는 zip 안에 들어가는 파일 |
|---|---|
| OBJ + MTL | `이름.obj` + `이름.mtl` + `이름.png` |
| glTF | `이름.gltf` + `이름.bin` + `이름.png` |
| GLB | `이름.glb` (텍스처 포함) |

텍스처를 쓰지 않는 단색 면의 색은 텍스처 PNG 아래쪽 색 띠에 함께 들어가서, 어느 엔진에서 열어도 같은 색으로 보입니다.

### 화면
- 모든 버튼이 도트 아이콘 — 마우스를 잠시 올리면 이름과 단축키가 툴팁으로 표시
- 도트 느낌으로 둥글린 창, 제목줄 오른쪽 닫기 버튼, 크기 조절 되는 창은 오른쪽 아래 손잡이
- 저장·오류 같은 알림은 왼쪽 아래에 잠깐 떴다 사라짐
- 글자 입력칸 — 깜빡이는 caret, `←` `→` `Home` `End`로 이동(`Shift`면 선택), 드래그로 선택, 더블클릭은 단어·세 번 클릭은 전체 선택, `Ctrl+A/C/X/V`

### 도트 커서
![커서](cursors.png)

상황에 따라 바뀌는 도트 커서 — 연필 심·페인트 통은 현재 색, 스포이드 액체는 커서 아래 색을 보여줍니다.

---

## ✦ 빌드

### 준비물
| 도구 | 버전 / 설명 |
|---|---|
| CMake | 3.25 이상 |
| Ninja | 프리셋이 `Ninja Multi-Config` 사용 |
| C 컴파일러 | C23 지원 (GCC 13+, Clang 17+) |
| raylib 5.5 | 설치돼 있으면 사용, 없으면 CMake가 자동으로 받아서 빌드 |
| Git | raylib을 자동으로 받을 때 필요 |

<details>
<summary><b>Windows</b>: MinGW-w64 (GCC)</summary>

[Scoop](https://scoop.sh)으로 설치하는 방법 (PowerShell):

```powershell
scoop install git cmake ninja mingw
```

또는 winget:

```powershell
winget install Git.Git Kitware.CMake Ninja-build.Ninja BrechtSanders.WinLibs.POSIX.UCRT
```

설치 후 새 터미널에서 `gcc --version`이 13 이상인지 확인하고, 아래 [명령](#명령)을 그대로 실행하면 됩니다.
실행 파일은 `build\Windows\bin\Release\TinyPoly.exe`에 생깁니다.

> MSVC(Visual Studio)는 아직 테스트하지 않았습니다. MinGW GCC로 빌드해 주세요.
</details>

<details>
<summary><b>Linux</b>: raylib 빌드에 필요한 패키지</summary>

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

### 명령
```sh
cmake --preset default
cmake --build --preset release      # 또는 debug
```

실행 파일은 `build/<OS>/bin/Release/TinyPoly` 에 생깁니다 (`<OS>`는 `Windows`, `Darwin`, `Linux`).

### VS Code
`.vscode` 설정이 들어 있어서 **F5**로 빌드 + 디버그 실행이 됩니다. **실행 및 디버그**에서 OS에 맞는 항목을 고르세요.
디버깅에는 Microsoft의 **C/C++ 확장**(`ms-vscode.cpptools`)이 필요합니다. 폴더를 열면 VS Code가 설치를 권합니다.

---

## ✦ 조작

### 모델링
| 입력 | 동작 |
|---|---|
| 클릭 / 드래그 | 선택, 이동 |
| 빈 곳 드래그 (2D) / `Ctrl`+드래그 (3D) | 박스 선택 (`Shift`로 추가) |
| `R` / `T` + 드래그 | 회전 / 크기 |
| 드래그 중 `Shift` | 원점 이동 |
| 우클릭 | 메뉴 |
| 우클릭 드래그 | 회전 (3D) / 화면 이동 (2D) |
| 휠 / 가운데 드래그 | 줌 / 화면 이동 |
| `E` / `X` / `Del` | 돌출 / 면 삭제 / 정점 삭제 |
| `F` / `G` | 면 / 메쉬 속성 |
| `Ctrl+C` / `Ctrl+V` / `Ctrl+X` | 메쉬 복사 / 붙여넣기 / 삭제 |
| 방향키, WASD | 선택한 것을 그리드 한 칸 이동 (`Ctrl`: ¼칸) |
| `Space` | 마우스 아래 뷰 전체화면 |
| `M` / `L` | 렌더 방식 / 음영 |
| `Tab` | 텍스처 모드 |

### 텍스처
| 입력 | 동작 |
|---|---|
| `V` `B` `I` `G` | UV, 펜, 스포이드, 채우기 |
| `Ctrl`+클릭 | 색 집기 |
| 우클릭 | 보조색으로 그리기 (UV 도구는 메뉴) |
| `X` | 좌·우클릭 색 교환 |
| `R` / `H` / `T`+드래그 | UV 회전 / 뒤집기 / 크기 |
| 방향키, WASD | UV 한 픽셀 이동 |
| `Ctrl+C` / `Ctrl+V` / `Del` | 복사 / 붙여넣기 / 삭제 (UV 또는 선택 영역) |
| `Space`+드래그, 가운데 드래그 | 화면 이동 |
| 휠 / `1`–`4` | 확대 / 맞춤·100·200·400% |
| `Tab` | 모델링 모드 |

**공통:** `Ctrl+Z/Y` 되돌리기·다시, `Ctrl+S` 저장, `Ctrl+Shift+S` 다른 이름으로, `Ctrl+O` 열기, `O` 프로젝트 창, `P` 색 선택 창

---

## ✦ 앞으로

- [ ] 애니메이션 타임라인
- [ ] GIF / 스프라이트시트 내보내기
- [ ] 프로젝트 창 폴더
- [ ] 저장 안 하고 닫을 때 경고
- [ ] 한글 파일 이름 표시 (UI 폰트)

---

## ✦ 라이선스

TinyPoly는 [MIT 라이선스](../LICENSE)입니다.

사용하는 라이브러리:

| 라이브러리 | 용도 | 라이선스 | 포함 방식 |
|---|---|---|---|
| [raylib](https://github.com/raysan5/raylib) 5.5 | 창, 입력, 렌더링 | zlib | 빌드 시 CMake가 받아옴 |
| [microui](https://github.com/rxi/microui) 2.02 | UI | MIT | `src/microui/`에 수정 없이 포함 |

각 라이브러리의 저작권 고지는 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)에 있습니다.
