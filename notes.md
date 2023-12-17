## 什么是 `_In_`？

`_In_` 表示函数使用这个参数的值，但是不会去修改它

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

## 
