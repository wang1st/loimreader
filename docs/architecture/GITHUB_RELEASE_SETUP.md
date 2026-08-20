# GitHub 自动发布与 ctdy123.com 登记

## 仓库设置

1. 源码仓库当前保持公开；标准运行器直接使用 `ubuntu-24.04-arm`、`windows-11-arm`、`macos-26` 等六个标签。产品下载地址不依赖仓库可见性。
2. 创建 GitHub Environment：`ctdy123-production`，启用人工审批和仅允许 `v3.*` 标签部署。
3. 在 Repository Secrets 中配置：
   - `RELEASE_ED25519_PRIVATE_KEY`：32 字节 Ed25519 私钥的 64 位十六进制表示。
4. 在 `ctdy123-production` Environment Secrets 中配置：
   - `CTDY123_RELEASE_TOKEN`：仅用于版本登记的随机 Token。
5. 网站进程环境配置：
   - `CTDY123_RELEASE_TOKEN`：与 GitHub Environment 相同。
   - `RELEASE_ED25519_PUBLIC_KEY`：对应公钥的 64 位十六进制表示。
   - `LOIMREADER_RELEASE_REPOSITORY`：实际 GitHub `owner/repository`。

本机 `/Users/ethan/secrets.json` 已包含 Ed25519 密钥和 `ADMIN_API_KEY`。它只能作为一次性初始化来源，不能提交到 Git、复制进工作流 YAML 或出现在命令日志中。生产环境应生成独立的 `CTDY123_RELEASE_TOKEN`，不要长期复用网站管理员密钥。

## 发布顺序

1. `ci.yml` 在六个原生目标上编译、测试并保存短期构建产物。
2. 推送 `v3.*` 标签触发 `release.yml`。
3. 六个 MSI/DEB/DMG 全部成功后生成 schema v2 清单、SPDX 2.3 SBOM、SHA-256 和旧 Qt 兼容 MD5。
4. 工作流用 Ed25519 私钥对清单原始字节签名，并创建不可变 GitHub Release。
5. 生产 Environment 审批后，工作流将原清单和签名提交到 ctdy123.com，取得六个短期 OSS 上传地址。
6. 工作流直传安装包到私有 OSS，不持有 OSS 长期密钥。
7. 网站验证 Token、签名、仓库、六目标、版本递增，并流式复算大小、SHA-256、MD5；全部一致后才更新 `updates/loimreader/version.json`。
8. 工作流从公开版本查询接口回读版本号和六个 ctdy123 下载地址，并实际跟随下载；不一致则整次发布失败。

若 GitHub Release 已经创建、只有网站登记失败，运行
`Retry ctdy123 release registration` 工作流并输入现有版本号。该工作流从 GitHub Release 下载已签名清单和六个安装包，幂等重传并轮询公开接口，不会重新构建。

## 旧 Qt 升级兼容

- `GET /api/update/check` 继续提供 `releases[].platforms[]`。
- `POST /api/client/version/check` 继续提供 `hasUpdate` 和 `updateInfo`。
- 登录与心跳响应继续提供 `updateUrl`、`updateSize`、`updateLog`、`checksumMD5`。
- 旧请求未携带架构时，Windows/Linux 默认 amd64；历史 Qt macOS 包默认 arm64。
- 用户下载和升级 URL 均为 `https://ctdy123.com/download/loimreader/...`，由网站跳转到短期私有 OSS 地址。

## 共享 ECS 隔离要求

- 此发布链路不需要 `CTDY123_ROOT_PASSWORD`、`ZPULSE_SERVER_ROOT_PASSWORD` 或 SSH 私钥。
- 不执行 `nginx reload`、`systemctl restart`、PM2 操作或站点目录同步。
- 当前 ctdy123 应用目录是 `/opt/limereader`，容器名是 `limereader-app`。不得把仓库里的旧 PM2 部署脚本用于这台共享 ECS。
- 网站代码应先在本地完成生产构建，再上传到独立候选目录并用 `127.0.0.1` 临时端口验证。切换时只能重建 `limereader-app`，不得执行整组 `docker compose down/up`。
- 切换前后都要记录 Redis、Vaultwarden 与 z-pulse 服务的容器/进程状态，并分别验证 `ctdy123.com`、`z-pulse.cn`。不得写入 z-pulse 的目录、服务单元或 Nginx 路由。
- 首次部署发布 API 属于网站变更，必须保留 `/opt/limereader-backups/<commit>` 回滚副本；候选目录中的环境文件应在验证完成后删除。

## 回滚

发布 API 默认阻止版本号倒退。若新版本需要撤回：

1. 在 GitHub 将 Release 标记为撤回，但保留审计材料。
2. 发布更高补丁版本指向修复包；不要把 `latestVersion` 改回旧值。
3. 只有紧急事故才能由网站管理员手工恢复 OSS 旧清单，并记录原因和清单哈希。
