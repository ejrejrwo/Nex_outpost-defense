# 에셋 출처와 사용 범위

## Native 2.1.0 시각 패스

현재 플레이 화면은 Unreal Canvas와 native C++ 렌더러가 그린다. 실제 게임플레이 규칙은 기존과 같이 30초 준비, 3개 웨이브, 총 30명의 적 구성을 사용하며, 시각 에셋 교체가 지형 충돌·경로·입력·승패 규칙을 바꾸지 않는다.

새 시각 에셋은 `SourceArt/DesignV3/`에 보관한다.

| 파일 | 용도와 출처 |
| --- | --- |
| `sprites_atlas.png` | Blender에서 만든 8×8 방향별 스프라이트 아틀라스 |
| `sprites_atlas.json` | 아틀라스 셀, UV, 앵커, 카메라와 필터링 메타데이터 |
| `outpost_design_v3.blend` | 원본 절차적 geometry와 렌더 장면 |
| `arena_floor.png` | ImageGen으로 만든 금속 바닥 이미지 |
| `key_art.png` | ImageGen으로 만든 메뉴 타이틀 아트 |
| `imagegen-prompts.json` | 두 ImageGen 이미지의 프롬프트와 생성 기록 |

아틀라스는 `Tools/blender_outpost_art.py`를 Blender 4.5.9에서 실행해 만든다. 원본 geometry는 절차적으로 생성하며, 68° 고각 직교 카메라로 렌더링한다. 방향별 캐릭터 셀과 소품 셀의 정확한 배치는 `sprites_atlas.json`을 따른다. 아틀라스의 `generator`와 `turret` 셀은 미래 디자인 에셋이며 현재 기능성 게임 오브젝트가 아니다.

프로젝트 루트에서 Blender portable 실행 파일의 실제 경로를 넣어 다음처럼 재생성한다.

```powershell
& "C:\path\to\Blender\blender.exe" --background --python Tools/blender_outpost_art.py -- --output-dir SourceArt/DesignV3 --atlas-size 2048 --blend-name outpost_design_v3.blend
```

`Tools/import_design_v3.py`는 세 PNG를 `/Game/Outpost/DesignV3`에 추가로 import한다. `AOutpostGameMode`는 `ArtSprites`, `ArtFloor`, `ArtKeyArt` `UPROPERTY`로 텍스처를 보유한다. 에셋이 없거나 로드되지 않으면 렌더러는 기존 절차적 표현으로 돌아간다. 새 파이프라인에서는 레거시 import 스크립트를 실행할 필요가 없다.

생성된 세 PNG를 Unreal Editor-Cmd에서 import할 때는 프로젝트 루트에서 다음처럼 실행한다.

```powershell
& "C:\path\to\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\JD_GameProject\Outpost2D\Outpost2D.uproject" -run=pythonscript -script="C:\path\to\JD_GameProject\Outpost2D\Tools\import_design_v3.py" -unattended -nullrhi
```

Blender portable runtime과 실행 중 생성되는 `Saved` 내용은 Git과 export 대상에서 제외한다. 사용한 Blender runtime은 공식 서버에서 받은 파일의 SHA를 확인한 것이다. 이 작업에서 Blender MCP 연결이나 Higgsfield 사용을 주장하지 않는다.

## 레거시 이미지

`SourceArt/arena.png`, `SourceArt/sprites.png`, 기존 `/Game/Outpost/Art` 리소스와 `Tools/import_assets.py`는 이전 아트 파이프라인의 보존 자료다. 이 자료는 DesignV3 경로와 별도로 유지하며, 새 시각 패스의 필수 단계로 취급하지 않는다.

## 사운드

`Tools/generate_audio.py`는 네트워크와 외부 라이브러리 없이 Python 표준 라이브러리로 `SourceArt/sounds/`의 mono PCM16 WAV를 만든다. Unreal import 후 `/Game/Outpost/Audio`의 대응 `USoundWave`를 게임 이벤트에서 재생한다. 시각 패스는 사운드와 게임 규칙을 변경하지 않는다.

## 엔진 기본 리소스와 배포

Canvas와 Unreal Engine 기본 리소스의 사용·재배포는 Unreal Engine 5.8 및 Epic Games 라이선스 조건을 따른다. 프로젝트에 별도로 내려받은 서드파티 이미지·폰트·음원은 없다. 소스 아카이브에는 재현에 필요한 소스, 에셋, 도구와 문서를 포함하고 `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`, `Release` 같은 생성 산출물은 제외한다. 압축 생성 규칙은 `Tools/export_portfolio.ps1`에 있다.

## 범위 기록

ImageGen 결과와 프롬프트는 AI 보조 자료로 기록한다. 원본 게임의 캐릭터·맵·모델을 사용하지 않는다. 성장·타워·추가 공세에 관한 내용은 [방어 확장 설계안](BUILD_DEFENSE_PLAN.md)에 있으며 현재 구현으로 소개하지 않는다.
