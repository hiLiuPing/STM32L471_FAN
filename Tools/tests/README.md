# Stability host tests

Build and run from the repository root:

```powershell
D:\lvgl_pc\mingw64\bin\gcc.exe -std=c11 -Wall -Wextra -Werror `
  -IUSER\Middle\Transfer -IUSER\APP Tools\tests\test_stability.c `
  USER\APP\shake_detector.c -lm -o Tools\tests\test_stability.exe
Tools\tests\test_stability.exe
```

The test covers 32-bit tick wrap comparisons, fan deadline disable/reset/delayed
service behavior, bounded natural-wind phases over 31 simulated days, seven-day
forecast bitmap completeness, and stale/recovery elapsed-time handling.
It also verifies the production CRC-8 helper plus recovery after a bad CRC and
a simulated 200 ms truncated-frame reset.

Build and run the storage protection tests:

```powershell
D:\lvgl_pc\mingw64\bin\gcc.exe -std=c11 -Wall -Wextra -Werror `
  -IUSER\BSP `
  Tools\tests\test_storage_protection.c `
  -o build\test_storage_protection.exe
build\test_storage_protection.exe
```

This test injects EEPROM read/data failures and LittleFS mount/format failures.
It verifies that mount errors never trigger formatting, unavailable filesystems
do not publish a handle, and EEPROM I/O errors remain distinct from invalid data.
