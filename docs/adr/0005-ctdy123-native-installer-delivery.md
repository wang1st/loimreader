# ADR-0005：公开源码与 ctdy123 原生安装包交付边界

## 状态

已接受

## 背景

LoimReader 源码仓库暂时保持公开，但面向用户的 Windows、Linux、macOS 安装包需要统一从 `ctdy123.com/download/loimreader` 分发。GitHub Release 链接不是稳定的产品交付边界，也无法直接兼容未来仓库可见性、商业授权和下载统计策略。旧 Qt 客户端已经依赖现有版本查询、登录和心跳响应中的更新字段，其中 `checksumMD5` 会被实际当作 MD5 校验。

ctdy123.com 与 z-pulse.cn 共用一台 ECS，因此桌面发布不能通过 SSH 覆盖站点目录、重启 Nginx 或接触 z-pulse 服务。

## 决策

1. GitHub Actions 在六个原生运行器上分别生成 MSI、DEB、DMG，并继续创建带 SBOM 和签名清单的 GitHub Release。
2. 签名清单中的用户下载地址固定为 `https://ctdy123.com/download/loimreader/{platform}/{arch}/{filename}`，不把 GitHub Release 地址暴露为升级地址。
3. 发布工作流使用现有环境级 Token 和 Ed25519 签名调用 ctdy123 HTTPS API。网站返回短期 OSS 预签名上传地址，工作流直接上传安装包；GitHub 不持有 OSS 长期密钥。
4. 网站在流式复算文件大小、SHA-256 和 MD5 后，才原子更新 `updates/loimreader/version.json`。任一目标缺失或摘要不符时保留旧版本。
5. OSS Bucket 保持私有。稳定下载路由核对当前版本、平台、架构和文件名后，跳转到五分钟有效的签名地址。
6. 继续兼容旧 Qt 更新协议：
   - `GET /api/update/check` 保留 `releases[].platforms[]`；
   - `POST /api/client/version/check` 保留 `hasUpdate/updateInfo`；
   - 登录和心跳保留 `updateUrl/updateSize/updateLog/checksumMD5`；
   - `packages[platform]` 继续作为不带架构请求的兼容入口；
   - MD5 只服务旧客户端，新清单和新客户端以 SHA-256 为完整性依据。

## 后果

### 正面

- 网站下载页、旧客户端和新客户端共用同一稳定域名，不依赖 GitHub 仓库可见性。
- 六个安装包未全部上传并校验时不会切换最新版。
- 发布工作流不接触 ECS root，也不扩大共享服务器的故障范围。
- 公开源码与安装包分发解耦，为后续商业授权、下载统计和增值服务保留边界。

### 负面

- 每次发布同时占用 GitHub Actions、GitHub Release 和 OSS 存储流量。
- Windows 代码签名与 macOS Developer ID 公证仍需独立证书；没有证书时安装包可测试，但系统会显示发布者警告。
- 为旧 Qt 客户端保留 MD5 会增加一个兼容字段，但不把 MD5 当作现代安全校验。

### 中性

- GitHub 仓库本轮保持公开；以后是否私有不会改变 ctdy123 下载协议。
- GitHub Release 继续承担开发者归档和审计材料，不作为产品内更新地址。

## 失败模式

| 失败 | 行为 | 恢复 |
|---|---|---|
| 某一架构构建失败 | 不创建完整发布 | 修复后重新运行六目标构建 |
| OSS 上传中断 | 最新版本清单不变 | 重跑幂等发布工作流 |
| 文件大小或摘要不符 | 网站返回 422 并拒绝发布 | 重新上传对应安装包 |
| ctdy123 暂时不可用 | GitHub Release 可保留，客户端继续看到旧版本 | 运行登记重试工作流 |
| OSS 私有签名地址过期 | 稳定下载路由重新签发 | 用户重新点击原下载地址 |
| 旧客户端未上报架构 | Windows/Linux 默认 amd64，历史 macOS 默认 arm64 | 新客户端显式提交 `architecture` |

## 备选方案

- **直接给用户 GitHub Release URL**：拒绝；产品链接受仓库权限影响，且无法统一下载策略。
- **把 OSS Bucket 改为公共读**：拒绝；失去对象访问控制，也暴露存储边界。
- **让 GitHub Actions 保存 OSS 长期密钥**：拒绝；权限面过大，轮换和审计困难。
- **安装包经 Next.js/ECS 中转上传**：拒绝；会消耗共享 ECS 带宽和内存，并增加对 z-pulse 的资源干扰。
