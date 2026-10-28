<div align="center">

# ◆ TinyPoly

**작고 가벼운 로우폴리 3D 모델러 + 도트 텍스처 페인터**

4분할 뷰에서 클릭·드래그로 메쉬를 만들고, 128×128 16색 텍스처를 직접 그려서<br>
OBJ / glTF / GLB로 내보냅니다.

![version](https://img.shields.io/badge/version-0.4.5-f8c040)
![language](https://img.shields.io/badge/C23-raylib%205.5-3ca0fa)
![platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-8474be)
![license](https://img.shields.io/badge/license-MIT-50d250)

![모델링 모드](docs/model.png)

</div>

> picoCAD 2의 작업 방식(4분할 뷰, 클릭 중심 조작, 별도 텍스처 모드)에서 영감을 받아 처음부터 새로 만든 프로젝트입니다.
> picoCAD와는 관련이 없으며 코드나 에셋을 가져오지 않았습니다.

---

## ✦ 기능

### 모델링
- **4분할 뷰** — top / 3d / front / side. `Space`로 마우스 아래 뷰를 전체화면 전환
- **기본 도형 7종** — plane, cube, prism, pyramid, cylinder, cone, sphere (빈 곳 우클릭 → 보고 있는 뷰를 향해 생성)
- **클릭·드래그 편집** — 메쉬/정점 드래그로 이동, `R`+드래그 회전, `T`+드래그 크기, 드래그 중 `Shift`로 중심점 이동, `Ctrl`로 그리드 스냅 반전
- **면 편집** — 돌출(`E`), 삭제(`X`), 미러, 복제, 정점 병합·스냅
- **면 속성(`F`)** — 색, 양면, 음영, 텍스처, 뒤에 그리기
- **프로젝트 목록(`O`)** — 숨김, 잠금, 이름 변경
- 팔레트 램프 기반 음영, 투명색 처리, 텍스처/단색/와이어 렌더(`M`), 음영 전환(`L`)

### 텍스처
![텍스처 모드](docs/texture.png)

- **128×128, 16색** 텍스처 (한 색은 투명)
- **도구** — UV(`V`), 펜(`B`), 스포이드(`I`), 채우기(`G`), 선, 사각형, 원, 영역 선택
- **UV 편집** — 드래그, 꼭짓점 이동, 회전(`R`), 뒤집기(`H`), 크기(`T`+드래그), 복사/붙여넣기
- **팔레트 편집(`P`)** — RGB, 음영 램프, 자동 음영, 색 교환, 그라데이션, 배경색/투명색 지정
- PNG 가져오기 (16색 초과 시 팔레트 색으로 자동 축소)

### 파일
![파일 탐색 창](docs/file-browser.png)

- 앱 안의 파일 탐색 창 — 모든 OS에서 같은 모양과 조작
- 자체 포맷 `.tpm` (텍스트, 메쉬 + UV + 텍스처 + 팔레트)
- **내보내기**
  | 형식 | 결과물 |
  |---|---|
  | OBJ | `.obj` + `.mtl` + `.png` |
  | glTF | `.gltf` + `.bin` + `.png` |
  | GLB | `.glb` (텍스처 포함) |
  | 텍스처 | `.png` |
- `.tpm` / `.png` 파일을 창에 끌어다 놓아도 열림

### 도트 커서
![커서](docs/cursors.png)

상황에 따라 바뀌는 도트 커서 — 연필 심·페인트 통은 현재 색, 스포이드 액체는 커서 아래 색을 보여줍니다.

---

## ✦ 빌드

### 준비물
| | |
|---|---|
| CMake | 3.25 이상 |
| Ninja | 프리셋이 `Ninja Multi-Config` 사용 |
| C 컴파일러 | C23 지원 (GCC 13+, Clang 17+) |
| raylib 5.5 | 설치돼 있으면 사용, 없으면 CMake가 자동으로 받아서 빌드 |

<details>
<summary><b>Linux</b>: raylib 빌드에 필요한 패키지</summary>

```sh
# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
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

실행 파일은 `build/<OS>/bin/Release/TinyPoly` 에 생깁니다.

---

## ✦ 조작 요약

| 모델링 | | 텍스처 | |
|---|---|---|---|
| 좌클릭/드래그 | 선택·이동 | `V` `B` `I` `G` | UV·펜·스포이드·채우기 |
| `R` / `T` + 드래그 | 회전 / 크기 | `Ctrl`+클릭 | 색 집기 |
| 우클릭 | 메뉴 | 우클릭 | 보조색 (UV 도구는 메뉴) |
| 휠 / 가운데 드래그 | 줌 / 화면 이동 | `Space`·가운데 드래그 | 화면 이동 |
| `E` `X` `F` `G` | 돌출·면 삭제·면/메쉬 속성 | `R` `H` / `T`+드래그 | UV 회전·뒤집기 / 크기 |
| 방향키·WASD | 이동 (`Ctrl` 미세) | `1`–`4` | 확대 |
| `Tab` | 텍스처 모드 | `Tab` | 모델링 모드 |

공통: `Ctrl+Z/Y` 되돌리기·다시, `Ctrl+S` 저장, `Ctrl+Shift+S` 다른 이름, `Ctrl+O` 열기, `O` 프로젝트, `P` 팔레트

---

## ✦ 앞으로

- [ ] 애니메이션 타임라인
- [ ] GIF / 스프라이트시트 내보내기
- [ ] 프로젝트 목록 폴더
- [ ] 저장 안 하고 닫을 때 경고
- [ ] 한글 파일 이름 표시 (UI 폰트)

---

## ✦ 라이선스

TinyPoly는 [MIT 라이선스](LICENSE)입니다.

사용하는 라이브러리:

| 라이브러리 | 용도 | 라이선스 | 포함 방식 |
|---|---|---|---|
| [raylib](https://github.com/raysan5/raylib) 5.5 | 창, 입력, 렌더링 | zlib | 빌드 시 CMake가 받아옴 |
| [microui](https://github.com/rxi/microui) 2.02 | UI | MIT | `src/microui/`에 수정 없이 포함 |

각 라이브러리의 저작권 고지는 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)에 있습니다.
