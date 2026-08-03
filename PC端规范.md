# PC 客户端鉴权与设备管理规范

本文档面向 PC 客户端（C++/Qt）开发，描述与服务端的鉴权与设备管理协议。

## 概览
- 登录入口：`POST /api/auth/client/login`
- 鉴权方式：JWT（HTTP-only Cookie 用于 Web，PC 端使用 Bearer Token）
- 设备模型：统一用户模型中的 `devices.active[]`，PC 端类型为 `client`
- 配额策略：未超限→创建设备；超限→返回 403，提示去网站管理设备（不自动踢）
- 会话标识：`sessionId == machineCode`（客户端机器码/硬件指纹）

---

## 1. 登录 API
### URL
```
POST /api/auth/client/login
Content-Type: application/json
```

### 请求体
```json
{
  "email": "user@example.com",
  "password": "******",
  "machineCode": "WIN-ABCDEF123456",       // 必填，≥8 位，设备唯一标识
  "deviceInfo": {                           // 可选
    "name": "我的台式机",
    "type": "client",                     // 固定 client（可省略）
    "userAgent": "MyApp/1.0 (Windows 11)",
    "os": "Windows 11",
    "appVersion": "1.0.3"
  }
}
```

### 成功响应 (200)
```json
{
  "success": true,
  "token": "<JWT>",                       // 后续放在 Authorization: Bearer <token>
  "user": {
    "id": "user_...",
    "email": "user@example.com",
    "subscription": { "type": "monthly", "active": true, "expiresAt": "2026-01-01T..." }, //过期或免费用户都会显示free
    "deviceLimit": 3,
    "currentDevices": 1
  },
  "sessionId": "WIN-ABCDEF123456",        // 即 machineCode
  "message": "登录成功"
}
```

### 失败响应
- 400 `VALIDATION_ERROR`：缺少参数或 `machineCode` 不合法
- 401 `INVALID_CREDENTIALS`：邮箱或密码错误
- 403 `ACCOUNT_DISABLED`：账号未激活
- 403 `DEVICE_LIMIT_EXCEEDED`：设备超限（不会自动踢）
```json
{
  "success": false,
  "error": "DEVICE_LIMIT_EXCEEDED",
  "message": "设备数量超出限制，请登录网站管理设备",
  "deviceLimitExceeded": true,
  "currentDevices": 3,
  "deviceLimit": 3
}
```
- 500 `VALIDATION_ERROR`：服务器内部错误

### 行为说明
- 若同一用户已有相同 `machineCode` 的设备记录，登录时会先删除旧记录再写入（去重）。
- 设备写入字段包括：`type=client`、`name`、`loginTime/lastActiveTime`、`sessionId=machineCode`。

---

## 2. 设备管理（网站）
- 页面：`/profile/devices`
- 支持移除设备；PC 端在收到 403 超限时，引导用户打开该页面操作。

---

## 3. Bearer Token 使用
- 成功登录后，PC 端需将 `token` 保存在本地安全存储。
- 后续访问需要登录态的接口，添加请求头：
```
Authorization: Bearer <token>
```

---

## 4. C++/Qt 集成指引
以下示例基于 Qt 5/6，使用 `QNetworkAccessManager`。示例省略错误处理与线程隔离细节，可按需封装。

### 4.1 设备机器码（示例思路）
- Windows：组合主板序列号、磁盘序列号、用户 SID 再做哈希（SHA-256 → Base64URL）
- macOS：IOPlatformUUID 哈希
- Linux：/etc/machine-id 哈希

```cpp
#include <QCryptographicHash>
#include <QByteArray>
#include <QString>

static QString normalizeBase64Url(const QByteArray &in) {
  return QString::fromLatin1(in.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString makeMachineCode(const QByteArray &rawId) {
  auto hash = QCryptographicHash::hash(rawId, QCryptographicHash::Sha256);
  return normalizeBase64Url(hash).left(32); // 截断到合适长度
}
```

