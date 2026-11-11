Darkest Dungeon co-op - network self-test 1
===========================================

What this is
  A small stand-alone test program (it does not touch the game, Steam or your saves). It checks that
  the security building blocks of the co-op connection work on your Windows PC exactly as on the Linux
  test machine:
    - the cryptography from Windows itself (HMAC, PBKDF2, HKDF, AES-256-GCM) against published test vectors,
    - the password handshake: right password works, wrong password and a fake host are refused,
    - encrypted messages: tampered, replayed or reordered messages are refused.
  It makes no network connections.

Steps
  1. Unzip this kit anywhere.
  2. Open PowerShell in that folder (Shift + right-click > "Open PowerShell window here") and run:
         .\ddcoop-selftest.exe > selftest.txt
     It takes a few seconds (the password key derivation is deliberately slow).
     Windows SmartScreen may warn about an unknown program; it is the test program built for this project
     (sha256 5fedac9e40dcbfbf59b85edb2fe0b7e4e0a05c89212a41354dc180333e00736e).
  3. Put selftest.txt in <project dir> on the development machine.

Expected content of selftest.txt
    OK (0 failures)
