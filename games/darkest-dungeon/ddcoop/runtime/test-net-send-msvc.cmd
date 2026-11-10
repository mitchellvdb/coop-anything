@echo off
setlocal
pushd "%~dp0.."
if not exist build\win\net-send-tests mkdir build\win\net-send-tests
cl /nologo /std:c11 /O2 /MT /W4 /D_CRT_SECURE_NO_WARNINGS /Isrc /Fobuild\win\net-send-tests\ /Febuild\win\net-send-tests\test_net_send.exe tests\test_net_send.c src\dd_session.c src\dd_crypto_win.c src\dd_proto.c /link bcrypt.lib ws2_32.lib
if errorlevel 1 goto failed
build\win\net-send-tests\test_net_send.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
