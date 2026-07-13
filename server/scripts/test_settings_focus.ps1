param(
    [int]$SettingsActivations = 100,
    [int]$DelayMs = 600,
    [int]$SampleMs = 350,
    [ValidateSet('Mouse', 'AltTab')]
    [string]$SwitchMode = 'Mouse'
)

$ErrorActionPreference = 'Stop'

Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class SettingsFocusNative {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }

    [StructLayout(LayoutKind.Sequential)]
    public struct WINDOWPLACEMENT {
        public int length;
        public int flags;
        public int showCmd;
        public POINT ptMinPosition;
        public POINT ptMaxPosition;
        public RECT rcNormalPosition;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct GUITHREADINFO {
        public int cbSize;
        public int flags;
        public IntPtr hwndActive;
        public IntPtr hwndFocus;
        public IntPtr hwndCapture;
        public IntPtr hwndMenuOwner;
        public IntPtr hwndMoveSize;
        public IntPtr hwndCaret;
        public RECT rcCaret;
    }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")]
    public static extern bool GetWindowPlacement(IntPtr hwnd, ref WINDOWPLACEMENT placement);
    [DllImport("user32.dll")]
    public static extern bool SetWindowPlacement(IntPtr hwnd, ref WINDOWPLACEMENT placement);
    [DllImport("user32.dll")]
    public static extern bool MoveWindow(IntPtr hwnd, int x, int y, int width, int height, bool repaint);
    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hwnd, IntPtr insertAfter, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hwnd, int command);
    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")]
    public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extraInfo);
    [DllImport("user32.dll")]
    public static extern void keybd_event(byte virtualKey, byte scanCode, uint flags, UIntPtr extraInfo);
    [DllImport("user32.dll")]
    public static extern bool GetGUIThreadInfo(uint threadId, ref GUITHREADINFO info);
    [DllImport("user32.dll")]
    public static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")]
    public static extern IntPtr WindowFromPoint(POINT point);
    [DllImport("user32.dll")]
    public static extern IntPtr GetAncestor(IntPtr hwnd, uint flags);

    public static IntPtr[] TopLevelWindows() {
        var windows = new List<IntPtr>();
        EnumWindows((hwnd, _) => { windows.Add(hwnd); return true; }, IntPtr.Zero);
        return windows.ToArray();
    }
}
'@

function Get-WindowInfo([IntPtr]$Hwnd) {
    $text = [Text.StringBuilder]::new(512)
    [void][SettingsFocusNative]::GetWindowText($Hwnd, $text, $text.Capacity)
    $pidValue = [uint32]0
    $threadId = [SettingsFocusNative]::GetWindowThreadProcessId($Hwnd, [ref]$pidValue)
    [pscustomobject]@{
        Hwnd = $Hwnd
        Pid = $pidValue
        ThreadId = $threadId
        Title = $text.ToString()
        Visible = [SettingsFocusNative]::IsWindowVisible($Hwnd)
    }
}

function Invoke-RealClick([IntPtr]$Hwnd) {
    $rect = [SettingsFocusNative+RECT]::new()
    if (-not [SettingsFocusNative]::GetWindowRect($Hwnd, [ref]$rect)) {
        throw "GetWindowRect failed for $Hwnd"
    }
    $x = [int](($rect.Left + $rect.Right) / 2)
    # Click below Chromium's tab/title strip and the settings custom title bar.
    $y = [Math]::Min($rect.Bottom - 80, $rect.Top + 180)
    [void][SettingsFocusNative]::SetCursorPos($x, $y)
    Start-Sleep -Milliseconds 80
    $point = [SettingsFocusNative+POINT]::new()
    $point.X = $x
    $point.Y = $y
    $windowAtPoint = [SettingsFocusNative]::WindowFromPoint($point)
    $rootAtPoint = [SettingsFocusNative]::GetAncestor($windowAtPoint, 2)
    [SettingsFocusNative]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    [SettingsFocusNative]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    return [pscustomobject]@{
        X = $x
        Y = $y
        Window = $windowAtPoint.ToInt64()
        RootWindow = $rootAtPoint.ToInt64()
    }
}

