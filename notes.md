## 什么是 `_In_`

`_In_` 表示函数使用这个参数的值，但是不会去修改它

## 什么是 `_Inout_`

`_In_` 表示函数使用这个参数的值，在函数中也可能会去修改这个参数的值

## 什么是 RECT？

来自 `windef.h`

```cpp
typedef struct tagRECT
{
    LONG    left;
    LONG    top;
    LONG    right;
    LONG    bottom;
} RECT, *PRECT, NEAR *NPRECT, FAR *LPRECT;
```

这里的 LONG 就是 long。

(left, top) 是左上角坐标，(right, bottom) 是右下角坐标。如此，可以构成一个矩形。

## WCHAR 是什么

```cpp
#ifndef _MAC
typedef wchar_t WCHAR;    // wc,   16-bit UNICODE character
#else
```

就是一个 16 位的 unicode 字符。

## wchart_t 是什么

宽字符，编码为 UTF-16LE，也可以看一下上面的 WCHAR 条目。

## utf-8 是什么

这里的 8 表示一个字符的字节数是可变的。可以是一个字节表示一个字符，也可以是两个、三个、四个。目前，最多是四个字节，也就是 32 位。

## utf-16 是什么

就是用两个字节表示一个字符。但是其实也是可变的，它的单位一是两个字节，也就是 16 位，目前最多可以有两个单位，也就是 32 位。

## C++ 中指向对象的指针是怎么用的，和不用指针的区别在哪里


