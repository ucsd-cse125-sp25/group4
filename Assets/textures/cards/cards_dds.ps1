$OutDir = "cards"

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

Get-ChildItem -Path . -Filter '*.png' | ForEach-Object {
    $src = $_.FullName
    .\texconv.exe -f BC7_UNORM -o $OutDir $src
}