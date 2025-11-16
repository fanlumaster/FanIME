// 下拉菜单功能
export function setupDropdownMenu(
  btnId: string,
  menuId: string,
  messageAction: string,
  useStopPropagation: boolean = false
): void {
  const btn = document.getElementById(btnId);
  const menu = document.getElementById(menuId);

  if (!btn || !menu) {
    console.warn(`Elements not found for ${btnId} or ${menuId}`);
    return;
  }

  const textSpan = btn.querySelector('span');
  if (!textSpan) {
    console.warn(`Span not found in ${btnId}`);
    return;
  }

  btn.addEventListener('click', (e: Event) => {
    if (useStopPropagation) {
      e.stopPropagation();
    }
    menu.classList.toggle('open');
  });

  const menuItems = menu.querySelectorAll('.dropdown-item');
  menuItems.forEach((item: Element) => {
    item.addEventListener('click', () => {
      textSpan.textContent = item.textContent;

      if (window.chrome?.webview) {
        const htmlItem = item as HTMLElement;
        window.chrome.webview.postMessage(JSON.stringify({
          action: messageAction,
          value: htmlItem.dataset.value
        }));
      }

      menu.classList.remove('open');
    });
  });

  // 点击外部关闭
  document.addEventListener('click', (e: Event) => {
    const target = e.target as Node;
    if (!btn.contains(target) && !menu.contains(target)) {
      menu.classList.remove('open');
    }
  });
}

// 切换按钮功能
export function setupToggleButton(btnId: string): void {
  const toggle = document.getElementById(btnId);
  if (!toggle) {
    console.warn(`Toggle button not found: ${btnId}`);
    return;
  }

  toggle.addEventListener('click', () => {
    toggle.classList.toggle('active');
  });
}