#!/bin/bash

# =================================================================
# 部署系统验证脚本
# 检查所有必要的工具和配置
# =================================================================

set -e

BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} LoimReader 部署系统验证${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

ERRORS=0
WARNINGS=0

# 检查函数
check_command() {
    local cmd=$1
    local name=$2
    local install_hint=$3
    
    if command -v $cmd &> /dev/null; then
        local version=$($cmd --version 2>&1 | head -n 1 || $cmd version 2>&1 | head -n 1 || echo "已安装")
        echo -e "${GREEN}✅ $name${NC} - $version"
        return 0
    else
        echo -e "${RED}❌ $name 未安装${NC}"
        if [ -n "$install_hint" ]; then
            echo -e "${YELLOW}   安装: $install_hint${NC}"
        fi
        ERRORS=$((ERRORS + 1))
        return 1
    fi
}

check_file() {
    local file=$1
    local name=$2
    
    if [ -f "$file" ]; then
        echo -e "${GREEN}✅ $name${NC}"
        echo -e "   位置: $file"
        return 0
    else
        echo -e "${RED}❌ $name 不存在${NC}"
        echo -e "   预期位置: $file"
        ERRORS=$((ERRORS + 1))
        return 1
    fi
}

check_dir() {
    local dir=$1
    local name=$2
    
    if [ -d "$dir" ]; then
        echo -e "${GREEN}✅ $name${NC}"
        return 0
    else
        echo -e "${YELLOW}⚠️  $name 不存在${NC}"
        WARNINGS=$((WARNINGS + 1))
        return 1
    fi
}

# 1. 检查必需工具
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}1. 检查必需工具${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

check_command "cmake" "CMake" "brew install cmake"
check_command "qmake" "Qt" "brew install qt@6"
check_command "python3" "Python3" "brew install python3"
check_command "ossutil" "ossutil 2.x" "./install_ossutil.sh"

echo ""

# 2. 检查Python模块
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}2. 检查Python模块${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

if python3 -c "import PIL" 2>/dev/null; then
    echo -e "${GREEN}✅ Pillow (PIL)${NC}"
else
    echo -e "${RED}❌ Pillow 未安装${NC}"
    echo -e "${YELLOW}   安装: pip3 install Pillow${NC}"
    ERRORS=$((ERRORS + 1))
fi

echo ""

# 3. 检查部署脚本
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}3. 检查部署脚本${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

check_file "onekey_mac_deploy.sh" "macOS部署脚本"
check_file "onekey_win_deploy.bat" "Windows部署脚本"
check_file "install_ossutil.sh" "ossutil安装脚本"

echo ""

# 4. 检查OSS配置
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}4. 检查OSS配置${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

OSS_CONFIG="../deploy/oss-config.sh"
if check_file "$OSS_CONFIG" "OSS密钥配置"; then
    source "$OSS_CONFIG"
    if [ -n "$OSS_ACCESS_KEY_ID" ] && [ -n "$OSS_ACCESS_KEY_SECRET" ]; then
        echo -e "${GREEN}   密钥已加载${NC}"
        echo -e "   Access Key ID: ${OSS_ACCESS_KEY_ID:0:10}..."
    else
        echo -e "${RED}   密钥未配置${NC}"
        ERRORS=$((ERRORS + 1))
    fi
fi

echo ""

# 5. 检查文档
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}5. 检查文档${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

check_file "一键部署指南.md" "部署指南"
check_file "OSSUTIL_INSTALL.md" "ossutil安装文档"
check_file "OSS_SETUP.md" "OSS配置文档"
check_file "部署完成摘要.md" "部署摘要"

echo ""

# 6. 检查输出目录
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}6. 检查输出目录${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

check_dir "../deploy/uploads/loimreader" "输出目录"

echo ""

# 7. ossutil 详细检查
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}7. ossutil 详细检查${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

if command -v ossutil &> /dev/null; then
    OSSUTIL_VERSION=$(ossutil version 2>&1 | head -n 1)
    echo -e "${GREEN}✅ ossutil 版本: $OSSUTIL_VERSION${NC}"
    
    if [[ "$OSSUTIL_VERSION" == "2."* ]]; then
        echo -e "${GREEN}   版本正确 (2.x)${NC}"
    else
        echo -e "${YELLOW}⚠️  版本可能过旧，建议升级到 2.2.0${NC}"
        WARNINGS=$((WARNINGS + 1))
    fi
    
    # 测试连接（如果配置了密钥）
    if [ -n "$OSS_ACCESS_KEY_ID" ]; then
        echo ""
        echo -e "${BLUE}   测试OSS连接...${NC}"
        if ossutil ls oss://limereader-releases/updates/loimreader/ \
            --access-key-id "$OSS_ACCESS_KEY_ID" \
            --access-key-secret "$OSS_ACCESS_KEY_SECRET" \
            --endpoint https://oss-cn-hangzhou.aliyuncs.com &>/dev/null; then
            echo -e "${GREEN}   ✅ OSS连接成功${NC}"
        else
            echo -e "${YELLOW}   ⚠️  OSS连接失败（可能是权限或网络问题）${NC}"
            WARNINGS=$((WARNINGS + 1))
        fi
    fi
else
    echo -e "${RED}❌ ossutil 未安装${NC}"
fi

echo ""

# 总结
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}验证结果${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}🎉 所有检查通过！部署系统已就绪。${NC}"
    echo ""
    echo -e "${BLUE}下一步：${NC}"
    echo "  1. 运行: ./onekey_mac_deploy.sh -c"
    echo "  2. 查看: ../deploy/uploads/loimreader/"
    echo "  3. 上传: cd ../deploy/uploads/loimreader && ./upload_to_oss.sh"
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}⚠️  有 $WARNINGS 个警告，但可以继续使用。${NC}"
    echo ""
    echo -e "${BLUE}建议：${NC}"
    echo "  查看上面的警告信息并修复"
    exit 0
else
    echo -e "${RED}❌ 发现 $ERRORS 个错误，$WARNINGS 个警告。${NC}"
    echo ""
    echo -e "${BLUE}需要修复：${NC}"
    echo "  1. 安装缺失的工具"
    echo "  2. 配置OSS密钥"
    echo "  3. 重新运行验证: ./verify_setup.sh"
    exit 1
fi

