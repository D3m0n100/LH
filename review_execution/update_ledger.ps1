param([Parameter(Mandatory=$true)][string]$UpdatesFile)
$taskRoot = $PSScriptRoot
$taskJsonPath = Join-Path $taskRoot 'LH-repair-implementation-ledger-2026-09-30.json'
$taskLedger = Get-Content -Raw -Encoding UTF8 $taskJsonPath | ConvertFrom-Json
$taskUpdates = Get-Content -Raw -Encoding UTF8 $UpdatesFile | ConvertFrom-Json
foreach ($taskUpdate in $taskUpdates) {
    $taskEntry = $taskLedger.tasks | Where-Object { $_.id -eq $taskUpdate.id }
    if (-not $taskEntry) { throw "Unknown task $($taskUpdate.id)" }
    foreach ($taskProperty in $taskUpdate.PSObject.Properties) {
        $taskEntry | Add-Member -NotePropertyName $taskProperty.Name -NotePropertyValue $taskProperty.Value -Force
    }
}
$taskLedger | Add-Member -NotePropertyName updatedAt -NotePropertyValue ([DateTimeOffset]::UtcNow.ToString('o')) -Force
$taskLedger | ConvertTo-Json -Depth 14 | Set-Content -Encoding UTF8 $taskJsonPath
$taskLines = [System.Collections.Generic.List[string]]::new()
$taskLines.Add('# LH 修复实施记录 · 2026-09-30')
$taskLines.Add('')
$taskLines.Add('基线 f3f0242；根 D:/Table/LH。仅实施代码/必要测试与契约；未运行构建、CTest、Python 测试、GUI、安装包或设备验收，未提交/推送。最终验收由总控执行。')
$taskLines.Add('')
$taskLines.Add('本目录为授权范围内的实施交付目录；原审查与任务清单保持原样。实施中的条目可能尚未开始修改，具体以 behavior/files 为准。')
foreach ($taskEntry in $taskLedger.tasks) {
    $taskLines.Add('')
    $taskLines.Add("## $($taskEntry.id) · $($taskEntry.status) · $($taskEntry.title)")
    $taskLines.Add('')
    $taskLines.Add('文件：' + ($taskEntry.files -join '；'))
    $taskLines.Add('')
    $taskLines.Add('行为：' + ($taskEntry.behavior -join '；'))
    $taskLines.Add('')
    $taskLines.Add('兼容性：' + $taskEntry.compatibility)
    $taskLines.Add('')
    $taskLines.Add('总控检查（本对话未执行）：')
    foreach ($taskCheck in $taskEntry.checks) { $taskLines.Add('- `' + $taskCheck + '`') }
    $taskLines.Add('')
    $taskLines.Add('剩余问题：' + ($taskEntry.remainingQuestions -join '；'))
}
$taskLines | Set-Content -Encoding UTF8 (Join-Path $taskRoot 'LH-repair-implementation-ledger-2026-09-30.md')
