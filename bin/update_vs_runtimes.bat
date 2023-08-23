rem Updates SDK runtimes from VS runtimes. Must be updated when Microsoft updates VS runtimes (service pack, hotfix..)
rem Old 2005/8 stay here because RL needs one of them for .skp reader

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.CRT"        win32\Microsoft.VC80.CRT
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.OPENMP"     win32\Microsoft.VC80.OPENMP
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.MFC"        win32\Microsoft.VC80.MFC

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\redist\amd64\Microsoft.VC80.CRT"      x64\Microsoft.VC80.CRT
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\redist\amd64\Microsoft.VC80.OPENMP"   x64\Microsoft.VC80.OPENMP

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\redist\x86\Microsoft.VC90.CRT"      win32\Microsoft.VC90.CRT
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\redist\x86\Microsoft.VC90.OpenMP"   win32\Microsoft.VC90.OpenMP

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\redist\amd64\Microsoft.VC90.CRT"    x64\Microsoft.VC90.CRT
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\redist\amd64\Microsoft.VC90.OpenMP" x64\Microsoft.VC90.OpenMP

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\redist\x86\Microsoft.VC100.CRT"    win32
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\redist\x86\Microsoft.VC100.OpenMP" win32

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\redist\x64\Microsoft.VC100.CRT"    x64
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\redist\x64\Microsoft.VC100.OpenMP" x64

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\redist\x86\Microsoft.VC110.CRT"    win32
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\redist\x86\Microsoft.VC110.OpenMP" win32

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\redist\x64\Microsoft.VC110.CRT"    x64
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\redist\x64\Microsoft.VC110.OpenMP" x64

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\redist\x86\Microsoft.VC120.CRT"    win32
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\redist\x86\Microsoft.VC120.OpenMP" win32

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\redist\x64\Microsoft.VC120.CRT"    x64
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\redist\x64\Microsoft.VC120.OpenMP" x64

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
