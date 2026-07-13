// 下拉菜单功能
export function setupDropdownMenu(
  btnId: string,
  menuId: string,
  messageAction: string,
  useStopPropagation: boolean = false,
  configPath?: string
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

      // 分发给具体的函数去处理
      switch (messageAction) {
        case 'changeTheme':
          // setTheme(item.dataset.value);
          break;
        case 'changeCandidateArrange':
          const htmlItem = item as HTMLElement;
          applyCandidateArrange(htmlItem.dataset.value);
          break;
        default:
          break;
      }

      if (window.chrome?.webview && configPath) {
        const htmlItem = item as HTMLElement;
        window.chrome.webview.postMessage(JSON.stringify({
          type: 'configUpdate',
          data: {
            path: configPath,
            value: htmlItem.dataset.value
          }
        }));
      } else if (window.chrome?.webview && messageAction === 'changeCandidateArrange') {
        const htmlItem = item as HTMLElement;
        window.chrome.webview.postMessage(JSON.stringify({
          type: 'configUpdate',
          data: {
            path: 'appearance.candidate_window_layout',
            value: htmlItem.dataset.value
          }
        }));
      } else if (window.chrome?.webview) {
        const htmlItem = item as HTMLElement;
        window.chrome.webview.postMessage(JSON.stringify({
          type: messageAction,
          data: htmlItem.dataset.value
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

export function applyDropdownValue(btnId: string, menuId: string, value: string | undefined): void {
  if (!value) {
    return;
  }

  const btnLabel = document.querySelector<HTMLElement>(`#${btnId} span`);
  const item = document.querySelector<HTMLElement>(`#${menuId} .dropdown-item[data-value="${value}"]`);
  if (btnLabel && item) {
    btnLabel.textContent = item.textContent;
  }
}

// 切换按钮功能
export function setupToggleButton(btnId: string, onChanged?: (active: boolean) => void): void {
  const toggle = document.getElementById(btnId);
  if (!toggle) {
    console.warn(`Toggle button not found: ${btnId}`);
    return;
  }

  toggle.addEventListener('click', () => {
    toggle.classList.toggle('active');
    onChanged?.(toggle.classList.contains('active'));
  });
}

export function applyToggleState(btnId: string, active: boolean): void {
  document.getElementById(btnId)?.classList.toggle('active', active);
}

export function applyCandidateArrange(value: string | undefined): void {
  if (value !== 'horizontal' && value !== 'vertical') {
    return;
  }

  const wnd_h = document.getElementById('candidate-wnd-h');
  const wnd_v = document.getElementById('candidate-wnd-v');
  const label = document.querySelector<HTMLElement>('#arrangeBtn span');

  if (wnd_h) wnd_h.style.display = value === 'horizontal' ? 'flex' : 'none';
  if (wnd_v) wnd_v.style.display = value === 'vertical' ? 'flex' : 'none';
  if (label) label.textContent = value === 'horizontal' ? '横向' : '纵向';
}
