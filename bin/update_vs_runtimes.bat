rem Updates SDK runtimes from VS runtimes. Must be updated when Microsoft updates VS runtimes (service pack, hotfix..)

rem Visual C++ 2015, 2017, 2019, 2022 runtimes should be the same

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist\x86\Microsoft.VC140.CRT"    win32
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist\x86\Microsoft.VC140.OpenMP" win32

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist\x64\Microsoft.VC140.CRT"    x64
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist\x64\Microsoft.VC140.OpenMP" x64

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30036\x86\Microsoft.VC142.CRT"    win32
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30036\x86\Microsoft.VC142.OpenMP" win32

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30036\x64\Microsoft.VC142.CRT"    x64
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30036\x64\Microsoft.VC142.OpenMP" x64

xcopy /D /S /Y "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.34.31931\x86\Microsoft.VC143.CRT"    win32
xcopy /D /S /Y "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.34.31931\x86\Microsoft.VC143.OpenMP" win32

xcopy /D /S /Y "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.34.31931\x64\Microsoft.VC143.CRT"    x64
xcopy /D /S /Y "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.34.31931\x64\Microsoft.VC143.OpenMP" x64
