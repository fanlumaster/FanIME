import type { SettingsMessage } from '../../../../shared/messages';
type DictionaryRequest = Extract<SettingsMessage, { type: 'dictionaryRequest' }>['data'];
import { serializeHostMessage, isServerMessage } from '../../../../shared/messages';
import { setupToggleButton } from './shared';
import { updateConfig } from './config-sync';

type QuickPhraseRow = { code: string; word: string; weight: number };

let editing: QuickPhraseRow | null = null;
let requestCounter = 0;
let lastAction = 'query';
let toastTimer: number | null = null;

function post(action: DictionaryRequest['action'], data: Partial<Omit<DictionaryRequest, 'action' | 'requestId'>> = {}): void {
  lastAction = action;
  window.chrome?.webview?.postMessage(serializeHostMessage({
    type: 'dictionaryRequest',
    data: { requestId: `quick-${++requestCounter}`, dictionary: 'quick', action, ...data }
  }));
}

function syncHeader(): void {
  window.requestAnimationFrame(() => {
    const area = document.getElementById('quickPhraseTableWrap');
    const header = document.getElementById('quickPhraseTableHeaderWrap');
    if (area && header) header.style.paddingRight = `${area.offsetWidth - area.clientWidth}px`;
  });
}

function showToast(message: string, ok: boolean, durationMs = 3200): void {
  const toast = document.getElementById('quickPhraseToast');
  const table = document.getElementById('quickPhraseTableWrap');
  if (!toast) return;
  if (table) { const rect = table.getBoundingClientRect(); toast.style.left = `${rect.left + rect.width / 2}px`; }
  document.getElementById('quickPhraseToastMessage')!.textContent = message;
  document.getElementById('quickPhraseToastIcon')!.textContent = ok ? '' : '!';
  toast.className = `dict-toast visible ${ok ? 'success' : 'error'}`;
  if (toastTimer !== null) window.clearTimeout(toastTimer);
  toastTimer = window.setTimeout(() => { toast.classList.remove('visible'); toastTimer = null; }, durationMs);
}

function renderRows(rows: QuickPhraseRow[]): void {
  const body = document.getElementById('quickPhraseRows');
  if (!body) return;
  if (!rows.length) { body.innerHTML = '<tr><td colspan="5" class="dict-empty">没有找到快捷短语</td></tr>'; syncHeader(); return; }
  body.replaceChildren(...rows.map((row, index) => {
    const tr = document.createElement('tr');
    [String(index + 1), row.code, row.word, String(row.weight)].forEach((value, cellIndex) => {
      const td = document.createElement('td'); td.textContent = value;
      if (cellIndex === 0) td.className = 'dict-index-column';
      td.addEventListener('mouseenter', () => td.scrollWidth > td.clientWidth ? td.title = value : td.removeAttribute('title'));
      tr.appendChild(td);
    });
    const actions = document.createElement('td');
    const edit = document.createElement('button'); edit.className = 'dict-row-action'; edit.textContent = '编辑'; edit.addEventListener('click', () => openDialog(row));
    const remove = document.createElement('button'); remove.className = 'dict-row-action danger'; remove.textContent = '删除';
    remove.addEventListener('click', () => { if (window.confirm(`确定删除“${row.word}”吗？`)) post('delete', { oldCode: row.code, oldWord: row.word, code: row.code, word: row.word, weight: row.weight }); });
    actions.append(edit, remove); tr.appendChild(actions); return tr;
  }));
  syncHeader();
}

function openDialog(row: QuickPhraseRow | null = null): void {
  editing = row;
  document.getElementById('quickPhraseDialogTitle')!.textContent = row ? '编辑快捷短语' : '新增快捷短语';
  (document.getElementById('quickPhraseCode') as HTMLInputElement).value = row?.code ?? '';
  (document.getElementById('quickPhraseValue') as HTMLInputElement).value = row?.word ?? '';
  (document.getElementById('quickPhraseWeight') as HTMLInputElement).value = row ? String(row.weight) : '10';
  const modal = document.getElementById('quickPhraseModal')!; modal.classList.add('open'); modal.setAttribute('aria-hidden', 'false');
}

function closeDialog(): void {
  const modal = document.getElementById('quickPhraseModal')!; modal.classList.remove('open'); modal.setAttribute('aria-hidden', 'true'); editing = null;
}

function query(): void {
  post('query', { code: (document.getElementById('quickPhraseSearch') as HTMLInputElement).value.trim() });
}

