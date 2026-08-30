@echo off
rem ===========================================================================
rem  ACAH - build something you can actually send someone.
rem
rem    package.bat              build Release, then produce dist\ACAH.exe
rem    package.bat nobuild      package whatever is already built
rem
rem  OUTPUT
rem    dist\ACAH.exe   the game, one file. This is the thing to send. It needs
rem                    no folder, no installer, no runtime, no assets.
rem    dist\ACAH.zip   the same exe with a README, for when you want to send
rem                    the notes as well.
rem
rem  The game ships no assets at all - the world, the fonts, the music and
rem  every sound are generated in code - so "everything needed to run it" is
rem  the executable, PROVIDED the executable does not import anything that
rem  only exists on this machine. That last part is the whole reason this
rem  script checks rather than assumes: a GCC/MSYS2 build links against
rem  libstdc++-6.dll, libgcc_s_seh-1.dll and libwinpthread-1.dll by default,
rem  those DLLs live in the toolchain's bin directory, and the exe therefore
rem  runs perfectly here and fails on every other machine with a missing-DLL
rem  error. CMakeLists now links them statically; this script verifies it,
rem  and if anything non-system is still imported it copies the DLL next to
rem  the exe and tells you the single file is not enough this time.
rem ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "MODE=%~1"

rem --- build unless told not to -----------------------------------------------
if /i not "%MODE%"=="nobuild" (
    echo Building Release...
    call build.bat
    if errorlevel 1 (
        echo.
        echo   Build failed - nothing packaged.
        exit /b 1
    )
)

rem --- locate the executable --------------------------------------------------
set "EXE="
for %%P in (
    "build\bin\spiderbot.exe"
    "build\bin\Release\spiderbot.exe"
    "build\Release\spiderbot.exe"
    "build\spiderbot.exe"
) do if exist %%P if not defined EXE set "EXE=%%~P"

if not defined EXE (
    echo.
    echo   ERROR: could not find spiderbot.exe. Run build.bat first,
    echo   or check build\bin\ for where your generator put it.
    exit /b 1
)
echo Found executable: %EXE%

rem --- assemble the staging folder --------------------------------------------
set "OUT=dist\ACAH"
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "dist" 2>nul
mkdir "%OUT%" 2>nul

copy /y "%EXE%" "%OUT%\ACAH.exe" >nul
if errorlevel 1 (
    echo   ERROR: could not copy the executable.
    exit /b 1
)

rem --- what does this exe actually need? --------------------------------------
rem Read the import table and list every DLL that is NOT part of Windows. On a
rem correct static build this comes back empty and the exe is sendable on its
rem own. objdump ships with MinGW/MSYS2; if it is not on PATH we fall back to
rem a plain string scan, which catches the three runtimes that matter.
set "MISSINGDEP="
set "DEPLIST="
where objdump >nul 2>nul
if not errorlevel 1 (
    for /f "tokens=3" %%D in ('objdump -p "%OUT%\ACAH.exe" 2^>nul ^| findstr /i /c:"DLL Name:"') do (
        call :checkdep "%%D"
    )
) else (
    for %%D in (libstdc++-6.dll libgcc_s_seh-1.dll libgcc_s_dw2-1.dll libwinpthread-1.dll SDL3.dll) do (
        findstr /m /c:"%%D" "%OUT%\ACAH.exe" >nul 2>nul && call :checkdep "%%D"
    )
)

rem Copy anything it genuinely needs, from beside the exe or from the PATH.
if defined DEPLIST (
    echo.
    echo   This build imports non-system DLLs:%DEPLIST%
    for %%D in (%DEPLIST%) do (
        set "FOUND="
        if exist "build\bin\%%D" (
            copy /y "build\bin\%%D" "%OUT%\" >nul && set "FOUND=1"
        )
        if not defined FOUND (
            rem Search PATH for it. `for %%~$PATH:X` yields an empty string
            rem when nothing is found, so it has to go through a variable -
            rem testing %%~fF on an empty token expands to the current
            rem directory and would look like a hit.
            set "DEPPATH=%%~$PATH:D"
            if defined DEPPATH (
                copy /y "!DEPPATH!" "%OUT%\" >nul && set "FOUND=1"
            )
        )
        if defined FOUND ( echo     bundled %%D ) else (
            echo     COULD NOT FIND %%D - the package will not run elsewhere
            set "MISSINGDEP=1"
        )
    )
    echo   The single-file ACAH.exe will NOT be produced; send the zip instead.
) else (
    echo Import check: clean - this exe needs nothing but Windows.
)

