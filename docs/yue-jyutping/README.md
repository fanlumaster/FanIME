# 水杉輸入法 · 粵語（粵拼 Jyutping）輸入方案設計

> 狀態：設計提案（Design Proposal）
> 方案依據：[香港語言學學會粵語拼音方案（粵拼）](https://jyutping.org/jyutping/)，即 `jyutping.org/jyutping/` 所載之現行方案（含 2018 年新增之 `a`、`oet` 等韻母）。
> 本目錄同時包含可執行參考實作（`scheme/`）與數據資產（`data/`），詳見文末〈檔案清單〉。

---

## 1. 背景與目標

水杉輸入法（MSIME）README 明確列出「參與日語、注音、**粵語**等輸入方案的設計與實現」為開放貢獻方向。本提案為粵語輸入給出完整設計：

- **拼寫方案**：採用粵拼（Jyutping）——香港語言學學會方案，係目前學術、教育與資訊科技界事實標準（Unicode Unihan 數據庫之 `kCantonese` 欄位即採用粵拼）。
- **產品定位**：粵語作為與「中文」「日文」並列之第三種**輸入模式**（`input.mode = "cantonese"`），其下之拼寫方案為粵拼（預留 `input.cantonese_schema` 以容納未來耶魯拼音、教院式等變體）。
- **交付範圍**：本文檔給出方案表、輸入規則、切音算法、跨倉實現計劃、詞庫數據與授權評估、分階段路線圖；並附上以 Unihan `kCantonese` 構建之音節表（659 個實證音節）與 Python 參考切音器（26 個單元測試全數通過）。

非目標（留待後續提案）：粵語語音輸入、粵語口語專屬語法模型、方言變體（如懶音）之自動矯正策略細節。

---

## 2. 粵拼方案表（據 jyutping.org 官方頁面）

以下表格逐字摘自 <https://jyutping.org/jyutping/>（寬式國際音標轉寫）。

### 2.1 聲母表（19 個）

| 聲母 | IPA | 例字 | 聲母 | IPA | 例字 |
|---|---|---|---|---|---|
| b | [p] | 巴 | p | [pʰ] | 怕 |
| m | [m] | 媽 | f | [f] | 花 |
| d | [t] | 打 | t | [tʰ] | 他 |
| n | [n] | 那 | l | [l] | 啦 |
| g | [k] | 家 | k | [kʰ] | 卡 |
| ng | [ŋ] | 牙 | h | [h] | 蝦 |
| gw | [kʷ] | 瓜 | kw | [kʷʰ] | 夸 |
| w | [w] | 蛙 | z | [t͡s] | 渣 |
| c | [t͡sʰ] | 叉 | s | [s] | 沙 |
| j | [j] | 也 | | | |

**零聲母**不用字母標記，例如「呀」只拼作 `aa`。

### 2.2 鼻音單獨成韻（2 個）

| 韻 | IPA | 例字 |
|---|---|---|
| m | [m̩] | 唔 |
| ng | [ŋ̩] | 吳 |

> 注意：`m`、`ng` 既是聲母亦可獨立成韻，此乃粵拼切分之關鍵歧義源（見 §3.3）。

### 2.3 韻母表（現行官頁共 60 韻母，另 `m`/`ng` 兩個鼻音單獨成韻）

韻母 = 韻腹 + 韻尾（`-i -u -m -n -ng -p -t -k`）。摘錄官方韻母表（行＝韻腹，列＝韻尾）：

| 韻腹 | 單元音 | -i | -u | -m | -n | -ng | -p | -t | -k |
|---|---|---|---|---|---|---|---|---|---|
| i [iː] | i 思 | | iu 消 | im 閃 | in 先 | ing 升 | ip 攝 | it 泄 | ik 識 |
| yu [yː] | yu 書 | | | | yun 孫 | | | yut 雪 | |
| u [uː] | u 夫 | ui 灰 | | um* | un 歡 | ung 風 | up* | ut 闊 | uk 福 |
| e [e] | | ei 四 | | | en* | | | | |
| e [ɛː] | e 些 | | eu 掉 | em 𦧷 | | eng 鄭 | ep 夾 | et 坺 | ek 石 |
| eo [ɵ] | | eoi 需 | | | eon 詢 | | | eot 摔 | |
| oe [œː] | oe 鋸 | | | | | oeng 疆 | | oet* | oek 腳 |
| o [o]/[ɔː] | o 可 | oi 開 | ou 好 | | on 看 | ong 康 | | ot 喝 | ok 學 |
| a [ɐ] | a 嘞 | ai 擠 | au 周 | am 斟 | an 珍 | ang 增 | ap 汁 | at 侄 | ak 則 |
| aa [aː] | aa 渣 | aai 齋 | aau 嘲 | aam 站 | aan 讚 | aang 掙 | aap 集 | aat 扎 | aak 責 |

\* 標星為 2018 年新增或官頁未附例字之邊際韻母：`a`、`oet` 於 2018 年新增；`um`、`up`、`en` 於官頁無例字。`i 後接 -ng/-k` 時韻腹讀 [e]（星、識），`u 後接 -ng/-k` 時讀 [o]（風、福）——拼寫不变，屬音位規則，不影響輸入法編碼。

### 2.4 字調（6 個調號）

| | 平 | 上 | 去 | 入聲 |
|---|---|---|---|---|
| 陰 | 1 [˥] 詩 | 2 [˧˥] 史 | 3 [˧] 試 | 1 [˥] 識 / 3 [˧] 洩 |
| 陽 | 4 [˨˩] 時 | 5 [˩˧] 市 | 6 [˨] 事 | 6 [˨] 蝕 |

聲調以數目字 **1–6** 標於音節之後：`fu1` 夫、`fu2` 虎、`fu3` 副、`fu4` 扶、`fu5` 婦、`fu6` 父。入聲韻（`-p -t -k` 韻尾）只配 1/3/6 調，調號寫法與舒聲相同。

### 2.5 音節規模（實測）

以 Unicode Unihan 數據庫 `kCantonese` 欄位（29,936 字）統計：

| 指標 | 數值 |
|---|---|
| 有粵拼讀音之漢字 | 29,936 |
| 實證基本音節（不帶調） | **659** |
| 音節×聲調組合 | **1,869** |
| 各調音節數 | 1 調 453、2 調 295、3 調 384、4 調 236、5 調 157、6 調 344 |
| 最長音節 | 6 字母（如 `gwaang`、`kwaang`、`ngaang`） |
| 同音節最多字 | `zi`（387 字）、`ji`（320）、`jyu`（297）、`wai`（288）、`ci`（261） |

完整清單見 `data/jyutping_syllables.json`；逐字讀音見 `data/unihan_kcantonese.txt`。
官方方案中合法但 Unihan 未收之組合（如零聲母 `a`、`oet`）已在切音器中作可選支持（`allow_unattested`）。

---

## 3. 輸入互動設計

### 3.1 編碼字符集

| 按鍵 | 行為 |
|---|---|
| `a`–`z` | 音節字母 |
| `'`（撇號） | 強制音節分界（與全拼一致，沿用 `QuanpinScheme::handle_key` 之既有行為） |
| `1`–`6` | 聲調號（僅當「帶調輸入」開啟時；見 §3.4） |
| `Backspace` / `Esc` / `Return` | 與現有方案一致 |

### 3.2 切音算法（粵拼 → 音節序列）

粵拼連寫存在大量前綴歧義（實測 476 組「甲音節係乙音節前綴」），例如：

- `gwongzau`（廣州）= `gwong` + `zau`，但亦可切作 `gwo` + `ng` + `zau`；
- `ngong` 本身係一個音節（12 字），同時可切作 `ngo`+`ng`、`ng`+`ong`；
- `ng` 既是聲母（牙 `ngaa4`）又係韻母（吳 `ng4`）。

**算法**（參考實作：`scheme/jyutping_segment.py`；C++ 引擎應 1:1 移植）：

1. 先按 `'` 分段，段內獨立切分，結果按笛卡兒積組合；
2. 段內用動態規劃枚舉所有由**實證音節表**組成之覆蓋：
   - 第一優先：**覆蓋最多字符**（取得最遠可達位置）；
   - 第二優先：**音節數最少**（長音節優先，避免 `gwo`+`ng` 勝過 `gwong`）；
   - 第三優先：音節字頻權重（`char_count` 截斷於 256，僅作平手裁決）；
3. 結尾未能成節之殘片若為某音節之前綴，標記為 **pending**（保留於 composition，與全拼行為一致）；否則以較重罰分保留，由候選層決定是否顯示「無候選」；
4. 聲調數字（帶調模式）在音節邊界處被消費：`jyut6ping3` → `jyut` + `ping`；未實證之「音節+調」組合仍被接受（打字不阻塞），由候選查詢自然落空；
5. 輸出 top-N 切分（對應引擎 `cut_pinyin_by_mode` 返回多組切分的形態），供候選層做歧義回退。

**測試基線**：`scheme/test_jyutping_segment.py` 26 個用例（含 `jyutping`、`gwongzau`、`hoenggong`、`ng`、`m`、`ngong` 候選排序、pending、帶調、非法輸入），全部通過。新增/修改切音邏輯必須保持該基線。

### 3.3 `ng` 問題與歧義處理原則

- 音節表驅動：只有實證音節參與切分。`ngaa`（牙）只能切作 `ng`+`aa`，因為 `nga` 並非實證音節；`ngam`（暗）優先於 `n`+`gam`（字頻裁決）；
- 使用者可用 `'` 顯式指定邊界：`n'gam`、`ng'am` 等；
- 多候選切分（§3.2 第 5 點）令罕見切法仍可通過翻頁/候選項抵達。

### 3.4 聲調輸入（可選，默认關閉）

新增配置 `input.cantonese_tone`：

| 值 | 行為 |
|---|---|
| `off`（默认） | 數字鍵沿用現行候選選擇行為；不帶調检索 |
| `on` | 音節完整後輸入 `1`–`6` 附著為聲調，候選按調過濾 |

帶調模式下的**數字鍵衝突處理**（重要）：本倉 U 模式已有先例——「裸數字進編碼、`Shift+1..9` 選候選」（`src/Key/KeyEventSink.cpp:1357`、`src/Composition/CompositionProcessorEngine.cpp:2046`）。粵語帶調模式沿用同一交互：

- 裸數字 `1`–`6`：作為聲調附著到當前音節（切音器判定邊界）；
- `Shift+1..9`：選擇候選（與 U 模式一致）；
- 數字小鍵盤（`VK_NUMPAD0..9`）：保留候選選擇（TSF 已有此分類，見 `MetasequoiaIMEBaseStructure.cpp:258`），不與聲調衝突；
- 音節未完整時輸入數字：不附著，回退為候選選擇或忽略（由引擎按當前候選狀態決定，遵循「Test 與實際處理必須一致」之 TSF 硬約定）。

### 3.5 簡拼與快捷模式

- **簡拼**：取各音節首字母（零聲母音節取其首字母，即元音），如 `zgw` → `zung1 gwok3`。超級簡拼（`Shift+J`）直接復用，引擎側把「聲母」概念換成「音節首字母」即可。
- **以詞定字、快捷短語（K）、日期（T）、Unicode（U）、Emoji（E）、顏文字（M）**：模式無關，全部復用。
- **臨時粵語**：暫不需要（粵語本身即獨立模式）。

### 3.6 預編輯（preedit）顯示

復用 `src/Global/FanyDefines.h` 之 `raw` / `pinyin`（分詞）/ `empty` 三態，無需新增樣式枚舉：

- 原始按鍵：`jyutping`（含用戶輸入之 `'` 與調號原樣）；
- 分詞樣式：`jyut'ping`（或 `jyut6'ping3`，若帶調）；
- 候選窗內預編輯同上。

### 3.7 繁簡與標點

- 粵語模式**默認繁體輸出**（粵語書寫以繁體/傳承字形為主），但跟隨現行 `input.character_set` 設置，允許用戶顯式改簡體；建議在模式切換時不覆寫用戶已有之 `character_set` 選擇。
- 標點沿用中文標點集與智能標點規則；粵語專用標點（如引號內嘅用法）不在 P0 範圍。

---

## 4. 跨倉實現計劃（遵循根目錄 `AGENTS.md` 邊界）

> 產品級硬約定以本倉 `AGENTS.md` 為準。下列改動分佈在多個獨立倉庫，**任何協議/表名/配置改動必須雙端同步**。

### 4.1 引擎（`MSIME-Engine`）——核心

```
core/scheme_type.h            enum class SchemeType 追加 CantoneseJyutping（只追加，不重排）
schemes/cantonese_jyutping_scheme.{h,cpp}   IInputScheme 實作（仿 japanese_romaji_scheme）
cantonese/jyutping_syllabary.h   生成之音節表（由 data/ 產出，編譯期常量）
cantonese/jyutping_utils.{h,cpp} 切音 DP（移植 scheme/jyutping_segment.py）
cantonese/cantonese_query.{h,cpp} build_table_name（見 §5.2 分表規則）
providers/cantonese_candidate_provider.{h,cpp} 候選查詢（仿 japanese_candidate_provider）
providers/provider_registry.cpp  resolve()/reset_cache() 追加 case
```

`QueryRequest` 無需改結構：`normalized_segmentation` 放 `'` 連接之音節序列；帶調時調號保留在 `raw_input`，另在 provider 側解析過濾。

### 4.2 服務端（`MSIME-Server`）

| 位置 | 改動 |
|---|---|
| `src/config/ime_config.cpp` | `input.mode` 接受 `"cantonese"`；`GetConfiguredActiveInputScheme()` 在 cantonese 時返回 `SchemeType::CantoneseJyutping`；新增 `input.cantonese_schema`、`input.cantonese_tone` 讀寫（仿 `SetConfiguredInputScheme` 之白名單寫法） |
| `src/session/session_factory.cpp` | cantonese → `EngineInputSession(SchemeType::CantoneseJyutping)` |
| `src/session/engine_input_session.cpp` | 粵語模式下禁用全拼/雙拼專用路徑（helpcode、autocorrect），與 `Wubi`/`JapaneseRomaji` 同級處理 |
| `src/ipc/ipc.h` | **只追加**新 opcode。現行 `DataToTsfWorkerThreadMsgType::InputModeChanged = 18`（payload `"1"`=日文）、`MaxKnown = PunctuationLockChanged (21)`。新增 `CantoneseInputModeChanged = 22`（payload `"0"/"1"`）並把 `MaxKnown` 指向新值；已發布值 0–21 一律不動 |
| `src/ipc/event_listener.cpp` | 帶調模式下數字鍵歸屬判定（§3.4）；粵語模式下跳過全拼專有的雲聯想/AI 聯想前置條件（參考 `non_pinyin` 判斷處） |
| `src/webview2/windows_webview2.cpp` | `input.mode` 變更之 WebView2 同步路徑補 `"cantonese"` 分支 |

### 4.3 TSF 前端（本倉 `MSIME-Windows`）

改動力求最小——按鍵判定主要依賴 Server 回覆，本地僅需知道「是否粵語模式」：

| 位置 | 改動 |
|---|---|
| `src/Utils/FanyUtils.{h,cpp}` | `ReadConfiguredJapaneseInputMode()` 推廣為 `ReadConfiguredInputModeKind()`（返回 chinese/japanese/cantonese 三態），保留舊函數作包裝以免破壞既有調用 |
| `src/IPC/Ipc.h` | 與 Server 端**同步**追加 `CantoneseInputModeChanged = 22` 消息類型、`Global::CantoneseInputModeEnabled` 原子旗標，並同步 `MaxKnown`。現行日文旗標名為 `Global::JapaneseInputModeEnabled`（消息 `InputModeChanged = 18`），兩者結構體/枚舉佈局不動 |
| `src/IME/MetasequoiaIME.cpp` | Worker 消息分發追加新 msg_type（仿現有 `InputModeChanged` 分支寫 `Global::JapaneseInputModeEnabled` 之模式） |
| `src/LanguageBar/LanguageBar.cpp` + `.rc` + 資源 | 語言欄新增粵語圖標（淺/深色成對，同步資源 ID、`.rc`、`CMakeLists.txt` 與索引，遵循 §資源註冊約定） |
| `src/Key/KeyEventSink.cpp` | 帶調模式下數字鍵 `_IsKeyEaten` 判定（必須與 `_DispatchKeyDown` 實際分支一致，含離線 fallback；參考 U 模式既有註釋） |

**不得改動**：CLSID/profile GUID/compartment GUID（持久身份）；`DllMain`；IPC 既有結構體佈局（只追加消息類型，不改結構體）。

### 4.4 設置前端（`MSIME-UiHtml` → `webview2/settings/ime-settings/`）

- 輸入模式下拉追加「粵語（粵拼）」選項（值 `"cantonese"`）；
- 新增「帶調輸入」開關（`input.cantonese_tone`），默認關；
- 候選窗、懸浮工具欄無需結構改動（內容權威在 Server）。

### 4.5 詞庫（`MSIME-Dict` / `MSIME-Server` 資產）

見 §5。詞庫資產由 Server 加載，TSF 不解析詞庫（AGENTS.md 邊界）。

### 4.6 安裝（`Installer`）

`Prepare-PackageFiles.ps1` 收集詞庫時加入粵語詞庫文件；版本目錄升級時同步檢查。

---

## 5. 詞庫數據

### 5.1 數據來源與授權評估

| 來源 | 內容 | 授權 | 結論 |
|---|---|---|---|
| Unicode Unihan DB `kCantonese` | 29,936 字 × 粵拼（含調） | Unicode 數據授權（寬鬆，需署名） | ✅ 本提案已使用；發布時需在詞庫文檔中保留署名與修改聲明 |
| 粵語審音配詞字庫（CUHK） | 權威審音 | 學術用途，不可再分發 | ❌ 僅供人工校對參考 |
| rime-cantonese | 字表+詞表（粵拼） | LGPL-3.0（需核實當前版本） | ⚠️ 需與 GPL-3.0 兼容性核實後使用 |
| words.hk 粵典 | 詞彙+粵拼+釋義 | 待核實（疑似含 NC 條款） | ⚠️ 若含非商業條款，不得併入分發包 |
| 社區貢獻（README 歡迎詞庫 PR） | 行業/地區/興趣詞庫 | 貢獻者聲明 | ✅ 長期路線 |

**P0 最小可用集**：Unihan 單字表 + 人工整理之高頻詞條（建議首批 5,000–20,000 條常用粵語詞，以粵拼編碼、Tab 分隔「詞語、編碼、權重」三列，與現有批量導入格式一致）。

### 5.2 表結構與分表規則

為避免與全拼主庫 `msime.db` 之升級回放硬約定（`AGENTS.md` §全拼音庫分表命名）耦合，粵語詞庫建議**獨立成庫** `msime_cantonese.db`，但分表算法與全拼保持一致：

| 音節數（字數） | 表名 | 示例 |
|---|---|---|
| 1–7 | `yue_tbl_{N}_{首字母}` | `nei'hou` → `yue_tbl_2_n` |
| ≥ 8 | `yue_tbl_others_{首字母}` | 同全拼規則，禁止 `yue_tbl_8_*` |

另設單字表 `yue_single(code, value, weight)`（`code` 為帶調音節如 `jyut6`；查询時對不帶調輸入做前綴匹配，與 `japanese_lexicon` 之 `code LIKE ? ESCAPE '#'` 模式一致）。

> ⚠️ 決策點：若維護者傾向把粵語表併入 `msime.db`（如 `japanese_lexicon` 之先例），則建庫腳本、`build_table_name`、設置頁加詞、`user_dictionary_journal` 回放四處必須同步（AGENTS.md 明列），且需在回放測試中覆蓋粵語表名。兩種取態均需維護者拍板；本設計推薦獨立庫以降低回放風險。

### 5.3 建庫腳本（建議落位 `MSIME-Dict/makecikudb/yuedb/`）

```
create_db_and_table.py    建庫建表（規則同 §5.2）
import_unihan_single.py   由 data/unihan_kcantonese.txt 生成單字表（本目錄已附原型數據）
import_phrases.py         三列 TSV → 分表（首字母 = 第一音節首字符）
```

權重策略：P0 用 Unihan 字頻近似 + 人工詞頻；P2 接入粵語語料 n-gram（參考 `Metasequoia-n-gram` 倉）。

---

## 6. 分階段路線圖

| 階段 | 內容 | 驗收 |
|---|---|---|
| **P0 MVP** | 引擎方案+切音+單字候選；`input.mode="cantonese"` 全鏈路；設置頁選項；語言欄圖標 | 能以粵拼輸入任意 Unihan 收錄漢字；本文檔 §3.2 測試基線通過 |
| **P1** | 詞表與詞組輸入、簡拼、加詞（`create_word`）、獨立詞庫打包 | 常用 5,000 詞命中率 ≥ 90%（抽樣） |
| **P2** | 帶調輸入與調號過濾、調頻權重、懶音/模糊選項、用戶詞庫回放 | 帶調回归測試；升級回放不敗 |
| **P3** | n-gram 聯想、雲候選與 AI 聯想接入（沿用現有管道）、粵語專用標點/口語詞庫 | 與全拼同等功能面 |

每階段均須執行 `AGENTS.md` §构建与验证 之手動回归清單（x64/x86、首鍵/連打/選詞/翻頁/失焦回焦/斷管恢復、Excel/Chromium/UWP 宿主）。

---

## 7. 測試與驗收

- **引擎單測**（`MSIME-Engine/tests/src/test_cantonese.cpp`，仿 `test_pinyin.cpp`）：把 `scheme/test_jyutping_segment.py` 之用例移植為 C++ 向量；另加帶調、`'` 邊界、`ng` 歧義、空輸入、超長輸入（≥ 64 字符）用例。
- **IPC 協議測試**：追加新消息類型常量後，運行 `tests/src/test_ipc_protocol_constants.cpp` 所屬目標（AGENTS.md 要求）。
- **回放測試**：若採用 §5.2 決策，需補粵語表名之升級回放用例。
- **手動回归**：見 §6 各階段驗收欄。

---

## 8. 檔案清單（本目錄）

```
docs/yue-jyutping/
├── README.md                        ← 本文檔
├── scheme/
│   ├── jyutping_segment.py          ← 參考切音器（C++ 移植基準）
│   └── test_jyutping_segment.py     ← 26 個基線測試（全數通過）
└── data/
    ├── jyutping_syllables.json      ← 659 個實證音節（含調、字數、例字）
    └── unihan_kcantonese.txt        ← 29,936 字之粵拼讀音（Unihan kCantonese）
```

### 數據來源聲明

`data/unihan_kcantonese.txt` 與 `data/jyutping_syllables.json` 派生自 Unicode® Unihan 數據庫（<https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip>）之 `kCantonese` 欄位。
Copyright © 1991–2025 Unicode, Inc. 依 Unicode 數據授權條款使用；派生數據可能含修訂，使用時請一併保留本聲明。

### 參考

- 粵拼方案（官方頁）：<https://jyutping.org/jyutping/>
- 香港語言學學會粵拼頁：<https://lshk.org/jyutping-scheme/>
- 粵語審音配詞字庫：<https://humanum.arts.cuhk.edu.hk/Lexis/lexi-mf/>
- Unihan 數據庫：<https://www.unicode.org/charts/unihan.html>
- rime-cantonese：<https://github.com/rime/rime-cantonese>
