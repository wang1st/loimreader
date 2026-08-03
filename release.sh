#!/bin/bash

# =================================================================
# LoimReader 快速版本发布脚本
# 自动更新版本号、构建、打包
# =================================================================

set -e

BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 显示用法
show_usage() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE} LoimReader 版本发布脚本${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo "用法: $0 <版本号> [选项]"
    echo ""
    echo "参数:"
    echo "  <版本号>            新版本号（例如：2.7.1）"
    echo ""
    echo "选项:"
    echo "  -s, --skip-git      跳过Git提交和标签"
    echo "  -u, --upload        构建后自动上传到OSS"
    echo "  -h, --help          显示帮助"
    echo ""
    echo "示例:"
    echo "  $0 2.7.1           # 更新版本到2.7.1并构建"
    echo "  $0 2.7.1 -u        # 更新版本到2.7.1、构建并上传"
    echo "  $0 2.7.1 -s        # 更新版本到2.7.1，跳过Git操作"
    echo ""
    exit 1
}

# 检查参数
if [ -z "$1" ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    show_usage
fi

NEW_VERSION=$1
shift

# 解析选项
SKIP_GIT=false
DO_UPLOAD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--skip-git)
            SKIP_GIT=true
            shift
            ;;
        -u|--upload)
            DO_UPLOAD=true
            shift
            ;;
        *)
            echo -e "${RED}未知选项: $1${NC}"
            show_usage
            ;;
    esac
done

