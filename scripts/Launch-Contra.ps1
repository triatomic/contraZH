[CmdletBinding()]
param(
    [string]$ExePath = 'C:\Games\contra\contraprerelease\generalszh_c.exe'
)

$ErrorActionPreference = 'Stop'

function Start-Contra([string]$Executable, [string]$CommandLine, [hashtable]$Environment)
{
    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf))
    {
        throw "Executable not found at '$Executable'."
    }

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = (Get-Item -LiteralPath $Executable).FullName
    $startInfo.WorkingDirectory = Split-Path -Parent $startInfo.FileName
    $startInfo.Arguments = $CommandLine
    $startInfo.UseShellExecute = $false

    foreach ($name in $Environment.Keys)
    {
        if ([string]::IsNullOrWhiteSpace($name) -or $name.Contains('=') -or $name.Contains([char]0))
        {
            throw "Invalid environment variable name '$name'."
        }
        $startInfo.EnvironmentVariables[$name] = [string]$Environment[$name]
    }

    [System.Diagnostics.Process]::Start($startInfo)
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$form = New-Object System.Windows.Forms.Form
$form.Text = 'Launch Contra'
$form.ClientSize = New-Object System.Drawing.Size(1080, 680)
$form.MinimumSize = New-Object System.Drawing.Size(820, 460)
$form.StartPosition = 'CenterScreen'
$form.AutoScaleMode = 'Dpi'

$layout = New-Object System.Windows.Forms.TableLayoutPanel
$layout.Dock = 'Fill'
$layout.Padding = New-Object System.Windows.Forms.Padding(12)
$layout.ColumnCount = 3
$layout.RowCount = 7
[void]$layout.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle('Absolute', 80)))
[void]$layout.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle('Percent', 100)))
[void]$layout.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle('Absolute', 90)))
for ($i = 0; $i -lt 7; ++$i)
{
    $row = New-Object System.Windows.Forms.RowStyle
    $row.SizeType = 'AutoSize'
    if ($i -eq 3)
    {
        $row.SizeType = 'Percent'
        $row.Height = 100
    }
    [void]$layout.RowStyles.Add($row)
}
$form.Controls.Add($layout)

$exeLabel = New-Object System.Windows.Forms.Label
$exeLabel.Text = 'Executable'
$exeLabel.Dock = 'Fill'
$exeLabel.TextAlign = 'MiddleLeft'
$layout.Controls.Add($exeLabel, 0, 0)

$exeEntry = New-Object System.Windows.Forms.TextBox
$exeEntry.Text = $ExePath
$exeEntry.Dock = 'Fill'
$layout.Controls.Add($exeEntry, 1, 0)

$browse = New-Object System.Windows.Forms.Button
$browse.Text = 'Browse...'
$browse.Dock = 'Fill'
$browse.Add_Click({
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Filter = 'Executables (*.exe)|*.exe'
    $dialog.FileName = $exeEntry.Text
    try
    {
        if ($dialog.ShowDialog($form) -eq 'OK')
        {
            $exeEntry.Text = $dialog.FileName
        }
    }
    finally
    {
        $dialog.Dispose()
    }
})
$layout.Controls.Add($browse, 2, 0)

$argsLabel = New-Object System.Windows.Forms.Label
$argsLabel.Text = 'Arguments'
$argsLabel.Dock = 'Fill'
$argsLabel.TextAlign = 'MiddleLeft'
$layout.Controls.Add($argsLabel, 0, 1)

$argsEntry = New-Object System.Windows.Forms.TextBox
$argsEntry.Dock = 'Fill'
$layout.Controls.Add($argsEntry, 1, 1)
$layout.SetColumnSpan($argsEntry, 2)

$envLabel = New-Object System.Windows.Forms.Label
$envLabel.Text = 'Engine environment variables for this launch'
$envLabel.AutoSize = $true
$envLabel.Padding = New-Object System.Windows.Forms.Padding(0, 10, 0, 6)
$layout.Controls.Add($envLabel, 0, 2)
$layout.SetColumnSpan($envLabel, 3)

