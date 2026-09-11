param(
	[switch]
	$skipDelays = $false
)

Write-Output "[36m > compiling[0m"
Write-Output "[1/47] Building CXX object CMakeFiles\jed.dir\src\editor\editor-gotoline.cc.obj"
if (-not $skipDelays) { Start-Sleep -Milliseconds $(Get-Random -Minimum 200 -Maximum 1000); }
Write-Output "FAILED: [code=2] CMakeFiles/jed.dir/src/editor/editor-gotoline.cc.obj "
Write-Output "D:\Apps\VISUAL~1\VC\Tools\MSVC\1451~1.362\bin\Hostx64\x64\cl.exe  /nologo /TP -DPCRE2_STATIC -ID:\Projects\jed\main\deps\tomlplusplus\include -ID:\Projects\jed\main\out\debug\deps\pcre2\interface -ID:\Projects\jed\main\deps\tree-sitter\lib\include -ID:\Projects\jed\main\deps\cJSON -ID:\Projects\jed\main\src /DWIN32 /D_WINDOWS /Zi /W3 /Zc:preprocessor /MDd /Ob0 /Od /DDEBUG /EHsc /RTC1 -std:c++20 /showIncludes /FoCMakeFiles\jed.dir\src\editor\editor-gotoline.cc.obj /FdCMakeFiles\jed.dir\ /FS -c D:\Projects\jed\main\src\editor\editor-gotoline.cc"
Write-Output "D:\Projects\jed\main\src\editor\editor-gotoline.cc(62): error C2065: 'deviceContext': undeclared identifier"
Write-Output "D:\Projects\jed\main\src\editor\editor-gotoline.cc(66): error C2065: 'deviceContext': undeclared identifier"
Write-Output "D:\Projects\jed\main\src\editor\editor-gotoline.cc(68): error C2065: 'deviceContext': undeclared identifier"
Write-Output "D:\Projects\jed\main\src\editor\editor-gotoline.cc(69): error C2065: 'deviceContext': undeclared identifier"
Write-Output "D:\Projects\jed\main\src\editor\editor-gotoline.cc(72): error C2065: 'deviceContext': undeclared identifier"
Write-Output "D:\Projects\jed\main\src\editor\editor-gotoline.cc(77): error C2065: 'deviceContext': undeclared identifier"
Write-Output "D:\Projects\jed\main\src\editor\editor-gotoline.cc(83): error C2065: 'deviceContext': undeclared identifier"
Write-Output "[2/47] Building CXX object CMakeFiles\jed.dir\src\ui\text-box.cc.obj"
if (-not $skipDelays) { Start-Sleep -Milliseconds $(Get-Random -Minimum 200 -Maximum 1000); }
Write-Output "FAILED: [code=2] CMakeFiles/jed.dir/src/ui/text-box.cc.obj "
Write-Output "D:\Apps\VISUAL~1\VC\Tools\MSVC\1451~1.362\bin\Hostx64\x64\cl.exe  /nologo /TP -DPCRE2_STATIC -ID:\Projects\jed\main\deps\tomlplusplus\include -ID:\Projects\jed\main\out\debug\deps\pcre2\interface -ID:\Projects\jed\main\deps\tree-sitter\lib\include -ID:\Projects\jed\main\deps\cJSON -ID:\Projects\jed\main\src /DWIN32 /D_WINDOWS /Zi /W3 /Zc:preprocessor /MDd /Ob0 /Od /DDEBUG /EHsc /RTC1 -std:c++20 /showIncludes /FoCMakeFiles\jed.dir\src\ui\text-box.cc.obj /FdCMakeFiles\jed.dir\ /FS -c D:\Projects\jed\main\src\ui\text-box.cc"
Write-Output "D:\Projects\jed\main\src\ui\text-box.cc(66): error C2065: 'deviceContext': undeclared identifier"
Write-Output "D:\Projects\jed\main\src\ui\text-box.cc(73): error C2065: 'deviceContext': undeclared identifier"
Write-Output "D:\Projects\jed\main\src\ui\text-box.cc(81): error C2065: 'deviceContext': undeclared identifier"
Write-Output "D:\Projects\jed\main\src\ui\text-box.cc(95): error C2065: 'deviceContext': undeclared identifier"
Write-Output "D:\Projects\jed\main\src\ui\text-box.cc(113): error C2065: 'deviceContext': undeclared identifier"
if (-not $skipDelays) { Start-Sleep -Milliseconds $(Get-Random -Minimum 200 -Maximum 1000); }
Write-Output "[XXX/47] Building CXX object CMakeFiles\jed.dir\src\editor\editor-signaturehelp.cc.obj"
if (-not $skipDelays) { Start-Sleep -Milliseconds $(Get-Random -Minimum 200 -Maximum 1000); }
Write-Output "[30/60] Building CXX object CMakeFiles\jed.dir\src\tools.cc.obj"
if (-not $skipDelays) { Start-Sleep -Milliseconds $(Get-Random -Minimum 200 -Maximum 1000); }
Write-Output "D:\Projects\jed\main\src\tools.cc(22): warning C4018: '>=': signed/unsigned mismatch"
if (-not $skipDelays) { Start-Sleep -Milliseconds $(Get-Random -Minimum 200 -Maximum 1000); }
Write-Output "[57/60] Building CXX object CMakeFiles\jed-tests.dir\src\tools.cc.obj"
Write-Output "D:\Projects\jed\main\src\tools.cc(22): warning C4018: '>=': signed/unsigned mismatch"
Write-Output "ninja: build stopped: subcommand failed."
