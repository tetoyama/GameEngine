param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$sdk = Join-Path $Destination 'SDL3-3.4.18'
if (!(Test-Path -LiteralPath (Join-Path $sdk 'include/SDL3/SDL_gpu.h')) -or
    !(Test-Path -LiteralPath (Join-Path $sdk 'lib/x64/SDL3.lib'))) {
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $archive = Join-Path $Destination 'SDL3-devel-3.4.18-VC.zip'
    Invoke-WebRequest -Uri 'https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-devel-3.4.18-VC.zip' -OutFile $archive
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne '78D84602AE616CFE26B33A73B7D3B9B6B56E8A6650E53D92C9C6CA938F747ACD') {
        throw 'SDL SDK checksum mismatch.'
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $Destination -Force
}
