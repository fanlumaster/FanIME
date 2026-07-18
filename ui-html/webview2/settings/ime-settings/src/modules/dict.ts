type DictionaryType = 'quanpin' | 'wubi' | 'english';
type DictionaryRow = { code?: string; word: string; weight?: number; display?: string };

let dictionary: DictionaryType = 'quanpin';
let editing: DictionaryRow | null = null;
let requestCounter = 0;
let lastQuery = '';
let lastAction = 'query';

function post(action: string, data: Record<string, unknown> = {}): void {
  lastAction = action;
  window.chrome?.webview?.postMessage(JSON.stringify({
    type: 'dictionaryRequest',
    data: { requestId: `dict-${++requestCounter}`, dictionary, action, ...data }
  }));
}

function showToast(message: string, ok: boolean): void {
  const toast = document.getElementById('dictToast');
  if (!toast) return;
  const table = document.querySelector<HTMLElement>('.dict-table-wrap');
  if (table) {
    const rect = table.getBoundingClientRect();
    toast.style.left = `${rect.left + rect.width / 2}px`;
  }
  toast.textContent = message;
  toast.className = `dict-toast visible ${ok ? 'success' : 'error'}`;
  window.setTimeout(() => toast.classList.remove('visible'), 2800);
}

function renderRows(rows: DictionaryRow[]): void {
  const body = document.getElementById('dictRows');
  if (!body) return;
  if (!rows.length) {
    body.innerHTML = `<tr><td colspan="${dictionary === 'english' ? 4 : 5}" class="dict-empty">没有找到词条</td></tr>`;
    return;
  }
  body.replaceChildren(...rows.map((row, index) => {
    const tr = document.createElement('tr');
    const indexCell = document.createElement('td');
    indexCell.className = 'dict-index-column';
    indexCell.textContent = String(index + 1);
    tr.appendChild(indexCell);
    const values = dictionary === 'english'
      ? [row.word, row.display ?? row.word]
      : [row.code ?? '', row.word, String(row.weight ?? 0)];
    values.forEach((value) => {
      const td = document.createElement('td');
      td.textContent = value;
      td.addEventListener('mouseenter', () => {
        if (td.scrollWidth > td.clientWidth) td.title = value;
        else td.removeAttribute('title');
      });
      tr.appendChild(td);
    });
    const actions = document.createElement('td');
    const edit = document.createElement('button'); edit.className = 'dict-row-action'; edit.textContent = '编辑';
    edit.addEventListener('click', () => openDialog(row));
    const remove = document.createElement('button'); remove.className = 'dict-row-action danger'; remove.textContent = '删除';
    remove.addEventListener('click', () => {
      if (!window.confirm(`确定删除“${row.word}”吗？`)) return;
      if (dictionary === 'english') post('delete', { oldWord: row.word, word: row.word, display: row.display ?? row.word });
      else post('delete', { oldCode: row.code, oldWord: row.word, code: row.code, word: row.word, weight: row.weight });
    });
    actions.append(edit, remove); tr.appendChild(actions); return tr;
  }));
}

function updateMode(): void {
  const english = dictionary === 'english';
  const search = document.getElementById('dictSearch') as HTMLInputElement;
  search.value = '';
  search.placeholder = english ? '输入英文前缀，例如 meta' : dictionary === 'quanpin'
    ? '输入完整全拼，例如 nihao' : '输入五笔编码前缀';
  document.getElementById('dictHint')!.textContent = english
    ? '按英文前缀查询全部匹配结果'
    : dictionary === 'quanpin' ? '全拼新增会校验拼音合法性、汉字数量和重复词条' : '管理 86 五笔编码、词条及权重';
  document.getElementById('dictTableHeader')!.innerHTML = english
    ? '<th class="dict-index-column">No.</th><th>单词</th><th>显示内容</th><th>操作</th>'
    : '<th class="dict-index-column">No.</th><th>编码</th><th>词条</th><th>权重</th><th>操作</th>';
  document.getElementById('dictRows')!.innerHTML = `<tr><td colspan="${english ? 4 : 5}" class="dict-empty">输入查询条件后查看词条</td></tr>`;
}

function openDialog(row: DictionaryRow | null = null): void {
  editing = row;
  const english = dictionary === 'english';
  document.getElementById('dictDialogTitle')!.textContent = row ? '编辑词条' : '新增词条';
  document.getElementById('dictCodeField')!.firstChild!.textContent = english ? '单词' : dictionary === 'quanpin' ? '全拼' : '五笔编码';
  (document.getElementById('dictCode') as HTMLInputElement).value = row
    ? (english ? row.word : row.code ?? '')
    : '';
  document.getElementById('dictWeightField')!.style.display = english ? 'none' : 'grid';
  (document.getElementById('dictWord') as HTMLInputElement).value = english ? row?.display ?? '' : row?.word ?? '';
  (document.getElementById('dictWeight') as HTMLInputElement).value = row?.weight === undefined ? '' : String(row.weight);
  const modal = document.getElementById('dictModal')!; modal.classList.add('open'); modal.setAttribute('aria-hidden', 'false');
}

function closeDialog(): void {
  const modal = document.getElementById('dictModal')!; modal.classList.remove('open'); modal.setAttribute('aria-hidden', 'true'); editing = null;
}

function query(): void {
  lastQuery = (document.getElementById('dictSearch') as HTMLInputElement).value.trim();
  if (!lastQuery) { showToast('请输入查询内容', false); return; }
  post('query', dictionary === 'english' ? { word: lastQuery } : { code: lastQuery });
}

export function setupDictionary(): void {
  document.querySelectorAll<HTMLButtonElement>('.dict-tab').forEach((tab) => tab.addEventListener('click', () => {
    document.querySelector('.dict-tab.active')?.classList.remove('active'); tab.classList.add('active');
    dictionary = tab.dataset.dictionary as DictionaryType; updateMode();
  }));
  document.getElementById('dictSearchButton')?.addEventListener('click', query);
  document.getElementById('dictSearch')?.addEventListener('keydown', (event) => { if ((event as KeyboardEvent).key === 'Enter') query(); });
  document.getElementById('dictAddButton')?.addEventListener('click', () => openDialog());
  document.getElementById('dictCancelButton')?.addEventListener('click', closeDialog);
  document.addEventListener('keydown', (event: KeyboardEvent) => {
    if (event.key === 'Escape' && document.getElementById('dictModal')?.classList.contains('open')) {
      event.preventDefault();
      closeDialog();
      (document.activeElement as HTMLElement | null)?.blur();
    }
  });
  document.getElementById('dictSaveButton')?.addEventListener('click', () => {
    const code = (document.getElementById('dictCode') as HTMLInputElement).value.trim();
    const word = (document.getElementById('dictWord') as HTMLInputElement).value.trim();
    if (dictionary === 'english') post(editing ? 'update' : 'create', { word: code, display: word, oldWord: editing?.word });
    else post(editing ? 'update' : 'create', { code, word, weight: Number((document.getElementById('dictWeight') as HTMLInputElement).value), oldCode: editing?.code, oldWord: editing?.word });
  });
  window.chrome?.webview?.addEventListener('message', (event: Event & { data?: any }) => {
    const payload = typeof event.data === 'string' ? JSON.parse(event.data) : event.data;
    if (payload?.type !== 'dictionaryResponse') return;
    showToast(payload.message ?? (payload.ok ? '操作成功' : '操作失败'), Boolean(payload.ok));
    if (Array.isArray(payload.rows)) renderRows(payload.rows);
    if (payload.ok && lastAction !== 'query') { closeDialog(); if (lastQuery) query(); }
  });
  updateMode();
}
