#!/bin/bash

# =================================================================
# ossutil 2.2.0 安装脚本（macOS）
# 阿里云OSS命令行工具
# 官方文档：https://help.aliyun.com/zh/oss/developer-reference/ossutil-overview/
# =================================================================

set -e

BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} ossutil 2.2.0 安装脚本${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 检测系统架构
ARCH=$(uname -m)
VERSION="2.2.0"

if [ "$ARCH" = "arm64" ]; then
    OSSUTIL_URL="https://gosspublic.alicdn.com/ossutil/v2/${VERSION}/ossutil-${VERSION}-mac-arm64.zip"
    OSSUTIL_FILE="ossutil-${VERSION}-mac-arm64"
    echo -e "${GREEN}检测到 Apple Silicon (M系列芯片)${NC}"
else
    OSSUTIL_URL="https://gosspublic.alicdn.com/ossutil/v2/${VERSION}/ossutil-${VERSION}-mac-amd64.zip"
    OSSUTIL_FILE="ossutil-${VERSION}-mac-amd64"
    echo -e "${GREEN}检测到 Intel 芯片${NC}"
fi

echo ""
echo -e "${YELLOW}下载地址：${NC}$OSSUTIL_URL"
echo -e "${YELLOW}安装位置：${NC}/usr/local/bin/ossutil"
echo ""

# 开始安装
echo -e "${GREEN}[1/5] 下载 ossutil ${VERSION}...${NC}"
curl -o /tmp/ossutil.zip "$OSSUTIL_URL" || {
    echo -e "${RED}❌ 下载失败，请检查网络连接${NC}"
    exit 1
}

echo -e "${GREEN}[2/5] 解压安装包...${NC}"
cd /tmp
unzip -o ossutil.zip || {
    echo -e "${RED}❌ 解压失败${NC}"
    exit 1
}

echo -e "${GREEN}[3/5] 设置执行权限...${NC}"
chmod 755 "${OSSUTIL_FILE}/ossutil"

echo -e "${GREEN}[4/5] 安装到系统路径...${NC}"
sudo mv "${OSSUTIL_FILE}/ossutil" /usr/local/bin/ || {
    echo -e "${RED}❌ 安装失败，需要管理员权限${NC}"
    exit 1
}

# 创建符号链接（可选，保持兼容性）
sudo ln -sf /usr/local/bin/ossutil /usr/bin/ossutil 2>/dev/null || true

# 清理临时文件
rm -rf /tmp/ossutil.zip /tmp/${OSSUTIL_FILE}

# 验证安装
echo ""
echo -e "${GREEN}[5/5] 验证安装...${NC}"
if command -v ossutil &> /dev/null; then
    echo -e "${GREEN}✅ ossutil ${VERSION} 安装成功！${NC}"
    echo ""
    echo "版本信息："
    ossutil version
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE} 下一步操作${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "${YELLOW}方法1：使用项目配置文件（推荐）${NC}"
    echo "  source ../deploy/oss-config.sh"
    echo "  # 密钥已自动配置在文件中"
    echo ""
    echo -e "${YELLOW}方法2：手动配置 ossutil${NC}"
    echo "  ossutil config"
    echo "  # 按提示输入："
    echo "  # - endpoint: oss-cn-hangzhou.aliyuncs.com"
    echo "  # - accessKeyID: 你的AccessKeyId"
    echo "  # - accessKeySecret: 你的AccessKeySecret"
    echo "  # - region: cn-hangzhou"
    echo ""
    echo -e "${YELLOW}测试连接：${NC}"
    echo "  source ../deploy/oss-config.sh"
    echo "  ossutil ls oss://limereader-releases/updates/loimreader/"
    echo ""
else
    echo -e "${RED}❌ 安装失败${NC}"
    echo ""
    echo "请尝试手动安装："
    echo "  1. 访问官方文档：https://help.aliyun.com/zh/oss/developer-reference/ossutil-overview/"
    echo "  2. 下载对应版本的安装包"
    echo "  3. 手动解压并移动到 /usr/local/bin/"
    exit 1
fi

