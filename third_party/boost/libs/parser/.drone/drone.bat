@ECHO ON


git clone https://github.com/boostorg/boost-ci.git boost-ci-cloned --depth 1
REM Copy ci folder if not testing Boost.CI
if "%DRONE_REPO%" == "%DRONE_REPO:boost-ci=%" xcopy /s /e /q /i /y boost-ci-cloned\ci .\ci
rmdir /s /q boost-ci-cloned

set BOOST_CI_TARGET_BRANCH=%DRONE_BRANCH%
set BOOST_CI_SRC_FOLDER=%cd%

@ECHO OFF
echo ========================^> INSTALL

set custom_script=%BOOST_CI_SRC_FOLDER%\.drone\before-install.bat
if exist %custom_script% call %custom_script%

call %BOOST_CI_SRC_FOLDER%\ci\common_install.bat

set custom_script=%BOOST_CI_SRC_FOLDER%\.drone\after-install.bat
if exist %custom_script% call %custom_script%

echo ========================^> COMPILE

if NOT "%DRONE_JOB_BUILDTYPE%" == "boost" echo Ignoring DRONE_JOB_BUILDTYPE=%DRONE_JOB_BUILDTYPE% and doing a Boost build

call %BOOST_CI_SRC_FOLDER%\ci\build.bat