### 4.2 登录请求
```cpp
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

QNetworkAccessManager *nam = new QNetworkAccessManager(this);

QJsonObject payload{
  {"email", "user@example.com"},
  {"password", "******"},
  {"machineCode", makeMachineCode("windows-uuid-or-mixed")},
  {"deviceInfo", QJsonObject{
      {"name", "我的台式机"},
      {"type", "client"},
      {"userAgent", QString("MyApp/%1 (%2)").arg(APP_VERSION, OS_STRING)},
      {"os", OS_STRING},
      {"appVersion", APP_VERSION}
  }}
};

QNetworkRequest req(QUrl(QStringLiteral("%1/api/auth/client/login").arg(API_BASE)));
req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

QNetworkReply *rp = nam->post(req, QJsonDocument(payload).toJson());
QObject::connect(rp, &QNetworkReply::finished, this, [this, rp]() {
  const auto body = rp->readAll();
  rp->deleteLater();
  const auto doc = QJsonDocument::fromJson(body);
  if (!doc.isObject()) return;
  const auto obj = doc.object();
  if (obj.value("success").toBool()) {
    const QString token = obj.value("token").toString();
    // TODO: 保存 token 到安全存储（Windows Credential / macOS Keychain / Linux Secret Service）
  } else {
    const QString err = obj.value("error").toString();
    if (err == "DEVICE_LIMIT_EXCEEDED") {
      // 弹框引导访问网站设备管理
    }
  }
});
```

### 4.3 携带 Token 访问业务接口
```cpp
QNetworkRequest req(QUrl(QStringLiteral("%1/api/xxx/protected").arg(API_BASE)));
req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
```

### 4.4 登出（建议）
- 客户端本地删除 token；
- 可选：调用网站设备管理页移除该设备（后续可扩展 `/api/auth/client/logout`）。

### 4.5 心跳/在线状态（可选扩展）
- 建议每 5 分钟调用一次心跳接口，更新 `lastActiveTime`，服务端可基于该字段展示“在线”。
- 如需，我可补充：`POST /api/auth/client/heartbeat { sessionId }`。

---

## 5. 安全建议
- token 放系统安全存储；
- 与服务端通信强制 HTTPS；
- 机器码只作为会话标识使用，不要包含明文硬件序列号；
- 注意重放保护：登录后持久化 token，避免频繁登录。

---

## 6. 变更日志
- 2025-09-07 首版：`sessionId = machineCode`；PC 端登录前配额检查；登录成功写入设备；Web 各登录方式统一记录设备。

---

## 7. 新增：客户端登出与心跳

### 7.1 登出 API
```
POST /api/auth/client/logout
Authorization: Bearer <token>
```
- 功能：根据 JWT 中的 `sessionId`（machineCode）移除当前设备记录
- 成功响应：`{ "success": true, "removed": true }`
- 失败：401 `UNAUTHORIZED` / 404 `USER_NOT_FOUND`

Qt 调用示例：
```cpp
QNetworkRequest req(QUrl(QStringLiteral("%1/api/auth/client/logout").arg(API_BASE)));
req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
QNetworkReply *rp = nam->post(req, QByteArray());
```

### 7.2 心跳 API
```
POST /api/auth/client/heartbeat
Authorization: Bearer <token>   // 推荐
Content-Type: application/json

// 也支持无 token 形式：
{ "userId": "user_...", "machineCode": "WIN-ABCDEF123456" }
```
- 功能：将对应设备的 `lastActiveTime` 更新为当前时间
- 成功响应：`{ "success": true }`
- 失败：401 `UNAUTHORIZED`（无身份）/ 404 `DEVICE_NOT_FOUND`

Qt 调用示例：
```cpp
QNetworkRequest req(QUrl(QStringLiteral("%1/api/auth/client/heartbeat").arg(API_BASE)));
req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
QJsonObject payload; // 空体即可
QNetworkReply *rp = nam->post(req, QJsonDocument(payload).toJson());
```

- 建议频率：5 分钟一次
- 超时处理：若收到 401/404，可尝试刷新登录或引导用户重登

---

