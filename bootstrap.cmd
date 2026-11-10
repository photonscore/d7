@echo off
rem Copyright (c) 2026 Photonscore GmbH
rem
rem Licensed under the Apache License, Version 2.0 (the "License");
rem you may not use this file except in compliance with the License.
rem You may obtain a copy of the License at
rem
rem     http://www.apache.org/licenses/LICENSE-2.0
rem
rem Unless required by applicable law or agreed to in writing, software
rem distributed under the License is distributed on an "AS IS" BASIS,
rem WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
rem See the License for the specific language governing permissions and
rem limitations under the License.

rem Fetches pinned CPythons into third_party\python\<X.Y>\, asking first:
rem   bootstrap.cmd [--all] [--yes] [X.Y ...]
rem Then: third_party\python\3.12\python.exe configure.py
rem Writes nothing outside the checkout. Windows 10 and later (curl, tar).

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set RELEASE=20261026
set URL=https://github.com/astral-sh/python-build-standalone/releases/download
set DEFAULT=3.12
set ALL=3.11 3.12 3.13 3.14 3.15
set PINS=third_party\python\PINS
set TARGET=x86_64-pc-windows-msvc
set SYS=%SystemRoot%\System32

if /i not "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
  set "ERR=no pinned Python for %PROCESSOR_ARCHITECTURE%"
  goto fail
)

set "VERSIONS="
set YES=0
:args
if "%~1"=="" goto args_done
set "ARG=%~1"
if /i "!ARG!"=="--all" (
  set "VERSIONS=%ALL%"
) else if /i "!ARG!"=="--yes" (
  set YES=1
) else if "!ARG:~0,1!"=="-" (
  set "ERR=usage: bootstrap.cmd [--all] [--yes] [X.Y ...]"
  goto fail
) else (
  set "VERSIONS=!VERSIONS! !ARG!"
)
shift
goto args
:args_done
if not defined VERSIONS set "VERSIONS=%DEFAULT%"

rem What is missing: one "version sha256 size file" line each
if not exist .cache\downloads mkdir .cache\downloads
set "TODO=.cache\bootstrap-todo.txt"
type nul > "%TODO%"
set TOTAL=0
for %%v in (%VERSIONS%) do (
  if not exist "third_party\python\%%v\python.exe" (
    set "FOUND="
    for /f "usebackq eol=# tokens=1-5" %%a in ("%PINS%") do (
      if "%%a"=="%%v" if "%%b"=="%TARGET%" (
        echo %%a %%c %%d %%e>> "%TODO%"
        set /a TOTAL+=%%d
        set FOUND=1
      )
    )
    if not defined FOUND (
      set "ERR=no pinned Python %%v for %TARGET% in %PINS%"
      goto fail
    )
  )
)

if %TOTAL%==0 (
  del "%TODO%"
  for %%v in (%VERSIONS%) do echo Python %%v: third_party\python\%%v\python.exe
  exit /b 0
)

echo bootstrap.cmd will download, into this checkout only:
for /f "usebackq tokens=1-4" %%a in ("%TODO%") do (
  set /a M=%%c/1000000, F=%%c/100000%%10
  echo   - Python %%a ^(%TARGET%^)  !M!.!F! MB  -^> third_party\python\%%a\
)
set /a M=TOTAL/1000000, F=TOTAL/100000%%10
echo                                     total  %M%.%F% MB
if %YES%==0 (
  set "ANSWER="
  set /p "ANSWER=Proceed? [y/N] "
  if /i not "!ANSWER!"=="y" if /i not "!ANSWER!"=="yes" (
    set "ERR=nothing downloaded; --yes allows it without asking"
    goto fail
  )
)

for /f "usebackq tokens=1-4" %%a in ("%TODO%") do (
  set "ARCHIVE=.cache\downloads\%%d"
  echo Downloading %%d
  "%SYS%\curl.exe" -fsSL -o "!ARCHIVE!" "%URL%/%RELEASE%/%%d"
  if errorlevel 1 (
    set "ERR=downloading %%d failed"
    goto fail
  )
  set "GOT="
  for /f "skip=1 delims=" %%h in ('certutil -hashfile "!ARCHIVE!" SHA256') do (
    if not defined GOT set "GOT=%%h"
  )
  set "GOT=!GOT: =!"
  if /i not "!GOT!"=="%%b" (
    del "!ARCHIVE!"
    set "ERR=%%d has SHA-256 !GOT!, expected %%b"
    goto fail
  )
  if exist ".cache\python-%%a" rd /s /q ".cache\python-%%a"
  if exist "third_party\python\%%a" rd /s /q "third_party\python\%%a"
  mkdir ".cache\python-%%a"
  "%SYS%\tar.exe" -xzf "!ARCHIVE!" -C ".cache\python-%%a"
  if errorlevel 1 (
    set "ERR=unpacking %%d failed"
    goto fail
  )
  move ".cache\python-%%a\python" "third_party\python\%%a" > nul
  rd /s /q ".cache\python-%%a"
  del "!ARCHIVE!"
  echo Python %%a: third_party\python\%%a\python.exe
)
del "%TODO%"
exit /b 0

:fail
if exist "%TODO%" del "%TODO%"
1>&2 echo bootstrap.cmd: %ERR%
exit /b 1
