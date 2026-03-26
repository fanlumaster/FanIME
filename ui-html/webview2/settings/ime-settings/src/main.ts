import './styles/index.css';

import { loadHTML, showOnlyCurrentModule } from './utils/common-utils';
import { loadContent, setupSidebar } from './modules/sidebar';

async function initializeApp() {
  setupTitlebarButtons();
  setupTitlebarDrag();

  // 加载侧边栏
  const sidebarHTML = await loadHTML('/src/partials/sidebar.html');
  document.getElementById('sidebar-container')!.innerHTML = sidebarHTML;

  const container = document.getElementById('content-container')!;
  container.style.visibility = 'hidden';

  // 先将所有的 sidebar 中的分区的 html 加载进来
  await loadContent('general');
  await loadContent('appearance');
  await loadContent('input');
  await loadContent('helpcode');

  // 加载默认内容(通用设置)，初始化功能
  // 隐藏其他的分区
  showOnlyCurrentModule('appearance');
  setupSidebar();

  container.style.visibility = 'visible';
}

function setupTitlebarButtons(): void {
  const minimizeBtn = document.getElementById('btn-minimize');
  const maximizeBtn = document.getElementById('btn-maximize');
  const closeBtn = document.getElementById('btn-close');

  const postWindowMessage = (value: 'minimize' | 'maximize' | 'close') => {
    if (window.chrome?.webview) {
      window.chrome.webview.postMessage(
        JSON.stringify({
          type: 'windowControl',
          data: value
        })
      );
    } else {
      console.warn('[windowControl] webview2 not available:', value);
    }
  };

  minimizeBtn?.addEventListener('click', () => postWindowMessage('minimize'));
  maximizeBtn?.addEventListener('click', () => postWindowMessage('maximize'));
  closeBtn?.addEventListener('click', () => postWindowMessage('close'));
}

function setupTitlebarDrag(): void {
  const titlebar = document.querySelector<HTMLElement>('.titlebar');
  if (!titlebar) {
    console.warn('[windowControl] titlebar not found');
    return;
  }

  titlebar.addEventListener('mousedown', (e: MouseEvent) => {
    const target = e.target as HTMLElement | null;
    if (target?.closest('.window-controls')) {
      return;
    }

    if (window.chrome?.webview) {
      window.chrome.webview.postMessage(
        JSON.stringify({
          type: 'dragStart'
        })
      );
    }
  });
}

document.addEventListener('DOMContentLoaded', initializeApp);