## 8. Token 持久化与自动登录
为避免频繁登录，PC 端应将登录成功返回的 `token` 安全持久化，并在应用启动时自动装载与校验。

### 8.1 存储策略（优先级从高到低）
1) 系统级密码库（推荐）
- Windows: Credential Manager
- macOS: Keychain
- Linux: libsecret / Secret Service
- 建议使用跨平台封装库 [QtKeychain](https://github.com/frankosterfeld/qtkeychain)

2) 受保护文件（次选）
- 将 token 写入仅当前用户可读的文件，配合简易加密/混淆（如随机密钥+对称加密）
- 适合无法引入 QtKeychain 的场景

3) 纯内存（开发临时方案）
- App 关闭即失效，不建议用于生产

### 8.2 QtKeychain 示例
```cpp
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <qt5keychain/keychain.h> // 或 <qt6keychain/keychain.h>

static const char *kServiceName = "LimeReader";
static const char *kTokenKey = "auth_token";

void saveTokenSecurely(const QString &token) {
  auto *job = new QKeychain::WritePasswordJob(kServiceName);
  job->setAutoDelete(true);
  job->setKey(kTokenKey);
  job->setTextData(token);
  QObject::connect(job, &QKeychain::Job::finished, job, [](QKeychain::Job *j){
    if (j->error()) {
      qWarning() << "save token failed:" << j->errorString();
    }
  });
  job->start();
}

void loadTokenSecurely(std::function<void(QString)> cb) {
  auto *job = new QKeychain::ReadPasswordJob(kServiceName);
  job->setAutoDelete(true);
  job->setKey(kTokenKey);
  QObject::connect(job, &QKeychain::Job::finished, job, [job, cb](QKeychain::Job *){
    if (job->error()) {
      qWarning() << "load token failed:" << job->errorString();
      cb({});
    } else {
      cb(job->textData());
    }
  });
  job->start();
}

void clearTokenSecurely() {
  auto *job = new QKeychain::DeletePasswordJob(kServiceName);
  job->setAutoDelete(true);
  job->setKey(kTokenKey);
  QObject::connect(job, &QKeychain::Job::finished, job, [](QKeychain::Job *j){
    if (j->error()) qWarning() << "clear token failed:" << j->errorString();
  });
  job->start();
}
```

### 8.3 启动自动登录流程建议
1) 从安全存储加载 token
2) 检查 JWT 是否临期/过期
   - 解码 payload（Base64URL）读取 `exp`；若剩余不足 10 分钟，建议静默重登
3) 使用 token 调用轻量接口（如 `/api/auth/client/heartbeat`）
   - 成功：进入已登录态
   - 401/404：视为失效 → 弹出登录页或静默重登

```cpp
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

struct JwtPayload { qint64 exp = 0; };

static JwtPayload decodeJwtPayload(const QString &token) {
  JwtPayload p; 
  const auto parts = token.split('.');
  if (parts.size() != 3) return p;
  auto payload = QByteArray::fromBase64(parts[1].toUtf8(), QByteArray::Base64UrlEncoding);
  auto doc = QJsonDocument::fromJson(payload);
  if (doc.isObject()) p.exp = doc["exp"].toVariant().toLongLong();
  return p;
}

bool isTokenNearExpiry(const QString &token, int seconds = 600) {
  const auto p = decodeJwtPayload(token);
  if (p.exp <= 0) return true;
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  return (p.exp - now) < seconds;
}
```

### 8.4 自动刷新/重登策略
- 本方案未实现 Refresh Token，客户端可在以下场景触发“静默重登”：
  - 启动时 token 过期或即将过期
  - 心跳/业务接口出现 401/403/404
- 静默重登：后台再次调用 `POST /api/auth/client/login`，成功后覆盖旧 token

### 8.5 安全注意事项
- 始终使用 HTTPS
- token 仅在内存与系统密码库中出现，避免平明文落盘
- 传输/日志打印时一定不要输出完整 token；必要时打码显示
- 如用户主动登出：同时调用 `/api/auth/client/logout` 并清除本地 token

---