function Test-Activation([IntPtr]$Target, [IntPtr]$Settings, [uint32]$SettingsThreadId) {
    if ($SwitchMode -eq 'AltTab') {
        [SettingsFocusNative]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
        [SettingsFocusNative]::keybd_event(0x09, 0, 0, [UIntPtr]::Zero)
        [SettingsFocusNative]::keybd_event(0x09, 0, 2, [UIntPtr]::Zero)
        [SettingsFocusNative]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
        $clickPoint = [pscustomobject]@{ Method = 'AltTab' }
    }
    else {
        $clickPoint = Invoke-RealClick $Target
    }

    $samples = [Math]::Max(1, [int]($SampleMs / 10))
    $foregrounds = [Collections.Generic.List[IntPtr]]::new()
    $activeWindows = [Collections.Generic.List[IntPtr]]::new()
    $focusWindows = [Collections.Generic.List[IntPtr]]::new()
    for ($sample = 0; $sample -lt $samples; $sample++) {
        $foregrounds.Add([SettingsFocusNative]::GetForegroundWindow())
        $info = [SettingsFocusNative+GUITHREADINFO]::new()
        $info.cbSize = [Runtime.InteropServices.Marshal]::SizeOf($info)
        if ([SettingsFocusNative]::GetGUIThreadInfo($SettingsThreadId, [ref]$info)) {
            $activeWindows.Add($info.hwndActive)
            $focusWindows.Add($info.hwndFocus)
        }
        Start-Sleep -Milliseconds 10
    }

    $targetValue = $Target.ToInt64()
    $foregroundValues = @($foregrounds | ForEach-Object { $_.ToInt64() })
    $activeValues = @($activeWindows | ForEach-Object { $_.ToInt64() })
    $focusValues = @($focusWindows | ForEach-Object { $_.ToInt64() })
    $targetReached = $foregroundValues -contains $targetValue
    $leftAfterReach = $false
    if ($targetReached) {
        $first = [Array]::IndexOf($foregroundValues, $targetValue)
        for ($index = $first; $index -lt $foregroundValues.Count; $index++) {
            if ($foregroundValues[$index] -ne $targetValue) { $leftAfterReach = $true; break }
        }
    }

    $finalForeground = $foregroundValues[$foregroundValues.Count - 1]
    $finalActive = if ($activeValues.Count) { $activeValues[$activeValues.Count - 1] } else { 0 }
    $finalFocus = if ($focusValues.Count) { $focusValues[$focusValues.Count - 1] } else { 0 }
    $requiresSettingsFocus = $targetValue -eq $Settings.ToInt64()
    # MoveFocus transfers keyboard focus from the host HWND into WebView2's
    # Composition input HWND, so a non-zero focus HWND is expected here.
    $settingsFocusCorrect = -not $requiresSettingsFocus -or `
        ($finalActive -eq $Settings.ToInt64() -and $finalFocus -ne 0)

    [pscustomobject]@{
        Passed = $targetReached -and -not $leftAfterReach -and $finalForeground -eq $targetValue -and $settingsFocusCorrect
        TargetReached = $targetReached
        LeftAfterReach = $leftAfterReach
        Target = $targetValue
        Click = $clickPoint
        FinalForeground = $finalForeground
        FinalActive = $finalActive
        FinalFocus = $finalFocus
        SettingsFocusCorrect = $settingsFocusCorrect
        SettingsActiveChanges = @($activeWindows | Select-Object -Unique).Count
        SettingsFocusChanges = @($focusWindows | Select-Object -Unique).Count
    }
}

$windows = [SettingsFocusNative]::TopLevelWindows() | ForEach-Object { Get-WindowInfo $_ }
$settingsProcess = Get-Process MetasequoiaImeSettings -ErrorAction Stop | Select-Object -First 1
$settingsInfo = $windows | Where-Object {
    $_.Pid -eq $settingsProcess.Id -and $_.Title -eq 'Metasequoia IME Settings'
} | Select-Object -First 1
if (-not $settingsInfo) { throw 'Could not find the Settings top-level window.' }

$foreground = [SettingsFocusNative]::GetForegroundWindow()
$chromeInfo = $windows | Where-Object {
    $_.Visible -and $_.Title -and (Get-Process -Id $_.Pid -ErrorAction SilentlyContinue).ProcessName -eq 'chrome'
} | Sort-Object { if ($_.Hwnd -eq $foreground) { 0 } else { 1 } } | Select-Object -First 1
if (-not $chromeInfo) { throw 'Could not find a visible Chrome top-level window.' }

$chromePlacement = [SettingsFocusNative+WINDOWPLACEMENT]::new()
$chromePlacement.length = [Runtime.InteropServices.Marshal]::SizeOf($chromePlacement)
[void][SettingsFocusNative]::GetWindowPlacement($chromeInfo.Hwnd, [ref]$chromePlacement)
$settingsPlacement = [SettingsFocusNative+WINDOWPLACEMENT]::new()
$settingsPlacement.length = [Runtime.InteropServices.Marshal]::SizeOf($settingsPlacement)
[void][SettingsFocusNative]::GetWindowPlacement($settingsInfo.Hwnd, [ref]$settingsPlacement)
$screenWidth = [SettingsFocusNative]::GetSystemMetrics(0)
$screenHeight = [SettingsFocusNative]::GetSystemMetrics(1)
$halfWidth = [int]($screenWidth / 2)

# Arrange both ordinary windows side by side. TOPMOST is used only for the two
# setup clicks needed to put them above the IDE, and is removed before testing.
[void][SettingsFocusNative]::ShowWindow($chromeInfo.Hwnd, 9)
[void][SettingsFocusNative]::MoveWindow($chromeInfo.Hwnd, 0, 0, $halfWidth, $screenHeight - 80, $true)
[void][SettingsFocusNative]::ShowWindow($settingsInfo.Hwnd, 4)
[void][SettingsFocusNative]::MoveWindow($settingsInfo.Hwnd, $halfWidth, 0, $screenWidth - $halfWidth, $screenHeight - 80, $true)
[void][SettingsFocusNative]::SetWindowPos($settingsInfo.Hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x0013)
[void](Invoke-RealClick $settingsInfo.Hwnd)
[void][SettingsFocusNative]::SetWindowPos($chromeInfo.Hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x0013)
[void](Invoke-RealClick $chromeInfo.Hwnd)
Start-Sleep -Milliseconds 500
Write-Output ("chromeHwnd={0} chromeTitle={1} settingsHwnd={2} foreground={3} foregroundTitle={4}" -f `
    $chromeInfo.Hwnd.ToInt64(), $chromeInfo.Title, $settingsInfo.Hwnd.ToInt64(), `
    [SettingsFocusNative]::GetForegroundWindow().ToInt64(), `
    (Get-WindowInfo ([SettingsFocusNative]::GetForegroundWindow())).Title)

