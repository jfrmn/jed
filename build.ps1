param(
	[Alias("c")]
	[switch] $clean = $false,
	
	[Alias("re", "reconf")]
	[switch] $reconfigure = $false,
		
	[Alias("p")]
	[ValidateSet("debug", "stable", "release")]
	[string] $profile = "debug",
	
	[Alias("r")]
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

function Write-Step($message) {
	Write-Output "`e[1;36m > $message`e[0m";
}

# set tab color and title
Write-Output "`e];build`a`e[2;0;1,|" > $null;

if ($clean) {
	Write-Step "cleaning";
	
	if (Test-Path ".\out\$profile") {
		Remove-Item -Path ".\out\$profile" -Recurse -Force;
	}
}

if (-not (Test-Path ".\out\$profile")) {
	New-Item -ItemType 'directory' .\out\$profile;
	$reconfigure = $true;
}
	
if ($reconfigure) {
	Write-Step "reconfiguring";
	& cmake -S . -B .\out\$profile -G "Ninja" -DCMAKE_BUILD_TYPE="$profile";

	if ($LASTEXITCODE -ne 0) {
		exit;
	}
}

Write-Step "compiling";
& cmake --build .\out\$profile;

if ($LASTEXITCODE -ne 0) {
	exit;
}

if ($install) {
	
	if (-not $installDir) {
		$installDir = "out/$profile/install";
	}
		
	Write-Step "installing (dir=$installDir)";

	& cmake --install ./out/$profile/build --prefix $installDir;
}

if ($test) {
	Write-Step "running tests";
	& ./out/$profile/slick-edit-tests.exe
}

if ($run) {
	Write-Step "running";
	& ./out/$profile/slick-edit.exe
}