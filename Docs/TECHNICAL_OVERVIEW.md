# 기술 개요

## 프로젝트 경계

`Outpost2D.uproject`는 Unreal Engine 5.8 native C++ 프로젝트다. 규칙은 `Source/Outpost2D/OutpostSimulation.*`의 `Outpost::FSimulation`, Unreal 연결은 `AOutpostGameMode`, 아레나 렌더링은 `FOutpostArenaRenderer`, HUD와 입력은 각각 `AOutpostHUD`와 `AOutpostPlayerController`가 담당한다. 기존 웹 버전은 초기 프로토타입 참고 자료이며 native 런타임과 코드를 공유하지 않는다.

`Config/DefaultEngine.ini`는 `/Game/Outpost/Maps/OutpostArena`를 기본 맵으로 지정한다. 한 화면 직교 아레나와 Unreal Canvas를 유지한다. 현재 문서의 native 2.1.0 시각 패스는 아트 연결을 다루며, 30초 준비·3개 웨이브·총 30명 적이라는 실제 게임플레이 구성을 바꾸지 않는다.

## 시각 에셋 연결

`AOutpostGameMode`는 다음 텍스처를 `UPROPERTY`로 보유하고 BeginPlay에서 `/Game/Outpost/DesignV3` 경로로 로드한다.

| 속성 | 에셋 |
| --- | --- |
| `ArtSprites` | `T_SpritesAtlas` — Blender 절차적 geometry를 68° 직교 카메라로 렌더링한 8×8 방향별 아틀라스 |
| `ArtFloor` | `T_ArenaFloor` — ImageGen 금속 바닥 |
| `ArtKeyArt` | `T_KeyArt` — ImageGen 메뉴 타이틀 아트 |

`FOutpostArenaRenderer`와 HUD는 모두 Unreal Canvas에서 텍스처를 그린다. 아틀라스 방향·셀·앵커는 `SourceArt/DesignV3/sprites_atlas.json`을 따른다. 월드 오브젝트 레이어는 ground Y를 깊이로 안정 정렬한다. 바닥과 타이틀 아트가 로드되지 않거나 아틀라스가 없으면 해당 부분은 기존 절차적 Canvas 표현으로 폴백한다.

아틀라스의 `generator`와 `turret` 셀은 미래 디자인 에셋이다. 현재 기능성 생성기나 포탑 게임 오브젝트, 충돌체, 규칙으로 연결하지 않는다. 지형 충돌과 적 경로 검사는 `FSimulation`의 기존 고정 격자 규칙을 계속 사용한다.

새 PNG import는 `Tools/import_design_v3.py`가 `/Game/Outpost/DesignV3`에 추가로 수행한다. 기존 `SourceArt/arena.png`, `SourceArt/sprites.png`, `/Game/Outpost/Art`, `Tools/import_assets.py`는 레거시 자료로 남아 있으며 DesignV3 파이프라인에서 실행할 대상이 아니다.

## 고정 간격 시뮬레이션

`FSimulation`은 Unreal Actor와 Canvas를 참조하지 않는 값 중심 규칙 계층이다. 플레이어, 광석, 방벽, 적, 탄환, 강화 작업, 통계와 이벤트를 보관한다. GameMode는 외부 `DeltaSeconds`를 누적하고 `1/60`초 간격으로 `Sim.Tick`을 호출한 뒤 이벤트를 사운드와 화면 효과로 전달한다. 렌더러는 이 상태를 읽어 그리며 규칙을 소유하지 않는다.

입구 방어는 28×17 고정 격자와 상·하·좌·우 BFS를 사용한다. 우클릭 이동, 적 추적, 방벽 변경 후 재경로 탐색은 같은 격자 규칙을 공유한다. 방벽 배치는 경계·기존 물체·플레이어·살아 있는 적과의 겹침, 광석·대장간 접근, 양쪽 입구에서 돌아오는 경로를 검사한다. 이 검사는 시각 에셋과 독립적이다.

탄환은 이전 위치에서 다음 위치까지 선분을 검사하고 낮은 방벽은 통과한다. 회피는 고정 틱 동안 충돌을 나누어 검사한다. 저장 기록은 `UOutpostSaveGame`의 `OutpostNative2` 슬롯에 hard/practice를 별도 보관한다.

## 현재 게임플레이와 검증 범위

실제 게임플레이는 30초 준비 뒤 세 웨이브에서 총 30명의 적을 상대하고, Canvas HUD에서 도구·채굴·강화·방벽·승패 상태를 표시한다. 아트 import와 렌더링 연결은 이 규칙을 변경하지 않는다. native 2.1.0의 최종 C++ 테스트 13개와 Windows 패키징이 성공했다. 배포본에서 도전 모드의 방벽 빌드와 연습 모드의 화력 빌드를 보스까지 자동 완주하고 세 아트 에셋 로드를 확인했다. 실제 결과와 검증 한계는 [v2.1 검증 기록](VALIDATION_2.1.md)에 있다.

성장·타워·추가 공세·경제 확장은 [방어 확장 설계안](BUILD_DEFENSE_PLAN.md)의 구현 전 제안이다. 해당 문서의 수치와 단계는 현재 게임 기능으로 해석하지 않는다.

## AI와 도구 사용 범위

Blender 4.5.9 portable runtime과 `Tools/blender_outpost_art.py`는 아틀라스 geometry와 렌더 장면을 만든다. ImageGen은 `arena_floor.png`와 `key_art.png` 및 관련 프롬프트를 만들었고, `SourceArt/DesignV3/imagegen-prompts.json`에 기록을 보존한다. Blender runtime의 출처 파일은 공식 서버에서 받아 SHA를 확인했으며, 실행 중 `Saved` 산출물은 Git과 export에서 제외한다. 이 작업에서 Blender MCP 연결이나 Higgsfield 사용을 주장하지 않는다.