$failures = [Collections.Generic.List[string]]::new()
try {
    for ($iteration = 1; $iteration -le $SettingsActivations; $iteration++) {
        if ($SwitchMode -eq 'AltTab') {
            $settingsResult = Test-Activation $settingsInfo.Hwnd $settingsInfo.Hwnd $settingsInfo.ThreadId
            Start-Sleep -Milliseconds $DelayMs
            $chromeResult = Test-Activation $chromeInfo.Hwnd $settingsInfo.Hwnd $settingsInfo.ThreadId
        }
        else {
            $chromeResult = Test-Activation $chromeInfo.Hwnd $settingsInfo.Hwnd $settingsInfo.ThreadId
            Start-Sleep -Milliseconds $DelayMs
            $settingsResult = Test-Activation $settingsInfo.Hwnd $settingsInfo.Hwnd $settingsInfo.ThreadId
        }
        if (-not $chromeResult.Passed) {
            $failures.Add("$iteration Chrome: $($chromeResult | ConvertTo-Json -Compress)")
        }
        if (-not $settingsResult.Passed) {
            $failures.Add("$iteration Settings: $($settingsResult | ConvertTo-Json -Compress)")
        }
        Write-Output ("iteration={0} chrome={1} settings={2} transient={3} activeStates={4}" -f `
            $iteration, $chromeResult.Passed, $settingsResult.Passed, $settingsResult.LeftAfterReach, `
            $settingsResult.SettingsActiveChanges)
        Start-Sleep -Milliseconds $DelayMs
    }
}
finally {
    [void][SettingsFocusNative]::SetWindowPos($settingsInfo.Hwnd, [IntPtr](-2), 0, 0, 0, 0, 0x0013)
    [void][SettingsFocusNative]::SetWindowPos($chromeInfo.Hwnd, [IntPtr](-2), 0, 0, 0, 0, 0x0013)
    [void][SettingsFocusNative]::SetWindowPlacement($chromeInfo.Hwnd, [ref]$chromePlacement)
    [void][SettingsFocusNative]::SetWindowPlacement($settingsInfo.Hwnd, [ref]$settingsPlacement)
}

Write-Output "settings_activations=$SettingsActivations failures=$($failures.Count)"
$failures | ForEach-Object { Write-Output "FAIL $_" }
if ($failures.Count -gt 0) { exit 1 }
