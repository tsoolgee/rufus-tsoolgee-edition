# Rufus tsoolgee - UI smoke test, run on a (disposable) GitHub Windows runner.
#
#   fork/uitest.ps1 -Exe rufus.exe -OutDir shots -Locale en-US [-RequireUpdateSignature]
#
# Checks that the "Internal drives" checkbox exists and doesn't overlap "List USB Hard Drives",
# that enabling it asks for confirmation, that the system disk then appears tagged in the device
# list, and that trying to erase it brings up the extra warnings - the last one is answered "No",
# so nothing is ever written. Screenshots of every step go to OutDir.
param(
	[Parameter(Mandatory = $true)][string]$Exe,
	[Parameter(Mandatory = $true)][string]$OutDir,
	[string]$Locale = 'en-US',
	[switch]$RequireUpdateSignature
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class W {
	[DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
	[DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, int m, IntPtr w, IntPtr l);
	[DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")] public static extern IntPtr SendMessageSB(IntPtr h, int m, IntPtr w, StringBuilder l);
	[DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, int m, IntPtr w, IntPtr l);
	[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
	[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
	[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
	public delegate bool EnumProc(IntPtr h, IntPtr l);
	[DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
	[DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
	public static string AllText(IntPtr h) {
		var sb = new StringBuilder(Text(h));
		EnumChildWindows(h, (c, l) => { var t = Text(c); if (t.Length > 0) sb.Append(" | ").Append(t); return true; }, IntPtr.Zero);
		return sb.ToString();
	}
	public struct RECT { public int Left, Top, Right, Bottom; }
	public static string Text(IntPtr h) { var sb = new StringBuilder(8192); GetWindowTextW(h, sb, 8192); return sb.ToString(); }
	public static string Class(IntPtr h) { var sb = new StringBuilder(256); GetClassNameW(h, sb, 256); return sb.ToString(); }
	public static List<IntPtr> Windows(uint pid, bool visibleOnly) {
		var list = new List<IntPtr>();
		EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && (!visibleOnly || IsWindowVisible(h))) list.Add(h); return true; }, IntPtr.Zero);
		return list;
	}
	public static string EditText(IntPtr h) {
		int len = (int)SendMessageW(h, 0x000E, IntPtr.Zero, IntPtr.Zero);
		var sb = new StringBuilder(len + 2); SendMessageSB(h, 0x000D, (IntPtr)(len + 1), sb); return sb.ToString();
	}
	public static string CbItem(IntPtr cb, int i) {
		int len = (int)SendMessageW(cb, 0x0149, (IntPtr)i, IntPtr.Zero);
		var sb = new StringBuilder(len + 2); SendMessageSB(cb, 0x0148, (IntPtr)i, sb); return sb.ToString();
	}
}
'@

$IDC_DEVICE = 1001; $IDC_LIST_USB_HDD = 1022; $IDC_LIST_INTERNAL = 1990; $IDC_LOG_EDIT = 1055
$IDC_TS_CREDIT = 1991; $IDC_ABOUT = 1052
$WM_COMMAND = 0x0111; $WM_SYSKEYDOWN = 0x0104; $BM_CLICK = 0x00F5; $BM_SETCHECK = 0x00F1
$CB_GETCOUNT = 0x0146; $CB_SETCURSEL = 0x014E; $CB_SHOWDROPDOWN = 0x014F
$IDOK = 1; $IDCANCEL = 2; $IDYES = 6; $IDNO = 7
$he = $Locale -like 'he*'
$sysTag = if ($he) { 'דיסק המערכת' } else { 'SYSTEM DISK' }
$failures = New-Object System.Collections.Generic.List[string]
function Fail($m) { Write-Host "::error::[$Locale] $m"; $failures.Add($m) }
function Note($m) { Write-Host "[$Locale] $m" }

New-Item -ItemType Directory -Force $OutDir | Out-Null
$shot = 0
function Shot($name) {
	$script:shot++
	$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
	$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
	$g = [System.Drawing.Graphics]::FromImage($bmp)
	$g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
	$file = Join-Path $OutDir ('{0}-{1:D2}-{2}.png' -f $Locale, $script:shot, $name)
	$bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
	$g.Dispose(); $bmp.Dispose()
	Note "screenshot $file"
}

# --- Settings: advanced drive properties shown, no first-run prompts, verbose update check due now
$key = 'HKCU:\Software\Akeo Consulting\Rufus'
New-Item -Force $key | Out-Null
New-Item -Force 'HKCU:\Software\Microsoft\VisualStudio' -ErrorAction SilentlyContinue | Out-Null	# skips the "unofficial build" prompt
Set-ItemProperty $key ShowAdvancedDriveProperties 1 -Type DWord
Set-ItemProperty $key UpdateCheckInterval 86400 -Type DWord
Set-ItemProperty $key VerboseUpdateCheck 1 -Type DWord
Set-ItemProperty $key Locale $Locale -Type String
Remove-ItemProperty $key LastUpdateCheck -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $Exe -PassThru
$main = [IntPtr]::Zero
for ($i = 0; $i -lt 120 -and $main -eq [IntPtr]::Zero; $i++) {
	Start-Sleep -Milliseconds 500
	foreach ($h in [W]::Windows($proc.Id, $true)) {
		if (([W]::Text($h)) -like 'Rufus*' -and [W]::GetDlgItem($h, $IDC_DEVICE) -ne [IntPtr]::Zero) { $main = $h }
	}
}
if ($main -eq [IntPtr]::Zero) { throw "Rufus main window did not show up" }
Note "main window: $([W]::Text($main))"
Start-Sleep 4
[W]::SetForegroundWindow($main) | Out-Null
Shot 'main'

# --- 1. The new checkbox
$hUsb = [W]::GetDlgItem($main, $IDC_LIST_USB_HDD)
$hInt = [W]::GetDlgItem($main, $IDC_LIST_INTERNAL)
if ($hInt -eq [IntPtr]::Zero) { throw "Internal drives checkbox (1990) not found" }
if (-not [W]::IsWindowVisible($hInt)) { Fail "Internal drives checkbox is not visible" }
$ru = New-Object W+RECT; $ri = New-Object W+RECT; $rm = New-Object W+RECT
[W]::GetWindowRect($hUsb, [ref]$ru) | Out-Null; [W]::GetWindowRect($hInt, [ref]$ri) | Out-Null; [W]::GetWindowRect($main, [ref]$rm) | Out-Null
Note ("USB HDD '{0}' [{1},{2}-{3},{4}]  Internal '{5}' [{6},{7}-{8},{9}]" -f [W]::Text($hUsb), $ru.Left, $ru.Top, $ru.Right, $ru.Bottom, [W]::Text($hInt), $ri.Left, $ri.Top, $ri.Right, $ri.Bottom)
$overlap = ($ru.Left -lt $ri.Right) -and ($ri.Left -lt $ru.Right) -and ($ru.Top -lt $ri.Bottom) -and ($ri.Top -lt $ru.Bottom)
if ($overlap) { Fail "The two checkboxes overlap" }
if ($ri.Left -lt $rm.Left -or $ri.Right -gt $rm.Right) { Fail "Internal drives checkbox is outside of the window" }
if ($ri.Top -ne $ru.Top) { Fail "Internal drives checkbox is not on the same row as List USB Hard Drives" }
$expected = if ($he) { 'כוננים פנימיים' } else { 'Internal drives' }
if (-not ([W]::Text($hInt)).Contains($expected)) { Fail "Unexpected checkbox text '$([W]::Text($hInt))'" }

# --- 1b. Edited-version credit: title, button
if (-not ([W]::Text($main)).Contains('tsoolgee edition')) { Fail "Window title does not say 'tsoolgee edition'" }
$hCredit = [W]::GetDlgItem($main, $IDC_TS_CREDIT)
if ($hCredit -eq [IntPtr]::Zero -or -not [W]::IsWindowVisible($hCredit)) { Fail "Credit button not found / not visible" } else {
	$rc = New-Object W+RECT; [W]::GetWindowRect($hCredit, [ref]$rc) | Out-Null
	Note ("credit button '{0}' [{1},{2}-{3},{4}]" -f [W]::Text($hCredit), $rc.Left, $rc.Top, $rc.Right, $rc.Bottom)
	if (-not ([W]::Text($hCredit)).Contains('צול גאה')) { Fail "Credit button text '$([W]::Text($hCredit))'" }
	if ($rc.Left -lt $rm.Left -or $rc.Right -gt $rm.Right) { Fail "Credit button is outside of the window" }
}

# --- Dialog helpers
$seen = @{}
function Wait-Dialog([int]$timeoutSec = 30) {
	for ($i = 0; $i -lt $timeoutSec * 4; $i++) {
		foreach ($h in [W]::Windows($proc.Id, $true)) {
			if ($h -ne $main -and -not $seen.ContainsKey($h) -and [W]::Class($h) -eq '#32770') {
				$seen[$h] = $true
				Start-Sleep -Milliseconds 700
				# Upstream's own "DBX update available" prompt can pop up at any time on older
				# versions: decline it and keep waiting for the dialog we are after
				if ([W]::AllText($h) -match 'DBX') {
					Note "declining upstream DBX update prompt"
					Shot 'dbx-prompt'
					if (-not (Press $h $IDNO)) { Press $h $IDCANCEL | Out-Null }
					continue
				}
				return $h
			}
		}
		Start-Sleep -Milliseconds 250
	}
	return [IntPtr]::Zero
}
function Dialog-Text($h) { return [W]::AllText($h) }	# title + every child control (MessageBox or Rufus notification)
function Press($h, $id) {
	$b = [W]::GetDlgItem($h, $id)
	if ($b -eq [IntPtr]::Zero) { return $false }
	[W]::PostMessageW($b, $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
	Start-Sleep -Milliseconds 800
	return $true
}

# --- 1c. About box carries the "edited version" notice
[W]::PostMessageW($main, $WM_COMMAND, [IntPtr]$IDC_ABOUT, [IntPtr]::Zero) | Out-Null
$d = Wait-Dialog 15
if ($d -eq [IntPtr]::Zero) { Fail "About box did not open" } else {
	Start-Sleep 1
	Shot 'about'
	# RichEdit text via WM_GETTEXT
	$blurb = [W]::EditText([W]::GetDlgItem($d, 1033))
	Note "about: $($blurb -replace "`r?`n", ' / ')"
	if (-not $blurb.Contains('צול גאה')) { Fail "About box has no credit" }
	if (-not ($blurb.Contains('tsoolgee') -or $blurb.Contains('מהדורה ערוכה'))) { Fail "About box does not say it is an edited version" }
	Press $d $IDOK | Out-Null
}

# --- 2. Enabling internal drives asks first
[W]::SendMessageW($hInt, $BM_SETCHECK, [IntPtr]1, [IntPtr]::Zero) | Out-Null
[W]::PostMessageW($main, $WM_COMMAND, [IntPtr]$IDC_LIST_INTERNAL, $hInt) | Out-Null	# BN_CLICKED = 0
$d = Wait-Dialog 15
if ($d -eq [IntPtr]::Zero) { Fail "No confirmation when enabling internal drives" } else {
	Note ("confirmation: " + (Dialog-Text $d))
	Shot 'enable-internal-warning'
	Press $d $IDYES | Out-Null
}
Start-Sleep 6

# --- 3. The device list now holds the system disk, tagged
$cb = [W]::GetDlgItem($main, $IDC_DEVICE)
$n = [int][W]::SendMessageW($cb, $CB_GETCOUNT, [IntPtr]::Zero, [IntPtr]::Zero)
$sysIndex = -1
for ($i = 0; $i -lt $n; $i++) {
	$t = [W]::CbItem($cb, $i); Note "device[$i] = $t"
	if ($t.Contains($sysTag) -and $sysIndex -lt 0) { $sysIndex = $i }
}
[W]::SendMessageW($cb, $CB_SHOWDROPDOWN, [IntPtr]1, [IntPtr]::Zero) | Out-Null
Start-Sleep 1; Shot 'device-list'
[W]::SendMessageW($cb, $CB_SHOWDROPDOWN, [IntPtr]0, [IntPtr]::Zero) | Out-Null
if ($sysIndex -lt 0) { Fail "System disk not listed / not tagged '$sysTag'" }

# --- 4. Trying to erase the system disk: extra warnings, the last one answered No
if ($sysIndex -ge 0) {
	[W]::SendMessageW($cb, $CB_SETCURSEL, [IntPtr]$sysIndex, [IntPtr]::Zero) | Out-Null
	[W]::PostMessageW($main, $WM_COMMAND, [IntPtr](($IDC_DEVICE) -bor (1 -shl 16)), $cb) | Out-Null	# CBN_SELCHANGE
	Start-Sleep 3
	Shot 'system-disk-selected'
	[W]::PostMessageW($main, $WM_SYSKEYDOWN, [IntPtr][int][char]'Z', [IntPtr]0x20000000) | Out-Null	# Alt-Z = zero drive
	$got = @{ internal = $false; system = $false; last = $false }
	for ($k = 0; $k -lt 8; $k++) {
		$d = Wait-Dialog 30
		if ($d -eq [IntPtr]::Zero) { break }
		$t = Dialog-Text $d
		Note "dialog: $t"
		if ($t -match 'Last confirmation|אישור אחרון') {
			$got.last = $true; Shot 'last-confirmation'
			Press $d $IDNO | Out-Null	# never actually erase anything
			break
		} elseif ($t -match 'DANGER - System disk|סכנה - דיסק המערכת') {
			$got.system = $true; Shot 'system-disk-warning'; Press $d $IDYES | Out-Null
		} elseif ($t -match 'WARNING - Internal drive|אזהרה - כונן פנימי') {
			$got.internal = $true; Shot 'internal-drive-warning'; Press $d $IDYES | Out-Null
		} elseif ($t -match 'ALL DATA|WILL BE DESTROYED|יימחק|יושמד') {
			Shot 'unexpected-destroy-prompt'
			Fail "Reached the regular 'all data will be destroyed' prompt without our warnings"
			if (-not (Press $d $IDCANCEL)) { Press $d $IDNO | Out-Null }
			break
		} elseif ($t -match 'Another program or process|יישום או תהליך אחר') {
			# "processes are accessing this drive" comes before our warnings - go on
			Shot 'drive-in-use'
			Press $d $IDYES | Out-Null
		} else {
			Shot 'unexpected-dialog'
			Fail "Unexpected dialog: $t"
			if (-not (Press $d $IDCANCEL)) { Press $d $IDNO | Out-Null }
			break
		}
	}
	if (-not $got.internal) { Fail "No 'internal drive' warning" }
	if (-not $got.system) { Fail "No 'system disk' warning" }
	if (-not $got.last) { Fail "No last confirmation for the system disk" }
	Start-Sleep 2
	Shot 'after-cancel'
}

# --- 5. Log: update check from the fork, signature valid
$log = ''
for ($i = 0; $i -lt 90; $i++) {
	foreach ($h in [W]::Windows($proc.Id, $true)) {
		if ($h -ne $main -and [W]::Class($h) -eq '#32770' -and ([W]::AllText($h) -match 'DBX')) {
			Note "declining upstream DBX update prompt"
			if (-not (Press $h $IDNO)) { Press $h $IDCANCEL | Out-Null }
		}
	}
	foreach ($h in [W]::Windows($proc.Id, $false)) {
		$e = [W]::GetDlgItem($h, $IDC_LOG_EDIT)
		if ($e -ne [IntPtr]::Zero) { $log = [W]::EditText($e) }
	}
	if ($log -match 'Version signature is|Could not find a release version file|No new release|New release version found') { break }
	Start-Sleep 1
}
$log | Out-File -Encoding utf8 (Join-Path $OutDir "$Locale-log.txt")
# The update check part of the log, for the workflow output
($log -split "`r?`n") | Where-Object { $_ -match 'update|version file|signature|Found match|key\)' } | ForEach-Object { Note "log: $_" }
if ($log -notmatch 'github\.com/tsoolgee/rufus-tsoolgee-edition/releases/latest/download') { Fail "Update check does not use the fork's update-info" }
if ($log -match 'Version signature is invalid') { Fail "Update file signature is INVALID" }
if ($RequireUpdateSignature -and $log -notmatch 'Version signature is valid') { Fail "Update file signature was not validated" }
if ($log -match 'Version signature is valid') { Note "update signature valid" }

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
if ($failures.Count -gt 0) { Write-Host "[$Locale] $($failures.Count) failure(s)"; exit 1 }
Note "all checks passed"
exit 0
