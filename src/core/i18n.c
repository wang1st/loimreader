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
        [LOIM_TEXT_AUTO_SPLIT_APPLIED] = "Automatic split applied",
        [LOIM_TEXT_MARGIN_REDUCED] = "Print margin reduced",
        [LOIM_TEXT_MARGIN_INCREASED] = "Print margin increased",
        [LOIM_TEXT_ZOOMED_IN] = "Editor zoomed in",
        [LOIM_TEXT_ZOOMED_OUT] = "Editor zoomed out",
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
            "Ready - account login unavailable on this system"
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
        [LOIM_TEXT_AUTO_SPLIT_APPLIED] = "已应用自动分割",
        [LOIM_TEXT_MARGIN_REDUCED] = "已减小打印边距",
        [LOIM_TEXT_MARGIN_INCREASED] = "已增大打印边距",
        [LOIM_TEXT_ZOOMED_IN] = "编辑区已放大",
        [LOIM_TEXT_ZOOMED_OUT] = "编辑区已缩小",
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
        [LOIM_TEXT_LOGIN_UNAVAILABLE] = "此系统暂不支持账号登录"
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
