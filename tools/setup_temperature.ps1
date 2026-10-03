# Launch the USB wizard using the prepared runtime, or a normal Python installation.
$pythonExe = 'C:\Users\pc\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Test-Path -LiteralPath $pythonExe)) {
    $pythonExe = (Get-Command python -ErrorAction Stop).Source
}
$serialLibraries = 'C:\Users\pc\Documents\Codex\tools\temperature\python-libs'
$previousPythonPath = $env:PYTHONPATH
if (Test-Path -LiteralPath $serialLibraries) {
    $env:PYTHONPATH = if ($previousPythonPath) { "$serialLibraries;$previousPythonPath" } else { $serialLibraries }
}
try {
    & $pythonExe -X utf8 (Join-Path $PSScriptRoot 'setup_temperature.py') @args
    $toolExitCode = $LASTEXITCODE
} finally {
    $env:PYTHONPATH = $previousPythonPath
}
exit $toolExitCode
