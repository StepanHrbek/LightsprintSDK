@echo off
set sdkpath=.
set colladapath=c:\OpenCollada

xcopy /s /y "%colladapath%\COLLADABaseUtils\include\*.h" "%sdkpath%\COLLADABaseUtils\include\"
xcopy /s /y "%colladapath%\COLLADABaseUtils\src\*.cpp" "%sdkpath%\COLLADABaseUtils\src\"
xcopy /s /y "%colladapath%\COLLADAFramework\include\*.h" "%sdkpath%\COLLADAFramework\include\"
xcopy /s /y "%colladapath%\COLLADAFramework\src\*.cpp" "%sdkpath%\COLLADAFramework\src\"

xcopy /s /y "%colladapath%\COLLADASaxFrameworkLoader\include\*.h" "%sdkpath%\COLLADASaxFrameworkLoader\include\"
xcopy /s /y "%colladapath%\COLLADASaxFrameworkLoader\src\*.cpp" "%sdkpath%\COLLADASaxFrameworkLoader\src\"

del "%sdkpath%\COLLADASaxFrameworkLoader\src\generated14\COLLADASaxFWLColladaParserAutoGen14PrivateValidation.cpp"
del "%sdkpath%\COLLADASaxFrameworkLoader\src\generated15\COLLADASaxFWLColladaParserAutoGen15PrivateValidation.cpp"

xcopy /s /y "%colladapath%\GeneratedSaxParser\include\*.h" "%sdkpath%\GeneratedSaxParser\include\"
xcopy /s /y "%colladapath%\GeneratedSaxParser\src\*.cpp" "%sdkpath%\GeneratedSaxParser\src\"

del "%sdkpath%\GeneratedSaxParser\src\GeneratedSaxParserLibxmlSaxParser.cpp"
del "%sdkpath%\GeneratedSaxParser\include\GeneratedSaxParserLibxmlSaxParser.h"

xcopy /s /y "%colladapath%\Externals\expat\lib\*.c" "%sdkpath%\Externals\expat\lib\"
xcopy /s /y "%colladapath%\Externals\expat\lib\*.h" "%sdkpath%\Externals\expat\lib\"

xcopy /s /y "%colladapath%\Externals\MathMLSolver\include\*.h" "%sdkpath%\Externals\MathMLSolver\include\"
xcopy /s /y "%colladapath%\Externals\MathMLSolver\src\*.cpp" "%sdkpath%\Externals\MathMLSolver\src\"

xcopy /s /y "%colladapath%\Externals\pcre\include\*.h" "%sdkpath%\Externals\pcre\include\"
xcopy /s /y "%colladapath%\Externals\pcre\src\*.c" "%sdkpath%\Externals\pcre\src\"

xcopy /s /y "%colladapath%\Externals\UTF\include\*.h" "%sdkpath%\Externals\UTF\include\"
xcopy /s /y "%colladapath%\Externals\UTF\src\*.c" "%sdkpath%\Externals\UTF\src\"

..\..\..\bin\sed -e s/1500/1600/g "%colladapath%\COLLADABaseUtils\include\COLLADABUhash_map.h" > "%sdkpath%\COLLADABaseUtils\include\COLLADABUhash_map.h"

echo V COLLADASaxFrameworkLoader/include/COLLADASaxFWLPostProcessor.h line55 musi byt smazany assert, hazel by ho v "COLLADA 1.5.0 Kinematics\COLLADA\simple\simple.dae".
