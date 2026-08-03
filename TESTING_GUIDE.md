# 软件测试指南

## 本地测试环境设置

### 1. Windows Sandbox (推荐 - 免费)
```powershell
# 启用Windows Sandbox (需要Windows 10/11 Pro)
Enable-WindowsOptionalFeature -Online -FeatureName "Containers-DisposableClientVM"
```

**优点：**
- 每次启动都是全新环境
- 启动快速
- 无需额外软件

**缺点：**
- 仅限Windows 10/11 Pro
- 功能相对简单

### 2. VirtualBox + Windows ISO
1. 下载VirtualBox: https://www.virtualbox.org/
2. 下载Windows ISO: https://www.microsoft.com/software-download/
3. 创建虚拟机：
   - 内存: 2GB+
   - 硬盘: 20GB+
   - 启用网络

### 3. VMware Player
1. 下载: https://www.vmware.com/products/workstation-player.html
2. 创建虚拟机配置
3. 安装Windows系统

## 测试步骤

### 1. 构建和打包
```powershell
# 清理并重新打包
.\build.ps1 clean
.\build.ps1 package

# 测试打包完整性
.\test_package.ps1
```

### 2. 在测试环境中验证
1. 将`dist`文件夹复制到测试环境
2. 运行`dist\run.cmd`
3. 检查是否出现依赖错误

### 3. 常见问题排查

#### 问题1: GetCurrentPackageFullName错误
**原因：** 缺少MinGW运行时库
**解决：** 确保打包时包含：
- libgcc_s_seh-1.dll
- libstdc++-6.dll
- libwinpthread-1.dll

#### 问题2: Qt DLL缺失
**原因：** windeployqt未正确部署
**解决：** 使用更完整的windeployqt参数

#### 问题3: 插件缺失
**原因：** Qt插件目录不完整
**解决：** 检查platforms、imageformats等目录

## 自动化测试脚本

### 创建测试虚拟机快照
1. 安装干净的Windows系统
2. 创建快照
3. 每次测试前恢复到快照

### 批量测试脚本
```powershell
# 测试多个Windows版本
$versions = @("Windows 10", "Windows 11", "Windows Server 2019")
foreach ($version in $versions) {
    # 启动对应虚拟机
    # 复制测试文件
    # 运行测试
    # 收集结果
}
```

## 云测试服务

### Azure Virtual Machines
```bash
# 创建测试VM
az vm create --resource-group test-rg --name test-vm --image Win2019Datacenter --admin-username testuser
```

### AWS EC2
```bash
# 启动Windows实例
aws ec2 run-instances --image-id ami-windows-2019 --instance-type t2.medium
```

## 测试检查清单

- [ ] 程序能正常启动
- [ ] 所有功能正常工作
- [ ] 没有依赖错误
- [ ] 文件关联正确
- [ ] 卸载程序正常
- [ ] 不同Windows版本兼容性
- [ ] 32位/64位兼容性

## 性能测试

### 内存使用
```powershell
# 监控内存使用
Get-Process ctdy123 | Select-Object ProcessName, WorkingSet, VirtualMemorySize
```

### 启动时间
```powershell
# 测量启动时间
Measure-Command { Start-Process "ctdy123.exe" -Wait }
```

## 报告模板

```
测试报告
========
测试环境: Windows 10 Pro (版本号)
测试时间: 2025-01-09
测试人员: [姓名]

测试结果:
- 启动测试: ✓/✗
- 功能测试: ✓/✗  
- 依赖检查: ✓/✗
- 性能测试: ✓/✗

问题记录:
1. [问题描述]
   - 重现步骤:
   - 预期结果:
   - 实际结果:
   - 解决方案:

建议:
- [改进建议]
```
