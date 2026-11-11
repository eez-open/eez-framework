# Generates the test projects (make_fwtest.py, make_fwtest_ext.py) and builds
# them with EEZ Studio (headless) into <project>/generated.
#
#   pwsh tests/projects/build_projects.ps1 [-Studio C:\Work\eez\studio]
#
# Studio must be compiled (build/ is up to date, e.g. by its tsc --watch).

param(
    [string]$Studio = "C:\Work\eez\studio"
)

$ErrorActionPreference = "Stop"
# fwtest_ext (external assets) has fwtest as its master project
$projects = @("fwtest\fwtest.eez-project", "fwtest_ext\fwtest_ext.eez-project")

foreach ($generator in @("make_fwtest.py", "make_fwtest_ext.py")) {
    python (Join-Path $PSScriptRoot $generator)
    if ($LASTEXITCODE -ne 0) {
        throw "$generator failed ($LASTEXITCODE)"
    }
}

# ELECTRON_RUN_AS_NODE is inherited from VS Code and makes electron run as node
Remove-Item Env:ELECTRON_RUN_AS_NODE -ErrorAction SilentlyContinue

$electron = Join-Path $Studio "node_modules\electron\dist\electron.exe"
foreach ($project in $projects) {
    $path = Join-Path $PSScriptRoot $project
    Write-Host "Building $path"
    Push-Location $Studio
    try {
        # Studio prints its result lines among Chromium cache warnings
        $output = & $electron . --build-project $path 2>&1 | ForEach-Object { "$_" }
        $output | Where-Object { $_ -notmatch "disk_cache|gpu_disk_cache|cache_util_win" -and $_ -ne "" } | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0 -or -not ($output -match "Build successfully finished")) {
            throw "Studio build of $project failed"
        }
    } finally {
        Pop-Location
    }
}
