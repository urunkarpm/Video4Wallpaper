$sh = New-Object -ComObject WScript.Shell
$destPath = [System.IO.Path]::Combine([Environment]::GetFolderPath("Desktop"), "WallpaperEngine.lnk")
$sc = $sh.CreateShortcut($destPath)
$sc.TargetPath = [System.IO.Path]::Combine([Environment]::GetFolderPath("LocalApplicationData"), "WallpaperEngine", "WallpaperEngine.exe")
$sc.WorkingDirectory = [System.IO.Path]::Combine([Environment]::GetFolderPath("LocalApplicationData"), "WallpaperEngine")
$sc.IconLocation = [System.IO.Path]::Combine([Environment]::GetFolderPath("LocalApplicationData"), "WallpaperEngine", "WallpaperEngine.exe") + ",0"
$sc.Description = "Windows Live Wallpaper Engine"
$sc.Save()
Write-Host "Created user desktop shortcut at: $destPath"
