#!/bin/bash

# =================================================================
# LoimReader macOS 一键部署脚本
# 功能：构建 -> 打包DMG -> 生成version.json -> 准备OSS上传
# =================================================================

set -e

# 脚本配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)/deploy"

# 项目配置
CLIENT_NAME="LoimReader"
CLIENT_DIR="loimreader"
PROJECT_VERSION="2.7.2"
BUILD_DIR="$SCRIPT_DIR/build_cmake"
APP_PATH="$BUILD_DIR/bin/LoimReader.app"
UPLOAD_DIR="$DEPLOY_DIR/uploads/$CLIENT_DIR"

# OSS配置
OSS_BUCKET="limereader-releases"
OSS_REGION="oss-cn-hangzhou"
OSS_BASE_URL="https://limereader-releases.oss-cn-hangzhou.aliyuncs.com"
OSS_PREFIX="updates/$CLIENT_DIR"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 打印函数
print_header() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE} $1${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# 显示帮助
show_help() {
    print_header "LoimReader macOS 一键部署脚本"
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -v, --version VERSION    指定版本号（默认: 2.7.2）"
    echo "  -c, --clean              构建前清理"
    echo "  -s, --skip-build         跳过构建，仅打包"
    echo "  -u, --upload             构建完成后上传到OSS"
    echo "  -h, --help               显示帮助"
    echo ""
    echo "示例:"
    echo "  $0                       # 完整构建和打包"
    echo "  $0 -c                    # 清理后重新构建"
    echo "  $0 -v 2.7.3              # 指定版本号"
    echo "  $0 -u                    # 构建并上传到OSS"
    echo ""
}

# 检查环境
check_environment() {
    print_info "检查构建环境..."
    
    # 检查Qt
    if ! command -v qmake &> /dev/null && ! command -v qmake6 &> /dev/null; then
        print_error "未找到Qt6，请先安装: brew install qt@6"
        exit 1
    fi
    
    # 检查CMake
    if ! command -v cmake &> /dev/null; then
        print_error "未找到CMake，请先安装: brew install cmake"
        exit 1
    fi
    
    # 检查Python
    if ! command -v python3 &> /dev/null; then
        print_error "未找到Python3，请先安装: brew install python3"
        exit 1
    fi
    
    # 检查macdeployqt
    if ! command -v macdeployqt &> /dev/null; then
        print_warning "macdeployqt不在PATH中，尝试添加Qt路径..."
        export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"
        if ! command -v macdeployqt &> /dev/null; then
            print_error "无法找到macdeployqt"
            exit 1
        fi
    fi

    # 检查create-dmg
    if ! command -v create-dmg &> /dev/null; then
        print_error "未找到create-dmg，请先安装: brew install create-dmg"
        exit 1
    fi
    
    print_success "环境检查通过"
}

# 清理构建文件
clean_build() {
    print_info "清理构建文件..."
    rm -rf "$BUILD_DIR"
    rm -rf "$UPLOAD_DIR"
    print_success "清理完成"
}

# 生成资源
generate_assets() {
    print_info "生成应用资源..."
    cd "$SCRIPT_DIR"
    
    # 生成图标
    if [ -f "create_app_icon.py" ]; then
        python3 create_app_icon.py 2>/dev/null || print_warning "图标生成失败"
    fi
    
    # 生成DMG背景图
    if [ -f "create_dmg_background.py" ]; then
        python3 create_dmg_background.py 2>/dev/null || print_warning "背景图生成失败"
    fi
}

# 构建应用
build_app() {
    print_info "开始构建应用..."
    
    generate_assets
    
    # 创建构建目录
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # 配置CMake（传入版本号）
    print_info "配置CMake..."
    cmake -DCMAKE_BUILD_TYPE=Release -DPROJECT_VERSION_OVERRIDE="$PROJECT_VERSION" .. || {
        print_error "CMake配置失败"
        exit 1
    }
    
    # 编译
    print_info "编译应用..."
    local cpu_count=$(sysctl -n hw.ncpu)
    make -j"$cpu_count" || {
        print_error "编译失败"
        exit 1
    }
    
    # 设置应用图标
    set_app_icon
    
    # 部署依赖
    deploy_dependencies
    
    # 修复签名
    fix_signing
    
    print_success "应用构建完成"
}

# 设置应用图标
set_app_icon() {
    print_info "设置应用图标..."
    
    if [ ! -d "$APP_PATH" ]; then
        print_error "未找到应用: $APP_PATH"
        exit 1
    fi
    
    local icon_source="$SCRIPT_DIR/icons/macos.icns"
    local icon_dest="$APP_PATH/Contents/Resources/macos.icns"
    
    if [ ! -f "$icon_source" ]; then
        print_warning "图标文件不存在: $icon_source"
        return
    fi
    
    # 确保Resources目录存在
    mkdir -p "$APP_PATH/Contents/Resources"
    
    # 复制图标文件
    cp "$icon_source" "$icon_dest" || {
        print_error "图标复制失败"
        exit 1
    }
    
    print_success "应用图标设置完成"
}

# 部署Qt依赖
deploy_dependencies() {
    print_info "部署Qt依赖库..."
    
    if [ ! -d "$APP_PATH" ]; then
        print_error "未找到应用: $APP_PATH"
        exit 1
    fi
    
    cd "$BUILD_DIR/bin"
    macdeployqt "LoimReader.app" -verbose=1 || print_warning "macdeployqt有警告"
    
    print_success "依赖部署完成"
}

# 修复签名
fix_signing() {
    print_info "修复应用签名..."
    
    if [ -d "$APP_PATH" ]; then
        xattr -cr "$APP_PATH" 2>/dev/null || true
        codesign --force --deep --sign - "$APP_PATH" 2>/dev/null || print_warning "签名失败（不影响本地使用）"
        print_success "签名修复完成"
    fi
}

# 创建DMG
create_dmg() {
    print_info "创建DMG安装包..."
    
    if [ ! -d "$APP_PATH" ]; then
        print_error "未找到应用: $APP_PATH"
        exit 1
    fi
    
    # 创建上传目录
    mkdir -p "$UPLOAD_DIR"
    
    # DMG文件名（符合OSS规范）
    local dmg_name="LoimReader_v${PROJECT_VERSION}_macOS.dmg"
    local final_dmg="$UPLOAD_DIR/$dmg_name"
    
    # 删除旧DMG
    rm -f "$final_dmg"

    # 创建临时目录（作为create-dmg的源目录）
    local temp_dir="$UPLOAD_DIR/dmg_temp"
    rm -rf "$temp_dir"
    mkdir -p "$temp_dir"

    print_info "准备DMG内容..."
    cp -R "$APP_PATH" "$temp_dir/"

    # 创建使用说明
    cat > "$temp_dir/使用说明.txt" << 'EOF'
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🎯 LoimReader - 长图阅读器
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📦 安装方法
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
将「LoimReader.app」拖拽到「Applications」文件夹

🌟 核心功能
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
• 智能分页 - 避免文字断开
• 双栏布局 - 节省50%纸张
• 格式转换 - PNG/JPG/PDF
• 自动切边 - 去除空白边框
• 高清输出 - 300DPI品质

🔧 系统要求
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
• macOS 14.0 或更高版本
• Apple Silicon (M系列) 或 Intel处理器

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
© 2024-2025 LoimReader
EOF

    # 创建 Gatekeeper 绕过指南
    cat > "$temp_dir/Gatekeeper_绕过指南.txt" << 'EOF'
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🛡️ Gatekeeper 绕过指南（仅当首次打开被阻止时需要）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

方法一：右键打开（推荐）
1) 将「LoimReader.app」拖入「应用程序」后，不要直接双击。
2) 在「应用程序」中对「LoimReader.app」点按右键，选择「打开」。
3) 在弹窗中再次点击「打开」。

