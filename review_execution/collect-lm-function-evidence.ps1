# Read-only archive evidence extraction. Not a compiler or acceptance test.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskArchivePath = 'D:/Table/LM/程序.zip'
$taskPlanPath = 'C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-review-tasks-2026-09-30.json'
$taskPlan = Get-Content -Raw -Encoding UTF8 $taskPlanPath | ConvertFrom-Json
$taskFeasibility = Get-Content -Raw -Encoding UTF8 (Join-Path (Split-Path $taskPlanPath) 'LH-compiler-feasibility-2026-09-30.json') | ConvertFrom-Json
$taskOutput = Join-Path $PSScriptRoot 'function-contract-evidence'
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$taskEntries = @()
$taskArchive = [IO.Compression.ZipFile]::OpenRead($taskArchivePath)
try {
    foreach ($taskEntry in $taskArchive.Entries) {
        if ($taskEntry.FullName -notmatch '\.(inva|typ|code|rep)$') { continue }
        $taskStream = $taskEntry.Open()
        $taskBuffer = [IO.MemoryStream]::new()
        try { $taskStream.CopyTo($taskBuffer); $taskBytes = $taskBuffer.ToArray() }
        finally { $taskStream.Dispose(); $taskBuffer.Dispose() }
        $taskHasher = [Security.Cryptography.SHA256]::Create()
        try { $taskHash = [BitConverter]::ToString($taskHasher.ComputeHash($taskBytes)).Replace('-','').ToLowerInvariant() }
        finally { $taskHasher.Dispose() }
        $taskText = [Text.Encoding]::GetEncoding(936).GetString($taskBytes)
        $taskEntries += @{path=$taskEntry.FullName;hash=$taskHash;text=$taskText;lines=($taskText -split "`r?`n")}
    }
} finally { $taskArchive.Dispose() }
$taskIndex = @()
foreach ($taskItem in $taskPlan.tasks | Where-Object {$_.id -like 'FB*'}) {
    $taskName = [regex]::Match($taskItem.title, '核实 (\w+) 的').Groups[1].Value
    if (-not $taskName) { throw "Missing block name $($taskItem.id)" }
    $taskCandidate = $taskFeasibility.blocks | Where-Object {$_.id -eq $taskItem.id}
    $taskLegacyName = [regex]::Match($taskCandidate.legacyCandidate, '_(\w+)').Groups[1].Value
    if (-not $taskLegacyName) { $taskLegacyName = $taskName }
    $taskCalls = @()
    $taskInstances = [Collections.Generic.HashSet[string]]::new()
    foreach ($taskSource in $taskEntries | Where-Object {$_.path -match '\.inva$'}) {
        # Preserve line positions while excluding line comments. This is a lexical
        # evidence extractor, not a semantic parser of the legacy language.
        $taskSearch = [regex]::Replace($taskSource.text, '(?m)//[^\r\n]*', '')
        $taskPattern = '(?m)_' + [regex]::Escape($taskLegacyName) + '\b\s*(?:\[(?<instance>[^\]]+)\])?(?<body>[^;]*);'
        foreach ($taskMatch in [regex]::Matches($taskSearch,$taskPattern)) {
            $taskLine = 1 + ([regex]::Matches($taskSearch.Substring(0,$taskMatch.Index), "`n")).Count
            $taskInstance = $taskMatch.Groups['instance'].Value.Trim()
            if ($taskInstance) { [void]$taskInstances.Add($taskInstance) }
            $taskParameters = @()
            $taskParameterPattern = '(?<name>"[^"\r\n]+"|\w+)\s*(?<operator>::|:=|=:)\s*(?<value>[^,;\r\n]+)'
            foreach ($taskParameter in [regex]::Matches($taskMatch.Groups['body'].Value,$taskParameterPattern)) {
                $taskValue = $taskParameter.Groups['value'].Value.Trim()
                $taskKind = if ($taskValue -match '^0x[0-9a-f]+$') {'hex-literal'}
                    elseif ($taskValue -match '^-?\d+$') {'integer-literal'}
                    elseif ($taskValue -match '^-?\d+\.\d+(?:[Ee][+-]?\d+)?$') {'real-literal'}
                    elseif ($taskValue -match '^#\w+$') {'legacy-enum-token'}
                    elseif ($taskValue -match '^\w+\.\w+$') {'member-reference-syntax'}
                    else {'unresolved-symbol-or-expression'}
                $taskParameters += @{name=$taskParameter.Groups['name'].Value.Trim('"');operator=$taskParameter.Groups['operator'].Value;rawValue=$taskValue;syntacticKind=$taskKind;lhOperandEncoding=$null;direction=$null}
            }
            $taskCalls += @{entry=$taskSource.path;sha256=$taskSource.hash;line=$taskLine;instance=$taskInstance;source=$taskMatch.Value;orderedParameters=$taskParameters}
        }
    }
    $taskFields = @()
    foreach ($taskTypes in $taskEntries | Where-Object {$_.path -match '\.typ$'}) {
        for ($taskLineIndex=0;$taskLineIndex -lt $taskTypes.lines.Count;$taskLineIndex++) {
            $taskField = [regex]::Match($taskTypes.lines[$taskLineIndex], '^\s*(\d+)\s+(\d+)\s+(\w+)\.(\w+)\s*$')
            if ($taskField.Success -and $taskInstances.Contains($taskField.Groups[3].Value)) {
                $taskFields += @{entry=$taskTypes.path;sha256=$taskTypes.hash;line=($taskLineIndex+1);legacyAddress=[long]$taskField.Groups[1].Value;legacyTypeCode=[int]$taskField.Groups[2].Value;instance=$taskField.Groups[3].Value;field=$taskField.Groups[4].Value;lhOffset=$null;lhType=$null;direction=$null}
            }
        }
    }
    $taskCodeRows = @()
    foreach ($taskTypes in $taskFields | Group-Object entry,instance) {
        $taskFirstField = $taskTypes.Group | Sort-Object legacyAddress | Select-Object -First 1
        $taskCodePath = $taskFirstField.entry -replace '\.typ$','.code'
        $taskCodeEntry = $taskEntries | Where-Object {$_.path -eq $taskCodePath} | Select-Object -First 1
        if (-not $taskCodeEntry) { continue }
        for ($taskLineIndex=0;$taskLineIndex -lt $taskCodeEntry.lines.Count;$taskLineIndex++) {
            $taskRow = $taskCodeEntry.lines[$taskLineIndex]
            if ($taskRow -match ('^\s*\d+\s+\d+\s+' + $taskFirstField.legacyAddress + '(?:\s|$)')) {
                $taskCodeRows += @{entry=$taskCodePath;sha256=$taskCodeEntry.hash;line=($taskLineIndex+1);source=$taskRow;instance=$taskFirstField.instance;correlation='candidate matching first legacy field address; no LH ABI translation'}
            }
        }
    }
    $taskRecord = @{schemaVersion=1;taskId=$taskItem.id;block=$taskName;scope='offline lexical LM evidence; production LH contract unverified';downloadableTargetCertified=$false;archive=$taskArchivePath;callCount=$taskCalls.Count;calls=@($taskCalls | Select-Object -First 24);fields=$taskFields;candidateCodeRows=$taskCodeRows;planScope=$taskItem.scope;externalPrerequisites=$taskItem.externalPrerequisites;missing=@('LH firmware version, opcode and operand table','LH address unit, alignment, field width/direction','Named legacy operators/enums are not translated into LH values');extractionLimits=@('First 24 call excerpts retained; total count includes all matches','Lexical operator/value classification is not semantic direction or type proof','No unaddressed marker opcode is inferred from an empty parameter list')}
    $taskRecord.legacyCandidate = $taskCandidate.legacyCandidate
    $taskRecord.mappingStatus = $taskCandidate.mappingStatus
    $taskRecord.observedLegacyName = $taskLegacyName
    $taskRecord.lhNameMappingCertified = $false
    $taskRecord | ConvertTo-Json -Depth 12 | Set-Content -Encoding UTF8 (Join-Path $taskOutput ($taskItem.id + '-' + $taskName + '.json'))
    $taskIndex += @{id=$taskItem.id;block=$taskName;callCount=$taskCalls.Count;fieldCount=$taskFields.Count;candidateCodeRows=$taskCodeRows.Count;productionEnabled=$false;file=('function-contract-evidence/' + $taskItem.id + '-' + $taskName + '.json')}
}
$taskIndex | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $PSScriptRoot 'function-contract-evidence-index.json')
$taskIndex | Select-Object id,block,callCount,fieldCount,candidateCodeRows | Format-Table -AutoSize