rem Any DLLs the build dropped beside the exe anyway (a shared SDL build).
for %%D in ("%~dp0build\bin\*.dll") do copy /y "%%D" "%OUT%\" >nul 2>nul
for %%D in ("%~dp0build\bin\Release\*.dll") do copy /y "%%D" "%OUT%\" >nul 2>nul

rem --- the launcher -----------------------------------------------------------
> "%OUT%\PLAY.bat" (
    echo @echo off
    echo rem Launches ACAH. Edit the line below to change startup options:
    echo rem   --windowed        run in a window instead of fullscreen
    echo rem   --width N --height N   window size
    echo rem   --no-vsync        uncap the frame rate
    echo cd /d "%%~dp0"
    echo start "" "ACAH.exe"
)

rem --- the note that goes with it ---------------------------------------------
> "%OUT%\README.txt" (
    echo ACAH
    echo ====
    echo.
    echo A real-time spidertank game rendered entirely in coloured ASCII.
    echo You pilot a ten-metre six-legged walking tank through a campaign of
    echo corporate-war contracts: march the route, break the gun lines, wreck
    echo the objective, get paid, upgrade the machine, go again.
    echo.
    echo HOW TO RUN
    echo   Double-click PLAY.bat  ^(or run ACAH.exe directly^).
    echo   Windows may warn about an unrecognised app: More info -^> Run anyway.
    echo   Nothing installs. Delete the folder to uninstall.
    echo.
    echo CONTROLS
    echo   W A S D        move ^(relative to the camera^)
    echo   Mouse          aim the turret / swing the camera
    echo   Left mouse     fire weapon group 1
    echo   Right mouse    fire weapon group 2
    echo   1-4            switch a weapon mount on or off
    echo   5-8            move a mount between the left and right triggers
    echo   Space          HOLD to charge a jump, release to leap
    echo   Q E R F        the abilities your fitted parts provide
    echo   Z              gunsight ^(zoom^)
    echo   X              driver camera ^(first person^)
    echo   Enter          deploy / accept / fit a part
    echo   Backspace      open the workshop from a briefing / back out
    echo   Tab            workshop: compare with and without a part
    echo   N N            on a briefing, press twice to wipe the save
    echo   H              hide the HUD
    echo   M , .          mute / volume down / volume up
    echo   V P R B F      view and palette toggles ^(try V for the raw 3D view^)
    echo   F11            fullscreen
    echo   Esc            quit
    echo.
    echo CLIMBING
    echo   Drive into a wall and keep pushing: light leg sets walk straight up
    echo   buildings and over the parapet. Heavy legs cannot climb at all -
    echo   check a leg set's GRIP in the workshop before you buy it.
    echo.
    echo NOTES FOR TESTERS
    echo   - Dying ends the contract. You restart the same mission, same
    echo     ground, same enemies - so a loss is meant to teach you the level.
    echo     A contract you have already cleared pays a quarter if you run it
    echo     again, so money follows progress rather than grinding.
    echo   - Healing is mostly repair salvage: the amber crates that wrecked
    echo     armour leaves behind. Finishing an objective patches a little
    echo     more. Clearing a strongpoint is how you stay alive.
    echo   - Green crates are ammunition for whatever heavy gun you are
    echo     carrying.
    echo   - Contracts run ten to twenty minutes and are a chain of segments.
    echo     The corner panel names the current one and says in plain words
    echo     what finishes it; an amber diamond marks where to go, and an
    echo     amber chevron on the edge of the screen points at it when it is
    echo     behind you.
    echo   - Engine power is a mobility stat, not just a budget: a bigger
    echo     reactor makes the same legs walk faster. Compare DRIVE and SPD
    echo     in the workshop.
    echo   - Mission 2 has a duel in it. Helion keeps a spotter in the towers
    echo     of the core district - light frame, railgun, climbing claws. It
    echo     shoots from a hundred metres and leaves the moment you look at
    echo     it. You will need to go up after it.
    echo   - CLEARING PAYS. Every hostile is worth money, and closing a
    echo     segment out with nothing left standing near it pays a bonus on
    echo     top. Wrecked armour leaves repair salvage, so taking the ground
    echo     is also how you stay alive. Running the route is faster; taking
    echo     it apart is worth roughly twice as much.
    echo   - Every chassis has a TRAIT, shown in the workshop, and every
    echo     reactor has its own ability. A casemate shrugs off frontal hits;
    echo     a siege deck groups tighter standing still; a raider shoots as
    echo     well on the move. Fusion cores patch the frame, the overdrive
    echo     core fires an EMP that drops drones, the thermal plant runs you
    echo     dark so gun lines lose you.
    echo   - From the middle of the campaign you will meet marksmen, mortar
    echo     crews, jammers and repair walkers. If your fire control drops out
    echo     and the radar starts lying, there is a jammer nearby. If a
    echo     strongpoint will not die - watch for green crosses over it - kill
    echo     its Warden first. Mortars cannot depress: walk INTO them.
    echo   - Guns handle differently, not just hit differently. A rotary winds
    echo     up before it fires and its group walks off target if you lean on
    echo     the trigger; the reticle opens to show it, and letting go closes
    echo     it again. Burst rifles stay on target and reward picking a moment.
    echo   - The save file ^(acah_save.txt^) is written next to the exe.
    echo.
    echo   Feedback welcome: what killed you, what felt unfair, what was dull.
)