$grid = New-Object System.Windows.Forms.DataGridView
$grid.Dock = 'Fill'
$grid.AutoSizeColumnsMode = 'Fill'
$grid.AutoSizeRowsMode = 'AllCells'
$grid.DefaultCellStyle.WrapMode = 'True'
$grid.BackgroundColor = [System.Drawing.SystemColors]::Window
$grid.AllowUserToResizeRows = $false
$enabledColumn = New-Object System.Windows.Forms.DataGridViewCheckBoxColumn
$enabledColumn.Name = 'Enabled'
$enabledColumn.HeaderText = 'Use'
$enabledColumn.FillWeight = 15
[void]$grid.Columns.Add($enabledColumn)
[void]$grid.Columns.Add('Name', 'Variable')
[void]$grid.Columns.Add('Value', 'Value')
[void]$grid.Columns.Add('Description', 'Values / effect')
$grid.Columns['Enabled'].FillWeight = 10
$grid.Columns['Name'].FillWeight = 105
$grid.Columns['Value'].FillWeight = 25
$grid.Columns['Description'].FillWeight = 170
$grid.Columns['Description'].ReadOnly = $true
$grid.Add_DefaultValuesNeeded({ param($sender, $eventArgs) $eventArgs.Row.Cells['Enabled'].Value = $true })
$variables = @(
    @('CONTRA_BLENDSORT', '0', '0 disables blend sorting; 1 batches; 2 fully sorts meshes per object.'),
    @('CONTRA_CLOUDSUN', '0', '0 uses legacy cloud darkening; 1 lets clouds block sunlight.'),
    @('CONTRA_COLORLUT', '0', '0 disables color grading; 1 allows it.'),
    @('CONTRA_CRYOSHADER', '0', '0 uses plain cryo effects; 1 allows cryo shading.'),
    @('CONTRA_D3D9EX', '0', '0 forces plain D3D9; 1 allows D3D9Ex.'),
    @('CONTRA_DISRUPTSHADER', '0', '0 disables disruption shading; 1 allows it.'),
    @('CONTRA_ELECTRICSHADER', '0', '0 uses plain electric sprites; 1 allows electric shading.'),
    @('CONTRA_FLAMESHADER', '0', '0 uses plain flames; 1 shades flames; 2 adds heat haze.'),
    @('CONTRA_GPU_SKINNING', '0', '0 uses CPU skinning; 1 uses GPU shadow depth; 2 adds base passes; 3 adds material passes.'),
    @('CONTRA_HEADLIGHTSHADER', '0', '0 uses headlight meshes; 1 allows headlight shaders.'),
    @('CONTRA_INSTANCING', '0', '0 draws meshes separately; 1 instances shadow depth; 2 adds base passes; 3 adds material passes.'),
    @('CONTRA_INTERPOLATION', '0', '0 disables movement interpolation; 1 allows it.'),
    @('CONTRA_LASERGLOW', '0', '0 uses dynamic lights for laser glow; 1 uses the glow shader.'),
    @('CONTRA_LASERSHADER', '0', '0 uses plain lasers; 1 allows laser shading.'),
    @('CONTRA_PARTICLE_INSTANCING', '0', '0 disables billboard particle instancing; 1 allows it.'),
    @('CONTRA_PIXELLIGHTS', '0', '0 uses vertex lighting; 1 enables per-pixel terrain lights; 2 adds units.'),
    @('CONTRA_PLANARMIRROR', '0', '0 draws mirror meshes plain; 1 allows mirror shading.'),
    @('CONTRA_SHADOWMAP', '0', '0 disables shadow maps; 1 renders depth only; 2 enables full shadow maps.'),
    @('CONTRA_SKYCLOUDS', '0', '0 uses the legacy cloud texture; 1 uses HQ sky clouds.'),
    @('CONTRA_SLOPEMAP', '0', '0 disables slope maps and their derived bumps; 1 allows them.'),
    @('CONTRA_SOFTPARTICLES', '0', '0 uses hard particles; 1 uses scene depth or terrain fallback; 2 uses terrain only.'),
    @('CONTRA_SPECULARRECEIVE', '0', '0 keeps a separate shadow receiver pass; 1 allows the combined specular receiver.'),
    @('CONTRA_SSAA', '100', 'Supersampling percentage. 100 uses native resolution; 150 uses 1.5x; 200 uses 2x.'),
    @('CONTRA_SSAO', '0', '0 disables ambient occlusion; 1 allows it.'),
    @('CONTRA_STORMSHADER', '0', '0 disables storm shaders; 1 allows them.'),
    @('CONTRA_TERRAINGLINT', '0', '0 disables terrain and road glint shaders; 1 allows them.'),
    @('CONTRA_UNITCLOUDS', '0', '0 disables cloud shadows on objects; 1 allows them.'),
    @('CONTRA_WATER', '0', '0 uses legacy water; 1 uses ps_2_a; 2 uses vertex waves per polygon; 3 uses waves on a polar grid.'),
    @('WB_PARALLEL', '0', 'WorldBuilder only. 0 forces serial work; other values use automatic worker selection.')
)
foreach ($variable in $variables)
{
    $inherited = [Environment]::GetEnvironmentVariable($variable[0])
    $value = $variable[1]
    if ($null -ne $inherited)
    {
        $value = $inherited
    }
    $index = $grid.Rows.Add($false, $variable[0], $value, $variable[2])
    $grid.Rows[$index].Cells['Name'].ReadOnly = $true
}
$layout.Controls.Add($grid, 0, 3)
$layout.SetColumnSpan($grid, 3)

