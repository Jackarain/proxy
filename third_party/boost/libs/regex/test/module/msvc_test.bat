REM Basic command line to build everything with msvc:

rem del *.obj
rem del *.exe
rem del *.lib

cl /std:c++latest /EHsc /nologo /W4 /c "%VCToolsInstallDir%\modules\std.ixx"  || exit 1
cl /std:c++latest /EHsc /nologo /W4 /c /interface /I ..\..\..\.. ..\..\module\regex.cxx || exit 1
cl /std:c++latest /EHsc /nologo /W4 /c /I ..\..\..\.. ..\..\module\*.cpp || exit 1

lib *.obj /OUT:regex.lib || exit 1

rem time < nul

for %%f in (*.cpp) do (
   cl /std:c++latest /EHsc /nologo /W4 /I ..\..\..\.. %%f regex.lib || exit 1
)

rem time < nul

rem uncomment this section to test/time non-module build:
rem for %%f in (*.cpp) do (
rem cl /DTEST_HEADERS /std:c++latest /EHsc /nologo /W4 /I ..\..\..\.. %%f || exit 1
rem )

rem time < nul

