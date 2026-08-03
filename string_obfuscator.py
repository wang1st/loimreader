#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
字符串混淆工具
用于生成混淆的字符串常量，防止静态分析
"""

import sys
import os

def obfuscate_string(text):
    """混淆字符串"""
    # 简单的XOR加密
    key = "ctdy123_protection_key"
    result = []
    
    for i, char in enumerate(text):
        key_char = key[i % len(key)]
        encrypted = ord(char) ^ ord(key_char)
        result.append(f"0x{encrypted:02x}")
    
    return ", ".join(result)

def generate_obfuscated_strings():
    """生成常用的混淆字符串"""
    strings = {
        "free": "free",
        "DEVICE_LIMIT_EXCEEDED": "DEVICE_LIMIT_EXCEEDED", 
        "INVALID_CREDENTIALS": "INVALID_CREDENTIALS",
        "影谷长图阅读器 ctdy123.com": "影谷长图阅读器 ctdy123.com",
        "https://ctdy123.com/api/auth/client/login": "https://ctdy123.com/api/auth/client/login"
    }
    
    print("// 自动生成的混淆字符串常量")
    print("// 使用 OBFUSCATE_STR() 宏来解密")
    print()
    
    for name, text in strings.items():
        obfuscated = obfuscate_string(text)
        print(f"// {name}: {text}")
        print(f"static const char {name.upper().replace(' ', '_').replace('.', '_')}[] = {{{obfuscated}}};")
        print()

def main():
    if len(sys.argv) > 1:
        # 混淆命令行参数中的字符串
        text = sys.argv[1]
        obfuscated = obfuscate_string(text)
        print(f"原始字符串: {text}")
        print(f"混淆后: {{{obfuscated}}}")
        print(f"长度: {len(text)}")
    else:
        # 生成常用字符串
        generate_obfuscated_strings()

if __name__ == "__main__":
    main()
