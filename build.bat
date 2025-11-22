@echo off
chcp 65001 >nul

echo Converting CSV file to C code...

:: Generate embedded data file
(
  echo // Auto-generated from card.csv
  echo const char embedded_csv_data[] =
) > embedded_data.c

:: Add CSV content line by line
for /f "tokens=*" %%i in (card.csv) do (
  echo "%%i\n" >> embedded_data.c
)

echo ; >> embedded_data.c

echo Compiling main application...
gcc -Os -s -mwindows main_app.c data_loader.c json_logger.c cJSON.c embedded_data.c -o main_app.exe

if %errorlevel% == 0 (
    echo Compilation successful!
    
    echo Checking file size before compression:
    for %%F in (main_app.exe) do echo Original size: %%~zF bytes
    
    echo Compressing executable with UPX...
    upx --best main_app.exe
    
    if %errorlevel% == 0 (
        echo Checking file size after compression:
        for %%F in (main_app.exe) do echo Compressed size: %%~zF bytes
        
        echo Compression ratio analysis completed.
    ) else (
        echo Warning: Compression failed or UPX not found.
        echo Please check if UPX is installed and in PATH.
    )
    
    del embedded_data.c
    echo.
    echo Application features:
    echo - Fixed blank display issue
    echo - Added debug information
    echo - Improved data loading reliability
    echo - Added JSON log file monitoring
) else (
    echo Compilation failed!
)

echo.
pause