rem --- zip it -----------------------------------------------------------------
set "ZIP=dist\ACAH.zip"
if exist "%ZIP%" del /q "%ZIP%"
echo Packing %ZIP% ...
powershell -NoProfile -Command ^
    "Compress-Archive -Path 'dist\ACAH\*' -DestinationPath 'dist\ACAH.zip' -Force" 2>nul

rem --- the single file --------------------------------------------------------
rem The thing you actually want to hand someone. Only produced when the import
rem check came back clean, because an exe that needs a DLL sitting next to it
rem is not a single file however much we would like it to be.
if exist "dist\ACAH.exe" del /q "dist\ACAH.exe"
if not defined DEPLIST copy /y "%OUT%\ACAH.exe" "dist\ACAH.exe" >nul

echo.
if exist "dist\ACAH.exe" (
    for %%F in ("dist\ACAH.exe") do set /a EXEMB=%%~zF/1048576
    echo   dist\ACAH.exe   ^(!EXEMB! MB^)  <-- SEND THIS. One file. Nothing else
    echo                    needed: no folder, no installer, no runtime, and
    echo                    the game generates all its own art and sound.
    echo                    Windows will warn about an unrecognised app -
    echo                    More info -^> Run anyway.
) else (
    echo   No single-file exe was produced: this build still needs DLLs
    echo   alongside it. Send the zip below instead.
)
if exist "%ZIP%" (
    for %%F in ("%ZIP%") do set /a SIZEKB=%%~zF/1024
    echo   dist\ACAH.zip   ^(!SIZEKB! KB^)  the same exe plus PLAY.bat and the
    echo                    README, for when you want to send the notes too.
) else (
    echo   Could not create the zip ^(PowerShell missing?^). The folder
    echo   dist\ACAH is complete - zip it by hand if you need it.
)
echo.
if defined MISSINGDEP exit /b 1
endlocal
exit /b 0

rem ---------------------------------------------------------------------------
rem Classify one imported DLL. Windows' own libraries are fine - every machine
rem has them. Anything else has to travel with the game.
:checkdep
set "D=%~1"
set "DL=%D%"
for %%X in (
    KERNEL32.dll USER32.dll GDI32.dll ADVAPI32.dll SHELL32.dll ole32.dll
    OLEAUT32.dll IMM32.dll VERSION.dll WINMM.dll SETUPAPI.dll OPENGL32.dll
    msvcrt.dll ntdll.dll RPCRT4.dll COMDLG32.dll CFGMGR32.dll HID.DLL
    WS2_32.dll CRYPT32.dll bcrypt.dll dwmapi.dll UxTheme.dll
) do if /i "%D%"=="%%X" exit /b 0
rem The Universal CRT forwarders (api-ms-win-*) are part of Windows 10 and 11.
echo %D% | findstr /i /b /c:"api-ms-win-" >nul && exit /b 0
echo %D% | findstr /i /b /c:"ext-ms-" >nul && exit /b 0
set "DEPLIST=%DEPLIST% %D%"
exit /b 0
