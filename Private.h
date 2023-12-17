// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

// 包含一些经常使用但不经常更改的标准
#include "stdafx.h"
// Source Annotation Language 头文件，用于指定函数参数和返回值的属性，提高代码的可读性和可维护性
#include "sal.h"

// COM Component Object Model 基础 API 头文件
#include <combaseapi.h>
// OLE 控件头文件，构建在 COM 基础之上，Object Linking and Embedding
#include <olectl.h>
// 断言相关的宏和函数
#include <assert.h>

// 安全字符串处理头文件，提供了一些安全的字符串处理函数
#include <strsafe.h>
// 提供了一些安全的整数运算函数
#include <intsafe.h>

// 用于初始化 GUID 的头文件
#include "initguid.h"
// Microsoft Text Framework 头文件，用于实现文本服务，ctf 或许也可以指 Collaborative Translation Framework
// 见：https://github.com/katahiromz/ImeStudy
#include "msctf.h"
// 包含了一些与文本服务相关的函数声明
#include "ctffunc.h"
