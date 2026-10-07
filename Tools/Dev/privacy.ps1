# Does the repository expose personal data? (after v0.0.13, public repo)
#
#   pwsh Tools/Dev/privacy.ps1 [-Range <base>..<head>]
#
# Scans every tracked file - text and binary (glTF, PNG and fonts can embed
# paths and authoring metadata) - outside external/ (third-party code, as
# published by its authors) for:
#   - a local user profile path (C:\Users\<name>, /c/Users/, /home/<name>,
#     /Users/<name>): it names the developer's account and machine layout;
#   - an email address other than the noreply addresses commits and
#     attributions use;
#   - a private key or an access token.
# With -Range it also checks the commits in that range: each author and
# committer email must be a noreply address (GitHub keeps a pull request's
# own commits under refs/pull/* even after a squash merge).
# Every annotated tag's tagger email is checked the same way.
# Exit 0 if clean; 1 with every finding listed. CI runs it (the privacy job).
param([string]$Range = "")
$ErrorActionPreference = "Stop"
Set-Location (Resolve-Path (Join-Path $PSScriptRoot "../.."))

# Addresses meant to be public: GitHub's per-user noreply, GitHub's own,
# and the AI co-author attribution.
$allowedEmail = '@users\.noreply\.github\.com$|^noreply@github\.com$|^noreply@anthropic\.com$'
$userPath = '(?i)\b[a-z]:[\\/]+users[\\/]+(?!public\b|default\b|all users\b)[a-z0-9._-]+|(?<![a-z])/c/users/[a-z0-9._-]+|(?<![a-z0-9_])/home/[a-z][a-z0-9._-]+/|(?<![a-z0-9_])/Users/[A-Za-z][A-Za-z0-9._-]+/'
$email = '[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}'
$secret = '-----BEGIN [A-Z ]*PRIVATE KEY-----|\bghp_[A-Za-z0-9]{36}\b|\bgithub_pat_[A-Za-z0-9_]{40,}|\bAKIA[0-9A-Z]{16}\b|\bsk-ant-[A-Za-z0-9_-]{20,}|\bsk-[A-Za-z0-9]{40,}\b'

$findings = [System.Collections.Generic.List[string]]::new()
$files = @(git ls-files | Where-Object { $_ -notmatch '^external/' })
foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { continue } # deleted in the working tree
    # Latin-1 maps every byte to one character: text and binary alike.
    $text = [System.Text.Encoding]::Latin1.GetString([System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $file)))
    foreach ($m in [regex]::Matches($text, $userPath)) { $findings.Add("$file : user path '$($m.Value)'") }
    foreach ($m in [regex]::Matches($text, $secret)) { $findings.Add("$file : key or token '$($m.Value.Substring(0, [Math]::Min(12, $m.Value.Length)))...'") }
    foreach ($m in [regex]::Matches($text, $email)) {
        if ($m.Value -notmatch $allowedEmail) { $findings.Add("$file : email '$($m.Value)'") }
    }
}
Write-Host "Scanned $($files.Count) tracked files (external/ excluded)."

# Annotated tags carry their own identity (the tagger), pushed with them.
foreach ($line in @(git for-each-ref refs/tags --format='%(refname:short) %(objecttype) %(taggeremail)')) {
    $tag, $type, $tagger = $line -split ' '
    if ($type -eq 'tag' -and $tagger.Trim('<>') -notmatch $allowedEmail) { $findings.Add("tag $tag : tagger '$($tagger.Trim('<>'))'") }
}

if ($Range) {
    $identities = @(git log --format='%h %ae %ce' $Range)
    if ($LASTEXITCODE -ne 0) { throw "git log $Range failed (is the history fetched? fetch-depth: 0)" }
    foreach ($line in $identities) {
        $hash, $author, $committer = $line -split ' '
        foreach ($address in @($author, $committer) | Select-Object -Unique) {
            if ($address -notmatch $allowedEmail) { $findings.Add("commit $hash : identity '$address' (set user.email to your GitHub noreply address)") }
        }
    }
    Write-Host "Checked $($identities.Count) commit(s) in $Range."
}

if ($findings.Count -gt 0) {
    foreach ($finding in $findings) { Write-Host "  FAIL $finding" -ForegroundColor Red }
    Write-Host "FAILED: $($findings.Count) finding(s)" -ForegroundColor Red
    exit 1
}
Write-Host "PASS: no personal data found" -ForegroundColor Green
exit 0