function downloadExport(content: string, filename: string): void {
  const blob = new Blob([new Uint8Array([0xef, 0xbb, 0xbf]), content], {
    type: 'text/plain;charset=utf-8',
  });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = filename;
  anchor.style.display = 'none';
  document.body.appendChild(anchor);
  anchor.click();
  anchor.remove();
  window.setTimeout(() => URL.revokeObjectURL(url), 1000);
}

export function setupToolsSettings(): void {
  setupToggleButton('clipboardHistoryToggleBtn', (active) => {
    updateConfig('utility.clipboard_history', active);
  });
  setupToggleButton('quickPhraseToggleBtn', (active) => {
    updateConfig('utility.quick_phrase', active);
  });
  setupToggleButton('unicodeModeToggleBtn', (active) => {
    updateConfig('utility.unicode_mode', active);
  });
  setupToggleButton('dateTimeModeToggleBtn', (active) => {
    updateConfig('utility.date_time_mode', active);
  });
  setupToggleButton('emojiModeToggleBtn', (active) => {
    updateConfig('utility.emoji_mode', active);
  });
  setupToggleButton('kaomojiModeToggleBtn', (active) => {
    updateConfig('utility.kaomoji_mode', active);
  });
  setupToggleButton('jianpinModeToggleBtn', (active) => {
    updateConfig('utility.jianpin_mode', active);
  });
  setupToggleButton('yModeToggleBtn', (active) => {
    updateConfig('utility.y_mode', active);
  });
  setupToggleButton('rModeToggleBtn', (active) => {
    updateConfig('utility.r_mode', active);
  });
  document.getElementById('quickPhraseSearchButton')?.addEventListener('click', query);
  document.getElementById('quickPhraseSearch')?.addEventListener('keydown', event => { if ((event as KeyboardEvent).key === 'Enter') query(); });
  document.getElementById('quickPhraseAddButton')?.addEventListener('click', () => openDialog());
  document.getElementById('quickPhraseImportButton')?.addEventListener('click', () => {
    (document.getElementById('quickPhraseImportFile') as HTMLInputElement | null)?.click();
  });
  document.getElementById('quickPhraseImportFile')?.addEventListener('change', async (event) => {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];
    input.value = '';
    if (!file) return;
    try {
      const content = await file.text();
      if (!content.trim()) { showToast('文件内容为空', false); return; }
      post('import', { content });
    } catch {
      showToast('读取文件失败', false);
    }
  });
  document.getElementById('quickPhraseExportButton')?.addEventListener('click', () => {
    post('export', { dictionary: 'quick' });
  });
  document.getElementById('quickPhraseCancelButton')?.addEventListener('click', closeDialog);
  document.getElementById('quickPhraseSaveButton')?.addEventListener('click', () => {
    const code = (document.getElementById('quickPhraseCode') as HTMLInputElement).value.trim();
    const word = (document.getElementById('quickPhraseValue') as HTMLInputElement).value.trim();
    const weight = Number((document.getElementById('quickPhraseWeight') as HTMLInputElement).value);
    if (word.length > 199) { showToast('快捷短语不能超过 199 个 wchar 字符', false); return; }
    post(editing ? 'update' : 'create', { code, word, weight, oldCode: editing?.code, oldWord: editing?.word });
  });
  document.getElementById('quickPhraseToastClose')?.addEventListener('click', () => document.getElementById('quickPhraseToast')?.classList.remove('visible'));
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && document.getElementById('quickPhraseModal')?.classList.contains('open')) { event.preventDefault(); closeDialog(); (document.activeElement as HTMLElement | null)?.blur(); }
  });
  window.addEventListener('resize', syncHeader);
  window.chrome?.webview?.addEventListener('message', (event: Event & { data?: any }) => {
    const payload = typeof event.data === 'string' ? JSON.parse(event.data) : event.data;
    if (!isServerMessage(payload) || payload.type !== 'dictionaryResponse' || !String(payload.requestId ?? '').startsWith('quick-')) return;
    const isImport = lastAction === 'import';
    const isExport = lastAction === 'export';
    if (isExport && payload.ok && typeof payload.content === 'string' && typeof payload.filename === 'string') {
      downloadExport(payload.content, payload.filename);
    }
    showToast(payload.message ?? (payload.ok ? '操作成功' : '操作失败'), Boolean(payload.ok), isImport ? 5600 : 3200);
    renderRows(payload.rows.filter((row): row is QuickPhraseRow =>
      typeof row.code === 'string' && typeof row.weight === 'number'));
    if (payload.ok && lastAction !== 'query' && !isExport) { closeDialog(); query(); }
  });
}
