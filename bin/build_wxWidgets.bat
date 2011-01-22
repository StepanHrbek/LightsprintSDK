echo Running 4 build threads in parallel...

start build_wxWidgets_platform vc71 32 "%vs71comntools%vsvars32.bat"
start build_wxWidgets_2platforms vc80  "%vs80comntools%vsvars32.bat"  "%vs80comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_wxWidgets_2platforms vc90  "%vs90comntools%vsvars32.bat"  "%vs90comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_wxWidgets_2platforms vc100 "%vs100comntools%vsvars32.bat" "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"

exit

building 32bit and 64bit in parallel causes conflicts, so we can't run 7 threads in parallel as shown here:

start build_wxWidgets_platform vc71  32 "%vs71comntools%vsvars32.bat"
start build_wxWidgets_platform vc80  32 "%vs80comntools%vsvars32.bat"
start build_wxWidgets_platform vc80  64 "%vs80comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_wxWidgets_platform vc90  32 "%vs90comntools%vsvars32.bat"
start build_wxWidgets_platform vc90  64 "%vs90comntools%..\..\vc\bin\amd64\vcvarsamd64.bat"
start build_wxWidgets_platform vc100 32 "%vs100comntools%vsvars32.bat"
start build_wxWidgets_platform vc100 64 "%vs100comntools%..\..\vc\bin\amd64\vcvars64.bat"
