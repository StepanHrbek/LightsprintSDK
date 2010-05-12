rem Updates SDK runtimes from VS runtimes. Must be updated when Microsoft updates VS runtimes (service pack, hotfix..)

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.CRT"        win32\Microsoft.VC80.CRT
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.OPENMP"     win32\Microsoft.VC80.OPENMP

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\redist\amd64\Microsoft.VC80.CRT"      x64\Microsoft.VC80.CRT
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\redist\amd64\Microsoft.VC80.OPENMP"   x64\Microsoft.VC80.OPENMP

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\redist\x86\Microsoft.VC90.CRT"      win32\Microsoft.VC90.CRT
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\redist\x86\Microsoft.VC90.OpenMP"   win32\Microsoft.VC90.OpenMP

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\redist\amd64\Microsoft.VC90.CRT"    x64\Microsoft.VC90.CRT
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\redist\amd64\Microsoft.VC90.OpenMP" x64\Microsoft.VC90.OpenMP

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\redist\x86\Microsoft.VC100.CRT"    win32\Microsoft.VC100.CRT
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\redist\x86\Microsoft.VC100.OpenMP" win32\Microsoft.VC100.OpenMP

xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\redist\x64\Microsoft.VC100.CRT"    x64\Microsoft.VC100.CRT
xcopy /D /S /Y "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\redist\x64\Microsoft.VC100.OpenMP" x64\Microsoft.VC100.OpenMP
