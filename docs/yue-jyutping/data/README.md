# 數據資產說明

| 檔案 | 內容 | 來源 |
|---|---|---|
| `unihan_kcantonese.txt` | 29,936 個漢字嘅粵拼讀音，格式 `U+碼位<TAB>讀音1 讀音2 …`（帶調號 1–6） | Unicode Unihan 數據庫 `kCantonese` 欄位 |
| `jyutping_syllables.json` | 659 個實證音節：可用聲調、聲母/韻母分析、字數、例字 | 由上表統計生成 |
| `regen_unihan_data.py` | 重新生成以上兩個檔案嘅腳本（需要網絡） | — |

## 來源與授權

Unihan 數據庫下載自 <https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip>。

> Copyright © 1991–2025 Unicode, Inc. 本目錄嘅派生數據依 Unicode 數據授權條款
> （Unicode Data License）使用；如對數據做過修訂，發布時請一併保留本聲明並注明修改。

`kCantonese` 欄位嘅拼寫遵循香港語言學學會粵語拼音方案（粵拼），
見 <https://jyutping.org/jyutping/>。

## 重新生成

```powershell
python .\regen_unihan_data.py
```

生成後請運行 `..\scheme\test_jyutping_segment.py` 確認音節表基線（659 個實證音節）。
若官方數據庫更新導致音節數變化，需同步更新設計文檔 §2.5 嘅統計數字。
