Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap(64, 64)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

# Background dark rounded circle
$bgBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 18, 22, 32))
$g.FillEllipse($bgBrush, 2, 2, 60, 60)

# Cyan ring border
$pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 0, 220, 160), 3)
$g.DrawEllipse($pen, 3, 3, 58, 58)

# Play triangle
$playBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 0, 235, 165))
$p1 = New-Object System.Drawing.PointF(24, 18)
$p2 = New-Object System.Drawing.PointF(46, 32)
$p3 = New-Object System.Drawing.PointF(24, 46)
$pts = [System.Drawing.PointF[]]($p1, $p2, $p3)
$g.FillPolygon($playBrush, $pts)

$g.Dispose()
New-Item -ItemType Directory -Path 'src/res' -Force | Out-Null
$hIcon = $bmp.GetHicon()
$icon = [System.Drawing.Icon]::FromHandle($hIcon)
$stream = New-Object System.IO.FileStream('src/res/app.ico', [System.IO.FileMode]::Create)
$icon.Save($stream)
$stream.Close()
Write-Host "Generated src/res/app.ico successfully."
