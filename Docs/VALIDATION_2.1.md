# Native 2.1.0 그래픽 업데이트 검증

2026-09-08 · Unreal Engine 5.8.0 · Windows 11 · DX11/SM5 · 1440×960.

최종 그래픽 변경을 포함한 C++ 빌드와 Windows Development 패키징(`BuildCookRun`)이 성공했습니다. 아래 결과는 `Release/Windows/Outpost2D/Binaries/Win64/Outpost2D.exe`를 실행한 기록입니다. 테스트별 결과와 배포 파일 SHA-256은 [validation-summary-2.1.json](validation-summary-2.1.json)에 있습니다.

## 배포본 자동 플레이

| 모드 / 준비 빌드 | 채굴 | 방벽 이동 | 처치 | 남은 HP | 총 진행 시간 | 입력 검사 | 새 아트 로드 |
|---|---:|---:|---:|---:|---:|---|---|
| Hard / 화력 1단계 + 방벽 | 10 | 1 | 30 / 30 | 21 | 80.67초 | 13 / 13 | 3 / 3 |
| Practice / 화력 2단계 | 24 | 0 | 30 / 30 | 100 | 68.05초 | 13 / 13 | 3 / 3 |

두 실행 모두 실제 준비 시간을 사용해 이동·채굴·5초 강화 후 전투에 진입하고 최종 보스를 격퇴했습니다. 완주 중 체력·자원·무기 단계의 강제 지급은 없습니다. 자동 조종기가 조준·이동·회피를 결정하므로 이 결과를 사람의 승률이나 재미 평가로 해석하지 않습니다. 기존 30초 준비·3공세·총 30명의 게임 규칙은 변경하지 않았습니다.

## 아트와 실제 화면

Blender 4.5.9에서 원본 geometry와 8방향 스프라이트를 만들고, 내장 이미지 생성기로 바닥과 타이틀 아트를 만들었습니다. 원본·재생성 스크립트·프롬프트는 [에셋 기록](ASSETS.md)에 연결되어 있습니다.

- 최종 2048×2048 RGBA 아틀라스: 8×8의 64개 셀이 모두 비어 있지 않으며, 셀 경계에 최소 25px의 투명 여백이 있습니다.
- 세 PNG를 `/Game/Outpost/DesignV3`에 import했고 배포본에서 `artAssetsLoaded=true`를 확인했습니다.
- 메뉴 일러스트, 금속 바닥, 캐릭터·괴물·광석·대장간·방벽, 지면 Y에 따른 깊이 정렬을 실제 GPU 캡처로 확인했습니다.
- 캐릭터·광석의 크기와 바닥 대비를 조정하고, 평소 플레이어 머리 위의 중복 배지를 제거했습니다.

[메뉴](Screenshots/v2.1/menu.png), [준비](Screenshots/v2.1/prep.png), [방벽 배치](Screenshots/v2.1/build.png), [전투](Screenshots/v2.1/combat.png), [보스](Screenshots/v2.1/boss.png), [결과](Screenshots/v2.1/win.png)는 모두 최종 Windows 배포본의 `FScreenshotRequest` 캡처입니다. 컨셉 이미지나 웹 화면이 아닙니다. 포탑·발전기 스프라이트는 향후 기능을 위한 에셋이며 현재 전투 기능으로 연결하지 않았습니다.

## C++ 및 입력 검증

Unreal Automation Framework: **13개 성공, 실패 0, 미실행 0**. 기존 준비·강화·방벽 배치·경로·탄환·일시정지·전투 중 작업·회피·보스·저장 테스트가 최종 변경 후 모두 통과했습니다. 각 테스트 이름과 결과를 JSON에 보존했습니다.

배포본 입력 검사는 HUD 시작, 숫자키 1/2/3, D 이동, Esc 정지·재개, 화면↔월드 좌표 변환, F 보호구 강화, E 취소 환불, 전투 중 도구 전환과 Space 회피를 포함합니다. 상황을 구성하는 입력 검사 이후 상태를 초기화하고 별도의 정상 완주를 시작합니다.

입력은 Unreal 키 이벤트와 HUD 콜백을 주입한 방식입니다. OS 키보드·마우스를 통한 수동 완주와 사운드 청취는 이번 검증에 포함하지 않습니다. 자동 실행은 `-nosound`를 사용하며 실제 사용자 SaveGame 파일을 덮어쓰지 않습니다.

## 재현

```powershell
.\Tools\build.ps1 -Action Test
.\Tools\build.ps1 -Action Package
.\Tools\validate_package.ps1 -Mode hard -Rank 1
.\Tools\validate_package.ps1 -Mode practice -Rank 2
```

검증 도구는 종료 코드와 새 보고서 시각, 완주, 처치 수, 채굴량, 빌드별 방벽 이동, 입력 실패 수와 새 아트 로드를 검사합니다. 이전 버전 결과는 [v2.0.0 검증 기록](VALIDATION.md)에 별도로 보존했습니다.
