#include "loim/i18n.h"

#include <stddef.h>

static const char *const translations[LOIM_LOCALE_COUNT][LOIM_TEXT_KEY_COUNT] = {
    [LOIM_LOCALE_EN] = {
        [LOIM_TEXT_APP_TITLE] = "LoimReader",
        [LOIM_TEXT_IMAGE_FILES] = "Image files",
        [LOIM_TEXT_SIGNED_IN_FORMAT] = "Signed in as %.180s",
        [LOIM_TEXT_LOGIN_PROMPT] = "Enter your ctdy123.com account",
        [LOIM_TEXT_HTTPS_UNAVAILABLE] = "HTTPS support is unavailable",
        [LOIM_TEXT_LOGIN_FIELDS_REQUIRED] = "Enter a valid email and password",
        [LOIM_TEXT_LOGIN_START_FAILED] = "Unable to start login",
        [LOIM_TEXT_SIGNING_IN] = "Signing in...",
        [LOIM_TEXT_LOGIN_NETWORK_ERROR] = "Network or server response error",
        [LOIM_TEXT_LOGIN_INVALID_CREDENTIALS] = "Incorrect email or password",
        [LOIM_TEXT_LOGIN_DEVICE_LIMIT] =
            "Device limit reached; manage devices on ctdy123.com",
        [LOIM_TEXT_LOGIN_ACCOUNT_DISABLED] = "This account is not active",
        [LOIM_TEXT_LOGIN_FAILED_FORMAT] = "Login failed (HTTP %d)",
        [LOIM_TEXT_LAYOUT_FAILED_FORMAT] = "Layout failed: %s",
        [LOIM_TEXT_IMPORTED_FORMAT] =
            "Imported %zu, skipped %zu - %zu images, %zu pages",
        [LOIM_TEXT_LOADING_FORMAT] = "Loading %zu/%zu: %.160s",
        [LOIM_TEXT_IMPORT_PROGRESS_FORMAT] = "Processing %zu/%zu (%u%%)",
        [LOIM_TEXT_FILE_DIALOG_ERROR] = "File dialog error",
        [LOIM_TEXT_IMPORT_CANCELED] = "Import canceled",
        [LOIM_TEXT_IMPORT_BUSY] = "Please wait for the current import to finish",
        [LOIM_TEXT_IMPORT_QUEUE_FAILED] = "Unable to queue images: out of memory",
        [LOIM_TEXT_CHOOSE_IMAGES] = "Choose one or more images",
        [LOIM_TEXT_CLEAR_FAILED] = "Unable to clear document: out of memory",
        [LOIM_TEXT_READY] = "Ready - drop images here or choose Import",
        [LOIM_TEXT_DROP_IMAGES] = "Drop images here or press Ctrl+O",
        [LOIM_TEXT_EMPTY_TITLE] = "Drop images here",
        [LOIM_TEXT_EMPTY_SUBTITLE] =
            "Import multiple images at once, or click to choose files",
        [LOIM_TEXT_EMPTY_PROCESSING] = "Preparing your preview...",
        [LOIM_TEXT_CLEAR] = "Clear",
        [LOIM_TEXT_CLEAR_TOOLTIP] =
            "Clear current images and return to the import screen",
        [LOIM_TEXT_CLEARED] = "Current content cleared",
        [LOIM_TEXT_PLEASE_WAIT] = "Please wait",
        [LOIM_TEXT_SIGN_IN] = "Sign in",
        [LOIM_TEXT_SIGN_IN_TITLE] = "Sign in to ctdy123.com",
        [LOIM_TEXT_EMAIL] = "Email",
        [LOIM_TEXT_PASSWORD] = "Password",
        [LOIM_TEXT_CANCEL] = "Cancel",
        [LOIM_TEXT_TWO_COLUMNS] = "Two-column layout",
        [LOIM_TEXT_ONE_COLUMN] = "One-column layout",
        [LOIM_TEXT_THREE_COLUMNS] = "Three-column layout",
        [LOIM_TEXT_SINGLE_PAGE] = "Single-page layout",
        [LOIM_TEXT_PAGE_NUMBERS_ON] = "Page numbers on",
        [LOIM_TEXT_PAGE_NUMBERS_OFF] = "Page numbers off",
        [LOIM_TEXT_PAGE_NUMBERS_NONE] = "Page numbers hidden",
        [LOIM_TEXT_PAGE_NUMBERS_BOTTOM_RIGHT] = "Page numbers at bottom right",
        [LOIM_TEXT_PAGE_NUMBERS_BOTTOM_CENTER] = "Page numbers at bottom center",
        [LOIM_TEXT_AUTO_SPLIT_APPLIED] = "Automatic split applied",
        [LOIM_TEXT_MARGIN_REDUCED] = "Print margin reduced",
        [LOIM_TEXT_MARGIN_INCREASED] = "Print margin increased",
        [LOIM_TEXT_PREVIEW_ENLARGED] = "Preview page enlarged",
        [LOIM_TEXT_PREVIEW_REDUCED] = "Preview page reduced",
        [LOIM_TEXT_EXPORT_UNAVAILABLE] =
            "PDF export migration is not enabled in this preview",
        [LOIM_TEXT_EXPORT_SUCCESS_FORMAT] = "PDF exported: %.180s",
        [LOIM_TEXT_EXPORT_FAILED] = "PDF export failed",
        [LOIM_TEXT_PRINT_UNAVAILABLE] =
            "Print migration is not enabled in this preview",
        [LOIM_TEXT_PRINT_STARTED] = "Print dialog opened",
        [LOIM_TEXT_PRINT_FAILED] = "Unable to start printing",
        [LOIM_TEXT_SPLIT_OUTSIDE] = "Split position is outside the document",
        [LOIM_TEXT_SPLIT_UPDATED] = "Manual split updated",
        [LOIM_TEXT_SPLIT_FAILED] = "Unable to place split here",
        [LOIM_TEXT_DROP_RELEASE] = "Release to import the selected images",
        [LOIM_TEXT_DROP_FAILED] = "Unable to accept the dropped file",
        [LOIM_TEXT_LOGIN_UNAVAILABLE] =
            "Ready - account login unavailable on this system",
        [LOIM_TEXT_REGISTER_NOW] = "No account? Register now",
        [LOIM_TEXT_REGISTER_OPEN_FAILED] = "Unable to open the registration page",
        [LOIM_TEXT_ABOUT_TITLE] = "About LoimReader",
        [LOIM_TEXT_ABOUT_VERSION_FORMAT] = "Version %s · %s",
        [LOIM_TEXT_ABOUT_WEBSITE] = "ctdy123.com",
        [LOIM_TEXT_ABOUT_COPYRIGHT] = "Copyright © 2024 Ctdy123.com",
        [LOIM_TEXT_ABOUT_OK] = "OK",
        [LOIM_TEXT_ABOUT_OPEN_FAILED] = "Unable to open the website",
        [LOIM_TEXT_ABOUT_ACCOUNT_SECTION] = "Account & subscription",
        [LOIM_TEXT_ABOUT_ACCOUNT_EMAIL_FORMAT] = "Account: %.180s",
        [LOIM_TEXT_ABOUT_SUBSCRIPTION_ACTIVE_FORMAT] = "Subscription: Active (%s)",
        [LOIM_TEXT_ABOUT_SUBSCRIPTION_INACTIVE] = "Subscription: Free or expired",
        [LOIM_TEXT_ABOUT_SUBSCRIPTION_EXPIRES_FORMAT] = "Expires: %.40s",
        [LOIM_TEXT_ABOUT_NOT_SIGNED_IN] = "Not signed in",
        [LOIM_TEXT_SUBSCRIPTION_MONTHLY] = "Monthly",
        [LOIM_TEXT_SUBSCRIPTION_YEARLY] = "Yearly",
        [LOIM_TEXT_SUBSCRIPTION_TEAM] = "Team",
        [LOIM_TEXT_SUBSCRIPTION_ENTERPRISE] = "Enterprise",
        [LOIM_TEXT_SUBSCRIPTION_PAID] = "Paid",
        [LOIM_TEXT_SUBSCRIPTION_FREE] = "Free",
        [LOIM_TEXT_RESTORING_LOGIN] = "Restoring your account session...",
        [LOIM_TEXT_SESSION_DETAILS_UNAVAILABLE] =
            "Unable to verify subscription details; please sign in again",
        [LOIM_TEXT_SESSION_EXPIRED] =
            "Your session expired; please sign in again",
        [LOIM_TEXT_CREDENTIAL_SAVE_FAILED] =
            "Signed in, but secure login storage is unavailable",
        [LOIM_TEXT_UPDATE_AVAILABLE_TITLE] = "LoimReader update available",
        [LOIM_TEXT_UPDATE_AVAILABLE_FORMAT] =
            "Version %s is available. Download it now?",
        [LOIM_TEXT_UPDATE_AVAILABLE_NOTES_FORMAT] =
            "Version %s is available.\n\n%.800s\n\nDownload it now?",
        [LOIM_TEXT_UPDATE_DOWNLOAD] = "Download",
        [LOIM_TEXT_UPDATE_LATER] = "Later",
        [LOIM_TEXT_UPDATE_OPEN_FAILED] = "Unable to open the update download page",
        [LOIM_TEXT_TOOLTIP_LOGIN] =
            "Sign in — remove the watermark from exports",
        [LOIM_TEXT_TOOLTIP_ACCOUNT] =
            "Account — view details or sign in again",
        [LOIM_TEXT_TOOLTIP_OPEN] =
            "Open — import one or more images (Ctrl+O)",
        [LOIM_TEXT_TOOLTIP_EXPORT] =
            "Export — save the layout as a PDF",
        [LOIM_TEXT_TOOLTIP_PRINT] =
            "Print — send the document to a printer",
        [LOIM_TEXT_TOOLTIP_COLUMNS] =
            "Layout — cycle one, two or three columns",
        [LOIM_TEXT_TOOLTIP_PAGE_NUMBERS] =
            "Page numbers — cycle position or hide",
        [LOIM_TEXT_TOOLTIP_AUTO_SPLIT] =
            "Auto split — recalculate the best page breaks",
        [LOIM_TEXT_TOOLTIP_LESS_MARGIN] =
            "Less margin — shrink the print margins",
        [LOIM_TEXT_TOOLTIP_MORE_MARGIN] =
            "More margin — widen the print margins",
        [LOIM_TEXT_TOOLTIP_PREVIEW_LARGER] =
            "Zoom in — enlarge preview pages (+)",
        [LOIM_TEXT_TOOLTIP_PREVIEW_SMALLER] =
            "Zoom out — shrink preview pages (-)",
        [LOIM_TEXT_TOOLTIP_ABOUT] =
            "About — version and account details",
        [LOIM_TEXT_PRO_PROMPT_TITLE] = "LoimReader Pro",
        [LOIM_TEXT_PRO_PROMPT_MESSAGE] =
            "PDF export, printing and account sign-in are LoimReader Pro "
            "features.\n\nVisit ctdy123.com to get LoimReader Pro.",
        [LOIM_TEXT_PRO_PROMPT_VISIT] = "Visit Website",
        [LOIM_TEXT_PRO_PROMPT_LATER] = "Not Now",
        [LOIM_TEXT_EDITION_COMMUNITY] = "Community",
        [LOIM_TEXT_EDITION_PRO] = "Pro"
    },
    [LOIM_LOCALE_ZH_CN] = {
        [LOIM_TEXT_APP_TITLE] = "影谷长图阅读器",
        [LOIM_TEXT_IMAGE_FILES] = "图像文件",
        [LOIM_TEXT_SIGNED_IN_FORMAT] = "已登录：%.180s",
        [LOIM_TEXT_LOGIN_PROMPT] = "请输入 ctdy123.com 账号",
        [LOIM_TEXT_HTTPS_UNAVAILABLE] = "HTTPS 功能不可用",
        [LOIM_TEXT_LOGIN_FIELDS_REQUIRED] = "请输入有效的邮箱和密码",
        [LOIM_TEXT_LOGIN_START_FAILED] = "无法开始登录",
        [LOIM_TEXT_SIGNING_IN] = "正在登录…",
        [LOIM_TEXT_LOGIN_NETWORK_ERROR] = "网络连接或服务器响应异常",
        [LOIM_TEXT_LOGIN_INVALID_CREDENTIALS] = "邮箱或密码错误",
        [LOIM_TEXT_LOGIN_DEVICE_LIMIT] =
            "设备数量已达上限，请在 ctdy123.com 管理设备",
        [LOIM_TEXT_LOGIN_ACCOUNT_DISABLED] = "账号当前不可用",
        [LOIM_TEXT_LOGIN_FAILED_FORMAT] = "登录失败（HTTP %d）",
        [LOIM_TEXT_LAYOUT_FAILED_FORMAT] = "排版失败：%s",
        [LOIM_TEXT_IMPORTED_FORMAT] =
            "已导入 %zu 个，跳过 %zu 个；共 %zu 张图、%zu 页",
        [LOIM_TEXT_LOADING_FORMAT] = "正在载入 %zu/%zu：%.160s",
        [LOIM_TEXT_IMPORT_PROGRESS_FORMAT] = "正在处理 %zu/%zu（%u%%）",
        [LOIM_TEXT_FILE_DIALOG_ERROR] = "文件选择器出错",
        [LOIM_TEXT_IMPORT_CANCELED] = "已取消导入",
        [LOIM_TEXT_IMPORT_BUSY] = "请等待当前导入完成",
        [LOIM_TEXT_IMPORT_QUEUE_FAILED] = "无法加入导入队列：内存不足",
        [LOIM_TEXT_CHOOSE_IMAGES] = "请选择一张或多张图片",
        [LOIM_TEXT_CLEAR_FAILED] = "无法清空文档：内存不足",
        [LOIM_TEXT_READY] = "就绪——可拖入图片或点击导入",
        [LOIM_TEXT_DROP_IMAGES] = "将图片拖到这里，或按 Ctrl+O",
        [LOIM_TEXT_EMPTY_TITLE] = "将图片拖放到这里",
        [LOIM_TEXT_EMPTY_SUBTITLE] =
            "支持同时导入多张图片，也可以点击选择文件",
        [LOIM_TEXT_EMPTY_PROCESSING] = "正在生成预览…",
        [LOIM_TEXT_CLEAR] = "清空",
        [LOIM_TEXT_CLEAR_TOOLTIP] = "清空当前图片并返回导入页面",
        [LOIM_TEXT_CLEARED] = "当前内容已清空",
        [LOIM_TEXT_PLEASE_WAIT] = "请稍候",
        [LOIM_TEXT_SIGN_IN] = "登录",
        [LOIM_TEXT_SIGN_IN_TITLE] = "登录 ctdy123.com",
        [LOIM_TEXT_EMAIL] = "邮箱",
        [LOIM_TEXT_PASSWORD] = "密码",
        [LOIM_TEXT_CANCEL] = "取消",
        [LOIM_TEXT_TWO_COLUMNS] = "双栏布局",
        [LOIM_TEXT_ONE_COLUMN] = "单栏布局",
        [LOIM_TEXT_THREE_COLUMNS] = "三栏布局",
        [LOIM_TEXT_SINGLE_PAGE] = "单页布局",
        [LOIM_TEXT_PAGE_NUMBERS_ON] = "已显示页码",
        [LOIM_TEXT_PAGE_NUMBERS_OFF] = "已隐藏页码",
        [LOIM_TEXT_PAGE_NUMBERS_NONE] = "页码：空白",
        [LOIM_TEXT_PAGE_NUMBERS_BOTTOM_RIGHT] = "页码：右下角",
        [LOIM_TEXT_PAGE_NUMBERS_BOTTOM_CENTER] = "页码：底部居中",
        [LOIM_TEXT_AUTO_SPLIT_APPLIED] = "已应用自动分割",
        [LOIM_TEXT_MARGIN_REDUCED] = "已减小打印边距",
        [LOIM_TEXT_MARGIN_INCREASED] = "已增大打印边距",
        [LOIM_TEXT_PREVIEW_ENLARGED] = "预览页面已放大",
        [LOIM_TEXT_PREVIEW_REDUCED] = "预览页面已缩小",
        [LOIM_TEXT_EXPORT_UNAVAILABLE] = "PDF 导出功能正在迁移中",
        [LOIM_TEXT_EXPORT_SUCCESS_FORMAT] = "PDF 已导出：%.180s",
        [LOIM_TEXT_EXPORT_FAILED] = "PDF 导出失败",
        [LOIM_TEXT_PRINT_UNAVAILABLE] = "打印功能正在迁移中",
        [LOIM_TEXT_PRINT_STARTED] = "打印对话框已打开",
        [LOIM_TEXT_PRINT_FAILED] = "无法启动打印",
        [LOIM_TEXT_SPLIT_OUTSIDE] = "分割位置超出文档范围",
        [LOIM_TEXT_SPLIT_UPDATED] = "手动分割已更新",
        [LOIM_TEXT_SPLIT_FAILED] = "无法在此处设置分割",
        [LOIM_TEXT_DROP_RELEASE] = "松开鼠标即可导入所选图片",
        [LOIM_TEXT_DROP_FAILED] = "无法接收拖入的文件",
        [LOIM_TEXT_LOGIN_UNAVAILABLE] = "此系统暂不支持账号登录",
        [LOIM_TEXT_REGISTER_NOW] = "没有账号？去注册",
        [LOIM_TEXT_REGISTER_OPEN_FAILED] = "无法打开注册页面",
        [LOIM_TEXT_ABOUT_TITLE] = "关于影谷长图阅读器",
        [LOIM_TEXT_ABOUT_VERSION_FORMAT] = "版本 %s · %s",
        [LOIM_TEXT_ABOUT_WEBSITE] = "ctdy123.com",
        [LOIM_TEXT_ABOUT_COPYRIGHT] = "Copyright © 2024 Ctdy123.com",
        [LOIM_TEXT_ABOUT_OK] = "确定",
        [LOIM_TEXT_ABOUT_OPEN_FAILED] = "无法打开官网",
        [LOIM_TEXT_ABOUT_ACCOUNT_SECTION] = "账户与订阅",
        [LOIM_TEXT_ABOUT_ACCOUNT_EMAIL_FORMAT] = "登录账户：%.180s",
        [LOIM_TEXT_ABOUT_SUBSCRIPTION_ACTIVE_FORMAT] = "订阅状态：有效（%s）",
        [LOIM_TEXT_ABOUT_SUBSCRIPTION_INACTIVE] = "订阅状态：免费或已过期",
        [LOIM_TEXT_ABOUT_SUBSCRIPTION_EXPIRES_FORMAT] = "到期时间：%.40s",
        [LOIM_TEXT_ABOUT_NOT_SIGNED_IN] = "尚未登录",
        [LOIM_TEXT_SUBSCRIPTION_MONTHLY] = "月度订阅",
        [LOIM_TEXT_SUBSCRIPTION_YEARLY] = "年度订阅",
        [LOIM_TEXT_SUBSCRIPTION_TEAM] = "团队版",
        [LOIM_TEXT_SUBSCRIPTION_ENTERPRISE] = "企业版",
        [LOIM_TEXT_SUBSCRIPTION_PAID] = "付费订阅",
        [LOIM_TEXT_SUBSCRIPTION_FREE] = "免费版",
        [LOIM_TEXT_RESTORING_LOGIN] = "正在恢复账户会话…",
        [LOIM_TEXT_SESSION_DETAILS_UNAVAILABLE] =
            "无法确认订阅详情，请重新登录",
        [LOIM_TEXT_SESSION_EXPIRED] = "会话已失效，请重新登录",
        [LOIM_TEXT_CREDENTIAL_SAVE_FAILED] =
            "登录成功，但系统安全凭据库当前不可用",
        [LOIM_TEXT_UPDATE_AVAILABLE_TITLE] = "发现影谷长图阅读器新版本",
        [LOIM_TEXT_UPDATE_AVAILABLE_FORMAT] =
            "新版本 %s 已发布，是否现在下载？",
        [LOIM_TEXT_UPDATE_AVAILABLE_NOTES_FORMAT] =
            "新版本 %s 已发布。\n\n%.800s\n\n是否现在下载？",
        [LOIM_TEXT_UPDATE_DOWNLOAD] = "下载",
        [LOIM_TEXT_UPDATE_LATER] = "稍后",
        [LOIM_TEXT_UPDATE_OPEN_FAILED] = "无法打开升级下载页面",
        [LOIM_TEXT_TOOLTIP_LOGIN] = "登录 — 登录账户以移除导出水印",
        [LOIM_TEXT_TOOLTIP_ACCOUNT] = "账户 — 查看账户详情或重新登录",
        [LOIM_TEXT_TOOLTIP_OPEN] = "打开 — 导入一张或多张图片（Ctrl+O）",
        [LOIM_TEXT_TOOLTIP_EXPORT] = "导出 — 将排版保存为 PDF",
        [LOIM_TEXT_TOOLTIP_PRINT] = "打印 — 将当前文档发送到打印机",
        [LOIM_TEXT_TOOLTIP_COLUMNS] = "布局 — 循环切换单栏、双栏、三栏",
        [LOIM_TEXT_TOOLTIP_PAGE_NUMBERS] = "页码 — 切换页码位置或隐藏",
        [LOIM_TEXT_TOOLTIP_AUTO_SPLIT] = "自动分页 — 重新计算最佳分页位置",
        [LOIM_TEXT_TOOLTIP_LESS_MARGIN] = "减少边距 — 缩小打印边距",
        [LOIM_TEXT_TOOLTIP_MORE_MARGIN] = "增加边距 — 增大打印边距",
        [LOIM_TEXT_TOOLTIP_PREVIEW_LARGER] = "放大预览 — 放大预览页面（+）",
        [LOIM_TEXT_TOOLTIP_PREVIEW_SMALLER] = "缩小预览 — 缩小预览页面（-）",
        [LOIM_TEXT_TOOLTIP_ABOUT] = "关于 — 查看版本与账户信息",
        [LOIM_TEXT_PRO_PROMPT_TITLE] = "专业版功能",
        [LOIM_TEXT_PRO_PROMPT_MESSAGE] =
            "PDF 导出、打印与账号登录为专业版功能。\n\n"
            "请前往 ctdy123.com 获取影谷长图阅读器专业版。",
        [LOIM_TEXT_PRO_PROMPT_VISIT] = "前往官网",
        [LOIM_TEXT_PRO_PROMPT_LATER] = "知道了",
        [LOIM_TEXT_EDITION_COMMUNITY] = "社区版",
        [LOIM_TEXT_EDITION_PRO] = "专业版"
    }
};

