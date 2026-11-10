# Copyright (c) 2026 Photonscore GmbH
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Source it, from Bash or zsh: `. ./env.sh`. Puts the checkout's Python
# (D7_PYTHON, default 3.12) and dotnet first on PATH, and dotnet's own
# caches into .cache/. The runtime's certificate store follows HOME alone.

_d7=$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)
D7_PYTHON=${D7_PYTHON:-3.12}
if [ -x "$_d7/third_party/python/$D7_PYTHON/bin/python3" ]; then
  PATH="$_d7/third_party/python/$D7_PYTHON/bin:$PATH"
fi
if [ -x "$_d7/third_party/dotnet/dotnet" ]; then
  export DOTNET_ROOT="$_d7/third_party/dotnet"
  PATH="$DOTNET_ROOT:$PATH"
fi
export PATH
export DOTNET_CLI_HOME="$_d7/.cache/dotnet-home"
export NUGET_PACKAGES="$_d7/.cache/nuget"
export NUGET_HTTP_CACHE_PATH="$_d7/.cache/nuget-http"
export NUGET_PLUGINS_CACHE_PATH="$_d7/.cache/nuget-plugins"
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
unset _d7
