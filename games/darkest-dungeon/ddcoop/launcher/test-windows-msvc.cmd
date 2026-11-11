@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist build\win\tests\fake-steam mkdir build\win\tests\fake-steam
set "DDC_CC=cl /nologo /std:c11 /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /Ilauncher\src /Fobuild\win\tests\"
%DDC_CC% /Febuild\win\tests\test_lr_dlc.exe launcher\tests\test_lr_dlc.c launcher\src\lr_dlc.c launcher\src\lr_files.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_logpack.exe launcher\tests\test_lr_logpack.c launcher\src\lr_logpack.c launcher\src\lr_logs.c launcher\src\lr_wire.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_diagnostics.exe launcher\tests\test_lr_diagnostics.c launcher\src\lr_diagnostics.c launcher\src\lr_logpack.c launcher\src\lr_logs.c launcher\src\lr_wire.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_logs.exe launcher\tests\test_lr_logs.c launcher\src\lr_logs.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_mascot.exe launcher\tests\test_lr_mascot.c build\win\ddcoop.res /link gdiplus.lib ole32.lib user32.lib gdi32.lib
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_restart.exe launcher\tests\test_lr_restart.c launcher\src\lr_restart.c launcher\src\lr_wire.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_restart_sim.exe launcher\tests\test_lr_restart_sim.c launcher\src\lr_restart.c launcher\src\lr_wire.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_mods.exe launcher\tests\test_lr_mods.c launcher\src\lr_mods.c launcher\src\lr_files.c launcher\src\lr_roster.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_kit.exe launcher\tests\test_lr_kit.c launcher\src\lr_kit.c launcher\src\lr_mods.c launcher\src\lr_files.c launcher\src\lr_roster.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_kitsig_win.exe launcher\tests\test_lr_kitsig_win.c launcher\src\lr_kit.c launcher\src\lr_kitsig_win.c launcher\src\lr_mods.c launcher\src\lr_files.c launcher\src\lr_roster.c /link bcrypt.lib
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_wire.exe launcher\tests\test_lr_wire.c launcher\src\lr_wire.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_roster.exe launcher\tests\test_lr_roster.c launcher\src\lr_roster.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_owners.exe launcher\tests\test_lr_owners.c launcher\src\lr_owners.c launcher\src\lr_roster.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_live_party_win.exe launcher\tests\test_lr_live_party_win.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_snapshot.exe launcher\tests\test_lr_snapshot.c launcher\src\lr_snapshot.c launcher\src\lr_files.c launcher\src\lr_wire.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_core.exe launcher\tests\test_lr_core.c launcher\src\lr_wire.c launcher\src\lr_sock.c launcher\src\lr_relay.c launcher\src\lr_files.c launcher\src\lr_core.c launcher\src\lr_dlc.c launcher\src\lr_mods.c launcher\src\lr_kit.c launcher\src\lr_restart.c launcher\src\lr_owners.c launcher\src\lr_roster.c launcher\src\lr_snapshot.c /link /STACK:8388608 ws2_32.lib
if errorlevel 1 goto failed
%DDC_CC% /LD /Febuild\win\tests\fake-steam\steam_api64.dll launcher\tests\fake_storage_api.c
if errorlevel 1 goto failed
%DDC_CC% /Febuild\win\tests\test_lr_prepare_win.exe launcher\tests\test_lr_prepare_win.c launcher\src\lr_snapshot.c launcher\src\lr_files.c launcher\src\lr_wire.c
if errorlevel 1 goto failed
%DDC_CC% /D_WIN32_WINNT=0x0601 /Febuild\win\tests\test_lr_diagnostics_win.exe launcher\tests\test_lr_diagnostics_win.c launcher\src\lr_core.c launcher\src\lr_diagnostics.c launcher\src\lr_dlc.c launcher\src\lr_files.c launcher\src\lr_install.c launcher\src\lr_kit.c launcher\src\lr_kitsig_win.c launcher\src\lr_logpack.c launcher\src\lr_logs.c launcher\src\lr_mascot.c launcher\src\lr_mods.c launcher\src\lr_owners.c launcher\src\lr_prepare_win.c launcher\src\lr_relay.c launcher\src\lr_restart.c launcher\src\lr_roster.c launcher\src\lr_snapshot.c launcher\src\lr_sock.c launcher\src\lr_steam.c launcher\src\lr_wire.c build\win\ddcoop.res /link ws2_32.lib bcrypt.lib uuid.lib winmm.lib shell32.lib ole32.lib comctl32.lib gdiplus.lib user32.lib gdi32.lib advapi32.lib
if errorlevel 1 goto failed
build\win\tests\test_lr_diagnostics_win.exe build\win\tests\diagnostics-adapter
if errorlevel 1 goto failed
%DDC_CC% /D_WIN32_WINNT=0x0601 /DTEST_DIAG_DUO /Febuild\win\tests\test_lr_diagnostics_duo_win.exe launcher\tests\test_lr_diagnostics_win.c launcher\src\lr_core.c launcher\src\lr_diagnostics.c launcher\src\lr_dlc.c launcher\src\lr_files.c launcher\src\lr_install.c launcher\src\lr_kit.c launcher\src\lr_kitsig_win.c launcher\src\lr_logpack.c launcher\src\lr_logs.c launcher\src\lr_mascot.c launcher\src\lr_mods.c launcher\src\lr_owners.c launcher\src\lr_prepare_win.c launcher\src\lr_relay.c launcher\src\lr_restart.c launcher\src\lr_roster.c launcher\src\lr_snapshot.c launcher\src\lr_sock.c launcher\src\lr_steam.c launcher\src\lr_wire.c build\win\ddcoop.res /link ws2_32.lib bcrypt.lib uuid.lib winmm.lib shell32.lib ole32.lib comctl32.lib gdiplus.lib user32.lib gdi32.lib advapi32.lib
if errorlevel 1 goto failed
build\win\tests\test_lr_diagnostics_duo_win.exe build\win\tests\diagnostics-duo-adapter
if errorlevel 1 goto failed
for %%t in (logpack diagnostics wire roster mods kit kitsig_win owners live_party_win mascot logs dlc restart restart_sim core) do (
  build\win\tests\test_lr_%%t.exe
  if errorlevel 1 goto failed
)
pushd build\win\tests
test_lr_snapshot.exe
if errorlevel 1 (popd & goto failed)
popd
build\win\tests\test_lr_prepare_win.exe build\win\DDCoop.exe build\win\tests\fake-steam
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