方法二：系统设置允许
1) 打开「系统设置」→「隐私与安全性」。
2) 找到“已阻止来自开发者的应用”条目，点击“仍要打开/允许”。

方法三：移除隔离属性（终端）
1) 打开「终端」。
2) 执行以下命令（请先将App放入应用程序）：
   xattr -dr com.apple.quarantine /Applications/LoimReader.app
3) 再次尝试打开应用。

提示：仅当macOS提示无法验证开发者时才需要上述操作。
EOF

    # 准备 create-dmg 资源
    local dmg_resources_dir="$SCRIPT_DIR/dmg"
    local background_png="$dmg_resources_dir/background.png"
    local volume_icns="$dmg_resources_dir/volume.icns"

    # 记录资源存在性（背景预拷贝到源目录，避免Finder脚本同步问题）
    local background_args=()
    local volicon_args=()
    # 默认较大的窗口尺寸；如需匹配背景尺寸，请设置 CREATE_DMG_MATCH_BG_SIZE=true
    local window_w=800
    local window_h=600
    if [ -f "$background_png" ]; then
        mkdir -p "$temp_dir/.background"
        # 预处理背景图：转为PNG、sRGB、72DPI，并限制最大尺寸，避免Finder脚本失败
        local sanitized_bg="$temp_dir/.background/background.png"
        local srgb_profile="/System/Library/ColorSync/Profiles/sRGB Profile.icc"
        if command -v sips &> /dev/null; then
            cp -f "$background_png" "$sanitized_bg" || true
            # 转格式为PNG
            sips -s format png "$sanitized_bg" --out "$sanitized_bg" >/dev/null 2>&1 || true
            # 设置72DPI
            sips -s dpiWidth 72 -s dpiHeight 72 "$sanitized_bg" >/dev/null 2>&1 || true
            # 设置sRGB配置（如果存在）
            if [ -f "$srgb_profile" ]; then
                sips -s profile "$srgb_profile" "$sanitized_bg" >/dev/null 2>&1 || true
            fi
            # 限制背景大小（避免过大导致失败），最大宽高不超过 1200
            local bg_w=$(sips -g pixelWidth "$sanitized_bg" 2>/dev/null | awk '/pixelWidth/ {print $2}')
            local bg_h=$(sips -g pixelHeight "$sanitized_bg" 2>/dev/null | awk '/pixelHeight/ {print $2}')
            if [[ -n "$bg_w" && -n "$bg_h" ]]; then
                if [ "$bg_w" -gt 1200 ]; then
                    sips --resampleWidth 1200 "$sanitized_bg" >/dev/null 2>&1 || true
                fi
                # 重新读取以便窗口尺寸匹配
                bg_w=$(sips -g pixelWidth "$sanitized_bg" 2>/dev/null | awk '/pixelWidth/ {print $2}')
                bg_h=$(sips -g pixelHeight "$sanitized_bg" 2>/dev/null | awk '/pixelHeight/ {print $2}')
                if [[ ("${CREATE_DMG_MATCH_BG_SIZE}" == "1" || "${CREATE_DMG_MATCH_BG_SIZE}" == "true") && -n "$bg_w" && -n "$bg_h" && "$bg_w" -ge 200 && "$bg_h" -ge 200 ]]; then
                    window_w=$bg_w
                    window_h=$bg_h
                    print_info "根据背景图设置窗口大小: ${window_w}x${window_h}"
                fi
            fi
            # 若未启用匹配背景尺寸，则将背景图强制缩放为窗口尺寸以避免Finder异常
            if [[ "${CREATE_DMG_MATCH_BG_SIZE}" != "1" && "${CREATE_DMG_MATCH_BG_SIZE}" != "true" ]]; then
                sips -z $window_h $window_w "$sanitized_bg" >/dev/null 2>&1 || true
                print_info "已将背景图缩放为窗口大小: ${window_w}x${window_h}"
            fi
        else
            cp -f "$background_png" "$sanitized_bg" || print_warning "复制背景图失败，将尝试使用外部路径"
        fi
        if [ -f "$sanitized_bg" ]; then
            background_args=(--background "$sanitized_bg")
        else
            background_args=(--background "$background_png")
        fi
    else
        print_warning "未找到DMG背景图: $background_png (将使用默认背景)"
    fi

    # 计算四个图标的菱形布局（基于窗口中心）
    local cx=$((window_w / 2))
    local cy=$((window_h / 2))
    local offset_x=240
    local offset_y=200
    local app_x=$((cx - offset_x))
    local app_y=$((cy))
    local apps_x=$((cx + offset_x))
    local apps_y=$((cy))
    local top_file_y=$((cy - offset_y))
    # 图标大小为128，要求边缘距中心线40px → 中心偏移 = 40 + 128/2 = 104
    local text_spacing=104
    local top_left_file_x=$((cx - text_spacing))
    local top_right_file_x=$((cx + text_spacing))
    if [ -f "$volume_icns" ]; then
        volicon_args=(--volicon "$volume_icns")
    else
        print_warning "未找到卷标图标: $volume_icns (将使用默认图标)"
    fi

    # 使用 create-dmg 生成DMG（失败后回退为纯色背景重试一次）
    print_info "使用 create-dmg 生成DMG文件..."
    local attempts=${CREATE_DMG_RETRIES:-2}
    local attempt=1
    local created=false
    while [ $attempt -le $attempts ]; do
        print_info "create-dmg 尝试第 $attempt/$attempts 次（带背景）..."
        if create-dmg \
            --volname "LoimReader" \
            "${volicon_args[@]}" \
            "${background_args[@]}" \
            --window-pos 200 120 \
            --window-size $window_w $window_h \
            --icon-size 128 \
            --icon "LoimReader.app" $app_x $app_y \
            --app-drop-link $apps_x $apps_y \
            --icon "使用说明.txt" $top_left_file_x $top_file_y \
            --icon "Gatekeeper_绕过指南.txt" $top_right_file_x $top_file_y \
            "$final_dmg" \
            "$temp_dir"; then
            created=true
            break
        fi
        rm -f "$final_dmg"
        sleep 2
        attempt=$((attempt + 1))
    done
    if [ "$created" != true ]; then
        print_warning "create-dmg 设置背景多次失败，改为无背景打包..."
        rm -f "$final_dmg"
        create-dmg \
            --volname "LoimReader" \
            "${volicon_args[@]}" \
            --window-pos 200 120 \
            --window-size $window_w $window_h \
            --icon-size 128 \
            --icon "LoimReader.app" $app_x $app_y \
            --app-drop-link $apps_x $apps_y \
            --icon "使用说明.txt" $top_left_file_x $top_file_y \
            --icon "Gatekeeper_绕过指南.txt" $top_right_file_x $top_file_y \
            "$final_dmg" \
            "$temp_dir" || {
            print_error "DMG创建失败"
            # 不删除临时目录以便排查
            exit 1
        }
    fi

    # 清理临时文件
    rm -rf "$temp_dir"
    
    # 计算文件大小和哈希
    local dmg_size=$(du -h "$final_dmg" | cut -f1)
    local dmg_hash=$(md5 -q "$final_dmg")
    
    print_success "DMG创建成功"
    echo -e "${GREEN}  文件: $dmg_name${NC}"
    echo -e "${GREEN}  大小: $dmg_size${NC}"
    echo -e "${GREEN}  MD5: $dmg_hash${NC}"
    echo -e "${GREEN}  位置: $final_dmg${NC}"
    
    # 保存信息用于生成version.json
    echo "$dmg_size" > "$UPLOAD_DIR/.dmg_size"
    echo "$dmg_hash" > "$UPLOAD_DIR/.dmg_hash"
}