$helpLabel = New-Object System.Windows.Forms.Label
$helpLabel.Text = 'Tick Use to override a variable. Unchecked rows keep inherited settings. Most rendering controls require D3D9. Add custom variables in the blank row.'
$helpLabel.AutoSize = $true
$helpLabel.MaximumSize = New-Object System.Drawing.Size(1040, 0)
$helpLabel.Padding = New-Object System.Windows.Forms.Padding(0, 6, 0, 6)
$layout.Controls.Add($helpLabel, 0, 4)
$layout.SetColumnSpan($helpLabel, 3)

$launch = New-Object System.Windows.Forms.Button
$launch.Text = 'Launch Contra'
$launch.AutoSize = $true
$launch.Anchor = 'Right'
$launch.Add_Click({
    try
    {
        [void]$grid.EndEdit()
        $overrides = @{}
        foreach ($row in $grid.Rows)
        {
            if ($row.IsNewRow -or -not $row.Cells['Enabled'].Value)
            {
                continue
            }
            $name = ([string]$row.Cells['Name'].Value).Trim()
            if ($overrides.ContainsKey($name))
            {
                throw "Variable '$name' appears more than once."
            }
            $overrides[$name] = [string]$row.Cells['Value'].Value
        }
        $game = Start-Contra $exeEntry.Text $argsEntry.Text $overrides
        $status.Text = "Started Contra with PID $($game.Id)."
        $game.Dispose()
    }
    catch
    {
        $status.Text = $_.Exception.Message
        [void][System.Windows.Forms.MessageBox]::Show($form, $_.Exception.Message, 'Could not launch Contra', 'OK', 'Error')
    }
})
$layout.Controls.Add($launch, 0, 5)
$layout.SetColumnSpan($launch, 3)

$status = New-Object System.Windows.Forms.Label
$status.Text = 'Overrides apply only to the game process. Other variables are inherited.'
$status.AutoSize = $true
$status.Padding = New-Object System.Windows.Forms.Padding(0, 6, 0, 0)
$layout.Controls.Add($status, 0, 6)
$layout.SetColumnSpan($status, 3)

[void]$form.ShowDialog()
$form.Dispose()
