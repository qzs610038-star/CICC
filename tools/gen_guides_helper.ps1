param (
    [string]$BranchName = "dev_cpu"
)

# 确保在仓库根目录运行
$repoRoot = (git rev-parse --show-toplevel 2>$null)
if (-not $repoRoot) {
    Write-Error "当前目录不是一个 Git 仓库，请在 Git 项目内运行此脚本。"
    exit 1
}

# 获取当前日期
$dateStr = Get-Date -Format "yyyy-MM-dd"
# 按照格式一：日期+合并分支名 命名子目录
$dirName = "${dateStr}_merge_${BranchName}"

$targetDir = Join-Path $repoRoot "learning_guides\daily_sync\${dirName}"
if (-not (Test-Path $targetDir)) {
    New-Item -ItemType Directory -Path $targetDir | Out-Null
}

Write-Host "正在提取自 origin/main 到本地分支的 Git 代码变更..." -ForegroundColor Cyan

# 输出 Git 提交日志摘要
$logFile = Join-Path $targetDir "git_log_summary.txt"
git log origin/main..HEAD --oneline --graph --decorate 2>$null | Out-File -FilePath $logFile -Encoding utf8

# 输出差异文件列表及 diff
$diffFile = Join-Path $targetDir "git_diff_patch.txt"
git diff origin/main HEAD 2>$null | Out-File -FilePath $diffFile -Encoding utf8

# 输出修改的文件列表
$changedFiles = Join-Path $targetDir "changed_files.txt"
git diff origin/main HEAD --name-status 2>$null | Out-File -FilePath $changedFiles -Encoding utf8

Write-Host "=============================================" -ForegroundColor Green
Write-Host "Git 变更提取成功！已存放在：" -ForegroundColor Green
Write-Host "  $targetDir" -ForegroundColor Yellow
Write-Host "在与 AI 对话时，只需发送以下内容即可一键生成讲解指南：" -ForegroundColor Green
Write-Host "  '请帮我根据 $targetDir 中的改动，为今日合并生成三轨学习指南。'" -ForegroundColor Yellow
Write-Host "=============================================" -ForegroundColor Green
