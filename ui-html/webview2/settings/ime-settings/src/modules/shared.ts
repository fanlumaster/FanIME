// 下拉菜单功能
import { setSurfaceTheme, setThemeMode } from './theme';

export function setupDropdownMenu(
  btnId: string,
  menuId: string,
  messageAction: string,
  useStopPropagation: boolean = false,
  configPath?: string,
  valueTransform: (value: string) => unknown = (value) => value
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
    const willOpen = !menu.classList.contains('open');
    document.querySelectorAll('.dropdown-menu.open').forEach((openMenu) => {
      if (openMenu !== menu) {
        openMenu.classList.remove('open');
      }
    });
    menu.classList.toggle('open', willOpen);
  });

  // Event delegation so dynamically rebuilt items (e.g. system fonts) keep working.
  menu.addEventListener('click', (event: Event) => {
    const item = (event.target as HTMLElement | null)?.closest('.dropdown-item') as HTMLElement | null;
    if (!item || !menu.contains(item)) {
      return;
    }

    textSpan.textContent = item.textContent;

    switch (messageAction) {
      case 'changeTheme':
        setThemeMode(item.dataset.value);
        break;
      case 'changeSettingsTheme':
        setSurfaceTheme('settings', item.dataset.value);
        break;
      case 'changeCandTheme':
        setSurfaceTheme('cand', item.dataset.value);
        break;
      case 'changeFtbTheme':
        setSurfaceTheme('ftb', item.dataset.value);
        break;
      case 'changeMenuTheme':
        setSurfaceTheme('menu', item.dataset.value);
        break;
      case 'changeCandidateArrange':
        applyCandidateArrange(item.dataset.value);
        break;
      default:
        break;
    }

    // Always run transform first so local preview updates even without WebView2
    // (e.g. vite browser preview).
    const nextValue = configPath
      ? valueTransform(item.dataset.value ?? '')
      : (item.dataset.value ?? '');

    if (window.chrome?.webview && configPath) {
      window.chrome.webview.postMessage(JSON.stringify({
        type: 'configUpdate',
        data: {
          path: configPath,
          value: nextValue
        }
      }));
    } else if (window.chrome?.webview && messageAction === 'changeCandidateArrange') {
      window.chrome.webview.postMessage(JSON.stringify({
        type: 'configUpdate',
        data: {
          path: 'appearance.candidate_window_layout',
          value: item.dataset.value
        }
      }));
    } else if (window.chrome?.webview) {
      window.chrome.webview.postMessage(JSON.stringify({
        type: messageAction,
        data: item.dataset.value
      }));
    }

    menu.classList.remove('open');
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
  if (!btnLabel) {
    return;
  }

  const item = Array.from(document.querySelectorAll<HTMLElement>(`#${menuId} .dropdown-item`)).find(
    (el) => el.dataset.value === value
  );
  btnLabel.textContent = item?.textContent || value;
}

export function populateDropdownMenu(
  menuId: string,
  values: string[],
  options?: { selected?: string; previewFont?: boolean }
): void {
  const menu = document.getElementById(menuId);
  if (!menu) {
    return;
  }

  const names = [...values];
  const selected = options?.selected;
  if (selected && !names.includes(selected)) {
    names.unshift(selected);
  }

  const fragment = document.createDocumentFragment();
  for (const name of names) {
    const item = document.createElement('div');
    item.className = 'dropdown-item';
    item.dataset.value = name;
    item.textContent = name;
    if (options?.previewFont) {
      const quoted = /\s/.test(name) ? `"${name.replace(/"/g, '\\"')}"` : name;
      item.style.fontFamily = `${quoted}, sans-serif`;
    }
    fragment.appendChild(item);
  }
  menu.replaceChildren(fragment);
}

// 切换按钮功能
export function setupToggleButton(btnId: string, onChanged?: (active: boolean) => void): void {
  const toggle = document.getElementById(btnId);
  if (!toggle) {
    console.warn(`Toggle button not found: ${btnId}`);
    return;
  }

  const toggleState = () => {
    toggle.classList.toggle('active');
    const active = toggle.classList.contains('active');
    toggle.setAttribute('aria-checked', String(active));
    onChanged?.(active);
  };

  toggle.addEventListener('click', toggleState);
  toggle.addEventListener('keydown', (event: KeyboardEvent) => {
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault();
      toggleState();
    }
  });
}

export function applyToggleState(btnId: string, active: boolean): void {
  const toggle = document.getElementById(btnId);
  toggle?.classList.toggle('active', active);
  toggle?.setAttribute('aria-checked', String(active));
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
