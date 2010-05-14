
start build_wxWidgets_platform vc71 32 "%vs71comntools%vsvars32.bat"
start build_wxWidgets_2platforms vc80  "%vs80comntools%vsvars32.bat"  "%vs80comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_wxWidgets_2platforms vc90  "%vs90comntools%vsvars32.bat"  "%vs90comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_wxWidgets_2platforms vc100 "%vs100comntools%vsvars32.bat" "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"

exit

nmake nevim proc nezvlada 32bit a 64bit paralelne, takze nejde pustit 7 threadu, max 4

start build_wxWidgets_platform vc71  32 "%vs71comntools%vsvars32.bat"
start build_wxWidgets_platform vc80  32 "%vs80comntools%vsvars32.bat"
start build_wxWidgets_platform vc80  64 "%vs80comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_wxWidgets_platform vc90  32 "%vs90comntools%vsvars32.bat"
start build_wxWidgets_platform vc90  64 "%vs90comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_wxWidgets_platform vc100 32 "%vs100comntools%vsvars32.bat"
start build_wxWidgets_platform vc100 64 "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"