# 生成version.json（支持合并模式）
generate_version_json() {
    print_info "生成version.json..."
    
    # 读取DMG信息
    local dmg_size=$(cat "$UPLOAD_DIR/.dmg_size" 2>/dev/null || echo "未知")
    local dmg_hash=$(cat "$UPLOAD_DIR/.dmg_hash" 2>/dev/null || echo "待计算")
    local release_date=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    local version_file="$UPLOAD_DIR/version.json"
    local remote_version_tmp="$UPLOAD_DIR/version.remote.json"

    # 优先拉取远端version.json作为合并基准
    print_info "尝试从OSS读取现有 version.json 以合并..."
    if curl -fsS "${OSS_BASE_URL}/${OSS_PREFIX}/version.json" -o "$remote_version_tmp" 2>/dev/null; then
        if python3 - "$remote_version_tmp" << 'PY'
import json,sys
try:
    json.load(open(sys.argv[1], 'r', encoding='utf-8'))
    sys.exit(0)
except Exception as e:
    sys.exit(1)
PY
        then
            print_success "已从OSS获取远端version.json，作为合并基准"
            mv -f "$remote_version_tmp" "$version_file"
        else
            print_warning "远端version.json格式无效，忽略"
            rm -f "$remote_version_tmp"
        fi
    else
        print_info "OSS上未找到现有version.json，将基于本地/新建"
    fi
    
    # macOS 包信息
    local macos_package=$(cat << EOF
{
  "url": "LoimReader_v${PROJECT_VERSION}_macOS.dmg",
  "size": "${dmg_size}",
  "hash": "${dmg_hash}",
  "downloadUrl": "${OSS_BASE_URL}/${OSS_PREFIX}/LoimReader_v${PROJECT_VERSION}_macOS.dmg",
  "description": "macOS 磁盘镜像",
  "minSystemVersion": "macOS 14.0 或更高版本"
}
EOF
)
    
    # 检查是否存在现有的 version.json
    if [ -f "$version_file" ]; then
        print_info "找到现有 version.json，将合并信息"
        
        # 使用 Python 合并 JSON
        python3 << PYTHON
import json
import sys

try:
    # 读取现有 JSON
    with open('$version_file', 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    # 更新版本信息
    data['latestVersion'] = '${PROJECT_VERSION}'
    data['releaseDate'] = '${release_date}'
    data['releaseNotes'] = 'LoimReader 版本 ${PROJECT_VERSION}\\n- 性能优化\\n- Bug修复\\n- 功能改进'
    data['forceUpdate'] = False
    
    # 确保 packages 字段存在
    if 'packages' not in data:
        data['packages'] = {}
    
    # 更新 macOS 包信息
    data['packages']['macos'] = json.loads('''$macos_package''')
    
    # 保存
    with open('$version_file', 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    
    print("保留的平台信息:", ", ".join([k for k in data['packages'].keys() if k != 'macos']))
    sys.exit(0)
except Exception as e:
    print(f"合并失败: {e}", file=sys.stderr)
    sys.exit(1)
PYTHON
        
        if [ $? -eq 0 ]; then
            print_success "version.json 已合并（macOS 平台）"
        else
            print_warning "合并失败，将创建新文件"
            # 创建新文件
            cat > "$version_file" << EOF
{
  "latestVersion": "${PROJECT_VERSION}",
  "releaseDate": "${release_date}",
  "releaseNotes": "LoimReader 版本 ${PROJECT_VERSION}\\n- 性能优化\\n- Bug修复\\n- 功能改进",
  "forceUpdate": false,
  "packages": {
    "macos": $macos_package
  }
}
EOF
        fi
    else
        # 创建新文件
        print_info "创建新的 version.json"
        cat > "$version_file" << EOF
{
  "latestVersion": "${PROJECT_VERSION}",
  "releaseDate": "${release_date}",
  "releaseNotes": "LoimReader 版本 ${PROJECT_VERSION}\\n- 性能优化\\n- Bug修复\\n- 功能改进",
  "forceUpdate": false,
  "packages": {
    "macos": $macos_package
  }
}
EOF
    fi
    
    # 验证JSON格式
    if python3 -m json.tool "$version_file" > /dev/null 2>&1; then
        print_success "version.json 验证通过"
        echo -e "${GREEN}  位置: $version_file${NC}"
    else
        print_error "version.json 格式错误"
        exit 1
    fi
    
    # 清理临时文件
    rm -f "$UPLOAD_DIR/.dmg_size" "$UPLOAD_DIR/.dmg_hash"
}

# 生成上传脚本
generate_upload_script() {
    print_info "生成OSS上传脚本..."
    
    local upload_script="$UPLOAD_DIR/upload_to_oss.sh"
    
    cat > "$upload_script" << 'EOFSCRIPT'
#!/bin/bash

# OSS上传脚本 - 自动生成
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
CLIENT_DIR="loimreader"
VERSION="VERSION_PLACEHOLDER"
BUCKET="limereader-releases"
ENDPOINT="oss-cn-hangzhou.aliyuncs.com"
OSS_PREFIX="updates/$CLIENT_DIR"

echo "开始上传 LoimReader v$VERSION 到阿里云OSS..."

# 加载OSS配置（如果存在）
OSS_CONFIG="$DEPLOY_DIR/oss-config.sh"
if [ -f "$OSS_CONFIG" ]; then
    echo "✓ 加载OSS配置: $OSS_CONFIG"
    source "$OSS_CONFIG"
fi

# 检查ossutil（2.x版本）
if ! command -v ossutil &> /dev/null; then
    echo "❌ 错误: 未找到ossutil"
    echo "安装方法:"
    echo "  运行安装脚本: ../../loimreader/install_ossutil.sh"
    echo "  或参考: ../../loimreader/OSSUTIL_INSTALL.md"
    echo "  官方文档: https://help.aliyun.com/zh/oss/developer-reference/ossutil-overview/"
    exit 1
fi

# 验证ossutil版本
OSSUTIL_VERSION=$(ossutil version 2>&1 | head -n 1)
if ! echo "$OSSUTIL_VERSION" | grep -qE '2\.[0-9]+\.[0-9]+'; then
    echo "⚠️  警告: 建议使用 ossutil 2.x 版本"
    echo "  当前版本: $OSSUTIL_VERSION"
    echo "  参考: ../../loimreader/OSSUTIL_UPDATE.md"
fi

# 构建认证参数（使用2.x版本的长参数格式）
AUTH_PARAMS=""
if [ -n "$OSS_ACCESS_KEY_ID" ] && [ -n "$OSS_ACCESS_KEY_SECRET" ]; then
    echo "✓ 使用环境变量中的OSS密钥"
    AUTH_PARAMS="--access-key-id $OSS_ACCESS_KEY_ID --access-key-secret $OSS_ACCESS_KEY_SECRET --endpoint https://$ENDPOINT"
else
    echo "✓ 使用ossutil配置文件中的密钥"
    echo "提示: 如需使用环境变量，请设置 OSS_ACCESS_KEY_ID 和 OSS_ACCESS_KEY_SECRET"
fi

# 上传DMG文件
echo ""
echo "上传DMG文件..."
for file in "$SCRIPT_DIR"/LoimReader_v${VERSION}_*.dmg; do
    if [ -f "$file" ]; then
        echo "  上传: $(basename "$file")"
        if [ -n "$AUTH_PARAMS" ]; then
            ossutil cp "$file" "oss://$BUCKET/$OSS_PREFIX/$(basename "$file")" $AUTH_PARAMS
        else
            ossutil cp "$file" "oss://$BUCKET/$OSS_PREFIX/$(basename "$file")"
        fi
        echo "  ✓ 上传成功"
    fi
done

# 上传version.json
echo ""
echo "上传version.json..."
if [ -n "$AUTH_PARAMS" ]; then
    ossutil cp "$SCRIPT_DIR/version.json" "oss://$BUCKET/$OSS_PREFIX/version.json" $AUTH_PARAMS
else
    ossutil cp "$SCRIPT_DIR/version.json" "oss://$BUCKET/$OSS_PREFIX/version.json"
fi
echo "✓ version.json上传成功"

echo ""
echo "=========================================="
echo "  上传完成！"
echo "=========================================="
echo ""
echo "验证链接:"
echo "  version.json:"
echo "    https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/$OSS_PREFIX/version.json"
echo ""
echo "  DMG:"
echo "    https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/$OSS_PREFIX/LoimReader_v${VERSION}_macOS.dmg"
echo ""
echo "验证命令:"
echo "  curl -s https://limereader-releases.oss-cn-hangzhou.aliyuncs.com/$OSS_PREFIX/version.json | python3 -m json.tool"
echo ""

EOFSCRIPT
    
    # 替换版本号
    sed -i.bak "s/VERSION_PLACEHOLDER/$PROJECT_VERSION/g" "$upload_script"
    rm -f "$upload_script.bak"
    chmod +x "$upload_script"
    
    print_success "上传脚本已生成: $upload_script"
}

# 上传到OSS
upload_to_oss() {
    print_info "上传文件到OSS..."
    
    if [ ! -f "$UPLOAD_DIR/upload_to_oss.sh" ]; then
        print_error "上传脚本不存在"
        exit 1
    fi
    
    "$UPLOAD_DIR/upload_to_oss.sh"
}

# 显示摘要
show_summary() {
    echo ""
    print_header "部署完成"
    echo -e "${GREEN}✅ 构建成功${NC}"
    echo ""
    echo -e "${YELLOW}📁 生成的文件:${NC}"
    echo "  • DMG: $UPLOAD_DIR/LoimReader_v${PROJECT_VERSION}_macOS.dmg"
    echo "  • version.json: $UPLOAD_DIR/version.json"
    echo "  • 上传脚本: $UPLOAD_DIR/upload_to_oss.sh"
    echo ""
    echo -e "${YELLOW}🎯 直接运行程序（不需要安装DMG）:${NC}"
    echo "  • 程序位置: $APP_PATH"
    echo "  • 运行命令: open $APP_PATH"
    echo "  • 或双击: $APP_PATH"
    echo ""
    echo -e "${YELLOW}🚀 下一步:${NC}"
    if [ "$DO_UPLOAD" = true ]; then
        echo "  ✅ 文件已上传到OSS"
        echo "  • 验证: curl ${OSS_BASE_URL}/${OSS_PREFIX}/version.json"
    else
        echo "  1. 测试DMG: open $UPLOAD_DIR/LoimReader_v${PROJECT_VERSION}_macOS.dmg"
        echo "  2. 上传OSS: $UPLOAD_DIR/upload_to_oss.sh"
        echo "  3. 验证: curl ${OSS_BASE_URL}/${OSS_PREFIX}/version.json"
    fi
    echo ""
}

# 解析命令行参数
DO_CLEAN=false
DO_BUILD=true
DO_UPLOAD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--version)
            PROJECT_VERSION="$2"
            shift 2
            ;;
        -c|--clean)
            DO_CLEAN=true
            shift
            ;;
        -s|--skip-build)
            DO_BUILD=false
            shift
            ;;
        -u|--upload)
            DO_UPLOAD=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "未知参数: $1"
            show_help
            exit 1
            ;;
    esac
done

# 主程序
main() {
    print_header "LoimReader macOS 一键部署脚本 v${PROJECT_VERSION}"
    
    # 检查系统
    if [[ "$OSTYPE" != "darwin"* ]]; then
        print_error "此脚本只能在macOS上运行"
        exit 1
    fi
    
    # 检查环境
    check_environment
    
    # 清理
    if [ "$DO_CLEAN" = true ]; then
        clean_build
    fi
    
    # 构建
    if [ "$DO_BUILD" = true ]; then
        build_app
    fi
    
    # 创建DMG
    create_dmg
    
    # 生成version.json
    generate_version_json
    
    # 生成上传脚本
    generate_upload_script
    
    # 上传（可选）
    if [ "$DO_UPLOAD" = true ]; then
        upload_to_oss
    fi
    
    # 显示摘要
    show_summary
}

# 运行主程序
main "$@"