# 验证版本号格式
if ! [[ $NEW_VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo -e "${RED}错误: 版本号格式不正确${NC}"
    echo "正确格式: MAJOR.MINOR.PATCH (例如: 2.7.1)"
    exit 1
fi

# 获取当前版本
OLD_VERSION=$(grep "VERSION" CMakeLists.txt | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' | head -1)

if [ -z "$OLD_VERSION" ]; then
    echo -e "${RED}错误: 无法从CMakeLists.txt读取当前版本${NC}"
    exit 1
fi

# 显示版本变更
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} 版本更新${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${YELLOW}当前版本:${NC} $OLD_VERSION"
echo -e "${YELLOW}新版本:${NC}   $NEW_VERSION"
echo ""

if [ "$OLD_VERSION" = "$NEW_VERSION" ]; then
    echo -e "${YELLOW}⚠️  警告: 版本号未发生变化${NC}"
    read -p "确认继续? [y/N]: " confirm
    if [[ $confirm != "y" ]]; then
        echo "已取消"
        exit 0
    fi
fi

# 确认操作
echo "即将执行的操作:"
echo "  1. 更新源代码中的版本号"
if [ "$SKIP_GIT" = false ]; then
    echo "  2. 提交到Git并打标签"
else
    echo "  2. 跳过Git操作"
fi
echo "  3. 清理并重新构建"
echo "  4. 生成DMG和version.json"
if [ "$DO_UPLOAD" = true ]; then
    echo "  5. 上传到阿里云OSS"
fi
echo ""

read -p "确认继续? [y/N]: " confirm
if [[ $confirm != "y" ]]; then
    echo "已取消"
    exit 0
fi

# ============================================================
# 步骤 1: 更新版本号
# ============================================================
echo ""
echo -e "${GREEN}[1/4] 更新源代码中的版本号...${NC}"

# 备份文件
cp CMakeLists.txt CMakeLists.txt.bak
cp app_version.h app_version.h.bak
cp onekey_mac_deploy.sh onekey_mac_deploy.sh.bak

# 更新版本号
sed -i '' "s/VERSION $OLD_VERSION/VERSION $NEW_VERSION/g" CMakeLists.txt
sed -i '' "s/return \"$OLD_VERSION\";/return \"$NEW_VERSION\";/g" app_version.h
sed -i '' "s/PROJECT_VERSION=\"$OLD_VERSION\"/PROJECT_VERSION=\"$NEW_VERSION\"/g" onekey_mac_deploy.sh

# 验证修改
if grep -q "VERSION $NEW_VERSION" CMakeLists.txt && \
   grep -q "return \"$NEW_VERSION\"" app_version.h && \
   grep -q "PROJECT_VERSION=\"$NEW_VERSION\"" onekey_mac_deploy.sh; then
    echo -e "${GREEN}✅ 版本号更新成功${NC}"
    echo "  • CMakeLists.txt: VERSION $NEW_VERSION"
    echo "  • app_version.h: return \"$NEW_VERSION\""
    echo "  • onekey_mac_deploy.sh: PROJECT_VERSION=\"$NEW_VERSION\""
    
    # 删除备份文件
    rm -f CMakeLists.txt.bak app_version.h.bak onekey_mac_deploy.sh.bak
else
    echo -e "${RED}❌ 版本号更新失败${NC}"
    
    # 恢复备份
    mv CMakeLists.txt.bak CMakeLists.txt
    mv app_version.h.bak app_version.h
    mv onekey_mac_deploy.sh.bak onekey_mac_deploy.sh
    
    exit 1
fi

# ============================================================
# 步骤 2: Git提交和打标签
# ============================================================
if [ "$SKIP_GIT" = false ]; then
    echo ""
    echo -e "${GREEN}[2/4] 提交到Git...${NC}"
    
    # 检查是否有未提交的其他修改
    if git diff --name-only | grep -vE "CMakeLists.txt|app_version.h|onekey_mac_deploy.sh" > /dev/null; then
        echo -e "${YELLOW}⚠️  警告: 检测到其他未提交的修改${NC}"
        git status --short
        echo ""
        read -p "是否只提交版本号修改? [y/N]: " only_version
        if [[ $only_version == "y" ]]; then
            git add CMakeLists.txt app_version.h onekey_mac_deploy.sh
        else
            echo "请先处理其他修改"
            exit 1
        fi
    else
        git add CMakeLists.txt app_version.h onekey_mac_deploy.sh
    fi
    
    git commit -m "chore: 版本更新到 $NEW_VERSION"
    git tag -a "v$NEW_VERSION" -m "Release version $NEW_VERSION"
    
    echo -e "${GREEN}✅ Git提交完成${NC}"
    echo "  • Commit: 版本更新到 $NEW_VERSION"
    echo "  • Tag: v$NEW_VERSION"
else
    echo ""
    echo -e "${YELLOW}[2/4] 跳过Git操作${NC}"
fi

# ============================================================
# 步骤 3: 构建应用
# ============================================================
echo ""
echo -e "${GREEN}[3/4] 构建应用...${NC}"

./onekey_mac_deploy.sh -c || {
    echo -e "${RED}❌ 构建失败${NC}"
    exit 1
}

echo -e "${GREEN}✅ 构建完成${NC}"

# ============================================================
# 步骤 4: 验证输出
# ============================================================
echo ""
echo -e "${GREEN}[4/4] 验证输出文件...${NC}"

UPLOAD_DIR="../deploy/uploads/loimreader"
DMG_FILE="$UPLOAD_DIR/LoimReader_v${NEW_VERSION}_macOS.dmg"
VERSION_FILE="$UPLOAD_DIR/version.json"

# 检查文件
if [ -f "$DMG_FILE" ]; then
    DMG_SIZE=$(du -h "$DMG_FILE" | cut -f1)
    echo -e "${GREEN}✅ DMG文件:${NC} $DMG_FILE ($DMG_SIZE)"
else
    echo -e "${RED}❌ DMG文件未找到${NC}"
    exit 1
fi

if [ -f "$VERSION_FILE" ]; then
    # 验证JSON格式
    if python3 -m json.tool "$VERSION_FILE" > /dev/null 2>&1; then
        VERSION_IN_JSON=$(python3 -c "import json; print(json.load(open('$VERSION_FILE'))['latestVersion'])")
        if [ "$VERSION_IN_JSON" = "$NEW_VERSION" ]; then
            echo -e "${GREEN}✅ version.json:${NC} 版本号正确 ($VERSION_IN_JSON)"
        else
            echo -e "${YELLOW}⚠️  version.json版本号不匹配:${NC} $VERSION_IN_JSON != $NEW_VERSION"
        fi
    else
        echo -e "${RED}❌ version.json格式错误${NC}"
        exit 1
    fi
else
    echo -e "${RED}❌ version.json未找到${NC}"
    exit 1
fi

# ============================================================
# 步骤 5: 上传到OSS（可选）
# ============================================================
if [ "$DO_UPLOAD" = true ]; then
    echo ""
    echo -e "${GREEN}[5/5] 上传到OSS...${NC}"
    
    cd "$UPLOAD_DIR"
    ./upload_to_oss.sh || {
        echo -e "${RED}❌ 上传失败${NC}"
        exit 1
    }
    cd - > /dev/null
    
    echo -e "${GREEN}✅ 上传完成${NC}"
fi

# ============================================================
# 完成总结
# ============================================================
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} 发布完成${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${GREEN}版本:${NC} $NEW_VERSION"
echo -e "${GREEN}DMG:${NC} $DMG_FILE"
echo -e "${GREEN}JSON:${NC} $VERSION_FILE"
echo ""

if [ "$DO_UPLOAD" = true ]; then
    echo -e "${BLUE}✅ 已上传到OSS${NC}"
    echo ""
    echo "验证链接:"
    echo "  https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/updates/loimreader/version.json"
    echo ""
    if [ "$SKIP_GIT" = false ]; then
        echo "下一步:"
        echo "  git push origin main"
        echo "  git push origin v$NEW_VERSION"
    fi
else
    echo -e "${YELLOW}后续步骤:${NC}"
    echo ""
    echo "1. 测试应用:"
    echo "   open $DMG_FILE"
    echo ""
    echo "2. 编辑发布说明（可选）:"
    echo "   vim $VERSION_FILE"
    echo ""
    echo "3. 上传到OSS:"
    echo "   cd $UPLOAD_DIR"
    echo "   ./upload_to_oss.sh"
    echo ""
    if [ "$SKIP_GIT" = false ]; then
        echo "4. 推送到Git:"
        echo "   git push origin main"
        echo "   git push origin v$NEW_VERSION"
    fi
fi

echo ""
echo -e "${GREEN}🎉 版本 $NEW_VERSION 发布流程完成！${NC}"
echo ""

