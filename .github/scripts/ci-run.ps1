param(
    [Parameter(Mandatory = $true)][string]$Label,
    [Parameter(Mandatory = $true)][string]$Log,
    [Parameter(Mandatory = $true)][string]$Command
)

cmd /c "$Command 1> `"$Log`" 2>&1"
$status = $LASTEXITCODE

Get-Content $Log

if ($null -eq $status) { $status = 1 }

if ($status -ne 0) {
    $message = (Get-Content $Log -Tail 30 | ForEach-Object { $_ -replace '%', '%25' }) -join '%0A'
    Write-Host "::error title=$Label failed::$message"
    exit $status
}