loim_locale loim_locale_from_name(const char *language)
{
    if (language != NULL &&
        (language[0] == 'z' || language[0] == 'Z') &&
        (language[1] == 'h' || language[1] == 'H')) {
        return LOIM_LOCALE_ZH_CN;
    }
    return LOIM_LOCALE_EN;
}

const char *loim_text(loim_locale locale, loim_text_key key)
{
    if (locale < 0 || locale >= LOIM_LOCALE_COUNT) {
        locale = LOIM_LOCALE_EN;
    }
    if (key < 0 || key >= LOIM_TEXT_KEY_COUNT) {
        return "";
    }
    return translations[locale][key];
}

const char *loim_status_text(loim_locale locale, loim_status status)
{
    static const char *const english[] = {
        "ok",
        "invalid argument",
        "out of memory",
        "I/O error",
        "unsupported image format",
        "corrupt image",
        "invalid data",
        "data is too large",
        "not found"
    };
    static const char *const chinese[] = {
        "成功",
        "参数无效",
        "内存不足",
        "输入/输出错误",
        "不支持的图像格式",
        "图像文件损坏",
        "数据格式无效",
        "数据大小超出限制",
        "未找到"
    };
    size_t index = (size_t)status;

    if (status < LOIM_OK || status > LOIM_ERROR_NOT_FOUND) {
        return locale == LOIM_LOCALE_ZH_CN ? "未知错误" : "unknown error";
    }
    return locale == LOIM_LOCALE_ZH_CN ? chinese[index] : english[index];
}

bool loim_locale_complete(loim_locale locale)
{
    int key;

    if (locale < 0 || locale >= LOIM_LOCALE_COUNT) {
        return false;
    }
    for (key = 0; key < (int)LOIM_TEXT_KEY_COUNT; ++key) {
        const char *translation = translations[locale][key];

        if (translation == NULL || translation[0] == '\0') {
            return false;
        }
    }
    return true;
}
