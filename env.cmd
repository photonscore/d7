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

rem Run it in the cmd.exe to set up: `env.cmd`. Puts the checkout's Python
rem (D7_PYTHON, default 3.12) and dotnet first on PATH, and dotnet's own
rem caches into .cache\.

set "D7_ROOT=%~dp0"
set "D7_ROOT=%D7_ROOT:~0,-1%"
if not defined D7_PYTHON set D7_PYTHON=3.12
set "_D7_PY=%D7_ROOT%\third_party\python\%D7_PYTHON%"
if exist "%_D7_PY%\python.exe" set "PATH=%_D7_PY%;%_D7_PY%\Scripts;%PATH%"
set "_D7_PY="
if exist "%D7_ROOT%\third_party\dotnet\dotnet.exe" (
  set "DOTNET_ROOT=%D7_ROOT%\third_party\dotnet"
  set "PATH=%D7_ROOT%\third_party\dotnet;%PATH%"
)
set "DOTNET_CLI_HOME=%D7_ROOT%\.cache\dotnet-home"
set "NUGET_PACKAGES=%D7_ROOT%\.cache\nuget"
set "NUGET_HTTP_CACHE_PATH=%D7_ROOT%\.cache\nuget-http"
set "NUGET_PLUGINS_CACHE_PATH=%D7_ROOT%\.cache\nuget-plugins"
set DOTNET_CLI_TELEMETRY_OPTOUT=1
set DOTNET_NOLOGO=1
