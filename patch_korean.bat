@echo off
cd /d "%~dp0"
echo.
echo ======================================
echo  원더보이2 한글 패치 생성 도구
echo ======================================
echo.

echo [1/3] IPS 패치 적용 중 (mkbootleg.py)...
python tools\mkbootleg.py
if errorlevel 1 (
    echo.
    echo 오류: mkbootleg.py 실패. Python 3 설치 여부와 roms\wbml\ 폴더를 확인하세요.
    pause & exit /b 1
)
echo.

echo [2/3] 타일 매핑 정렬 중 (kralign.py)...
python tools\kralign.py
if errorlevel 1 (
    echo.
    echo 오류: kralign.py 실패.
    pause & exit /b 1
)
echo.

echo [3/3] 문자열 재인코딩 + krtext.h 생성 중 (krencode.py)...
python tools\krencode.py
if errorlevel 1 (
    echo.
    echo 오류: krencode.py 실패.
    pause & exit /b 1
)
echo.

echo [빌드] 에뮬레이터 재빌드 중...
cmake --build build_cmake
if errorlevel 1 (
    echo.
    echo 오류: 빌드 실패. build_cmake\ 폴더가 있는지 확인하세요.
    echo (없으면: cmake -B build_cmake -S . 먼저 실행)
    pause & exit /b 1
)
echo.
echo ======================================
echo  완료! roms\kr\ 폴더가 생성됐습니다.
echo  flydo.exe 실행 후 메뉴에서 'kr' 선택.
echo ======================================
echo.
pause
