# GitHub 自动发布与 ctdy123.com 登记

## 仓库设置

1. 将本仓库迁移到 GitHub；当前标准运行器可直接使用 `ubuntu-24.04-arm`、`windows-11-arm`、`macos-26` 等六个标签，私有仓库会消耗账户的 Actions 分钟额度。
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
3. 六个包全部成功后生成 schema v2 清单、SPDX 2.3 SBOM 和 SHA-256。
4. 工作流用 Ed25519 私钥对清单原始字节签名，并创建不可变 GitHub Release。
5. 生产 Environment 审批后，工作流将原清单和签名提交到 ctdy123.com。
6. 网站验证 Token、签名、仓库、六目标、版本递增，再更新 OSS `updates/loimreader/version.json`。
7. 工作流从公开版本查询接口回读版本号；不一致则整次发布失败。

## 共享 ECS 隔离要求

- 此发布链路不需要 `CTDY123_ROOT_PASSWORD`、`ZPULSE_SERVER_ROOT_PASSWORD` 或 SSH 私钥。
- 不执行 `nginx reload`、`systemctl restart`、PM2 操作或站点目录同步。
- ctdy123 网站代码本身的部署应使用独立 `ecs-user`、`/home/ecs-user/limereader` 和 `limereader-npm.service`；不得写入 z-pulse 的用户、目录或服务单元。
- 首次部署发布 API 属于网站变更，应沿用 ctdy123 的备份、构建、健康检查和单服务回滚流程。

## 回滚

发布 API 默认阻止版本号倒退。若新版本需要撤回：

1. 在 GitHub 将 Release 标记为撤回，但保留审计材料。
2. 发布更高补丁版本指向修复包；不要把 `latestVersion` 改回旧值。
3. 只有紧急事故才能由网站管理员手工恢复 OSS 旧清单，并记录原因和清单哈希。
