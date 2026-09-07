# 에셋 출처와 사용 범위

## 런타임 표현

현재 게임 화면은 C++에서 절차적으로 구성한 아레나 표현과 `AHUD::DrawHUD`의 Unreal Canvas HUD를 사용합니다. `SourceArt/arena.png`, `SourceArt/sprites.png`와 이를 import한 `/Game/Outpost/Art/T_Arena`, `T_Sprites`, `M_Floor`, `M_Sprite`는 이전 아트 파이프라인의 레거시 자료이며 현재 런타임의 주 화면을 그리는 데 사용하지 않습니다.

`SourceArt/prompts.json`에는 ImageGen에 사용한 원본 프롬프트와 생성기 표기를 보관합니다. 두 PNG는 내 손그림이나 외부 스톡 에셋이라고 주장하지 않으며, built-in ImageGen으로 생성된 AI 보조 자료입니다.

## 사운드

`Tools/generate_audio.py`는 네트워크와 외부 라이브러리 없이 Python 표준 라이브러리 `wave`, `math`, `struct`, `random`만 사용해 짧은 mono PCM16 WAV를 만듭니다. `SourceArt/sounds/`의 `S_shot`, `S_mine`, `S_forge`, `S_upgrade`, `S_lift`, `S_place`, `S_hit`, `S_hurt`, `S_wave`, `S_win`, `S_lose`는 다운로드 샘플이 아닌 로컬 프로시저럴 생성물입니다. Unreal import 후 `/Game/Outpost/Audio`의 대응 `USoundWave`를 게임 이벤트에서 재생합니다.

## 엔진 기본 리소스

게임 월드의 절차적 표현에는 Unreal Engine 기본 리소스와 Canvas API가 사용될 수 있습니다. 엔진 제공 리소스의 재배포와 사용은 Unreal Engine 5.8 및 Epic Games 라이선스 조건을 따릅니다. Windows 패키지에 포함된 `NOTICES.txt`도 함께 보존합니다.

프로젝트에 별도로 내려받은 서드파티 이미지·폰트·음원은 없습니다. 소스 아카이브에는 재현에 필요한 `SourceArt`, C++ `Source`, `Content`, 설정, 도구와 문서를 포함하고, `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`, `Release` 같은 생성 산출물은 제외합니다. 압축 생성 규칙은 `Tools/export_portfolio.ps1`에 있습니다.

## 재생성

```powershell
python Tools/generate_audio.py
```

PNG를 다시 import하거나 머티리얼을 갱신할 때는 Unreal Editor Python 환경에서 `Tools/import_assets.py`를 실행합니다. 이 단계는 Editor가 필요하며, 현재 C++ 절차적 렌더링 경로의 필수 실행 단계는 아닙니다.
