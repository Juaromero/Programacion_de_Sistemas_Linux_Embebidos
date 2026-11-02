# Exporta las transcripciones completas de Claude Code de este proyecto a
# Documentos/IA/Transcripciones_crudas/ (carpeta ignorada por Git).
# Revise el contenido (rutas, correos, datos personales) antes de publicar nada.
#
# Uso (PowerShell):  .\exportar_chats.ps1

$destino = Join-Path $PSScriptRoot "Transcripciones_crudas"
New-Item -ItemType Directory -Force $destino | Out-Null

$origen = Join-Path $env:USERPROFILE ".claude\projects"
$carpetas = Get-ChildItem $origen -Directory | Where-Object { $_.Name -like "*Proyecto-Osciloscopio*" -or $_.Name -like "*Programacion-de-Sistemas-Linux-Embebidos*" }

foreach ($c in $carpetas) {
    Get-ChildItem $c.FullName -Filter *.jsonl -Recurse | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $destino $_.Name) -Force
        Write-Host "Copiado:" $_.Name
    }
}
Write-Host "Listo. Archivos en $destino"
