export const DICTIONARY_PAGE_SIZE = 100;

export function createDictionaryPager(table: HTMLElement, load: (offset: number) => void) {
  let offset = 0;
  let hasMore = false;
  let pending = false;
  const footer = document.createElement('div');
  footer.className = 'dict-pagination';
  const previous = document.createElement('button');
  const next = document.createElement('button');
  const status = document.createElement('span');
  previous.type = next.type = 'button';
  previous.className = next.className = 'dict-button secondary';
  previous.textContent = '上一页'; next.textContent = '下一页';
  status.setAttribute('aria-live', 'polite');
  footer.append(previous, status, next); table.after(footer);
  const sync = () => {
    previous.disabled = pending || offset === 0;
    next.disabled = pending || !hasMore;
  };
  previous.addEventListener('click', () => load(Math.max(0, offset - DICTIONARY_PAGE_SIZE)));
  next.addEventListener('click', () => load(offset + DICTIONARY_PAGE_SIZE));
  sync();
  return {
    get offset() { return offset; },
    loading() { pending = true; status.textContent = '查询中…'; sync(); },
    failed() { pending = false; status.textContent = '查询失败，请重试'; sync(); },
    reset() { offset = 0; hasMore = false; pending = false; status.textContent = ''; sync(); },
    update(start: number, count: number, more: boolean) {
      offset = start; hasMore = more; pending = false;
      status.textContent = count ? `第 ${start + 1}–${start + count} 条${more ? '，后面还有结果' : ''}` : '没有更多结果';
      sync();
    },
  };
}
