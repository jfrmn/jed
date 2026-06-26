param(
	[Alias("c")]
	[switch] $clean = $false,
	
	[Alias("r")]
	[switch] $reconfigure = $false,
		
	[Alias("p")]
	[string] $preset = "debug",
	
	[Alias("go")]
	[switch] $run = $false,

	[Alias("t")]
	[switch] $test = $false,

	[switch] $install = $false,
	[string] $installDir = ""
)

if (-not $env:_VSDEVSHELL) {
	Import-Module "D:\Apps\Visual Studio 2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll";
	Enter-VsDevShell a93d51fe -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
	$env:_VSDEVSHELL = "-arch=x64 -host_arch=x64"
}

# set tab color and title
Write-Output "`e];build`a`e[2;0;1,|";

if ($clean) {
	
	Write-Output "cleaning...";
	
	if (Test-Path ".\out\$preset") {
		Remove-Item -Path ".\out\$preset" -Recurse -Force;
	}
}

if (-not (Test-Path ".\out\$preset")) {
	New-Item -ItemType 'directory' .\out\$preset;
	$reconfigure = $true;
}
	
if ($reconfigure) {
	Write-Output "configuring...";
	& cmake --preset=$preset;

	if ($LASTEXITCODE -ne 0) {
		exit;
	}
}

Write-Output "compiling...";
& cmake --build --preset=$preset

if ($LASTEXITCODE -ne 0) {
	exit;
}

if ($install) {
	
	if (-not $installDir) {
		$installDir = "out/$preset/install";
	}
		
	Write-Output "installing...($installDir)";

	if (-not (Test-Path ".\out\$preset\install")) {
		New-Item -ItemType 'directory' .\out\$preset\install;
	}

	& cmake --install ./out/$preset/build --prefix $installDir 
}

if ($test) {
	Write-Output "running tests...";
	& ./out/$preset/slick-edit-tests.exe
}

if ($run) {
	& ./out/$preset/slick-edit.exe
}