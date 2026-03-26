import './styles/index.css';

import { loadHTML, showOnlyCurrentModule } from './utils/common-utils';
import { loadContent, setupSidebar } from './modules/sidebar';

const RESIZE_BORDER = 6;
const WINDOW_CONTROLS_RESIZE_BORDER = 2;

async function initializeApp() {
  setupTitlebarButtons();
  setupTitlebarDrag();
  setupResizeHitTest();

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

  function isInWindowControlsArea(clientX: number, clientY: number): boolean {
    const controls = document.querySelector<HTMLElement>('.window-controls');
    if (!controls) {
      return false;
    }

    const rect = controls.getBoundingClientRect();
    return clientX >= rect.left && clientX <= rect.right && clientY >= rect.top && clientY <= rect.bottom;
  }

  titlebar.addEventListener('mousedown', (e: MouseEvent) => {
    const target = e.target as HTMLElement | null;
    if (target?.closest('.window-controls')) {
      return;
    }

    const activeBorder = isInWindowControlsArea(e.clientX, e.clientY)
      ? WINDOW_CONTROLS_RESIZE_BORDER
      : RESIZE_BORDER;
    const isOnResizeBorder =
      e.clientX <= RESIZE_BORDER ||
      e.clientX >= window.innerWidth - activeBorder ||
      e.clientY <= activeBorder ||
      e.clientY >= window.innerHeight - RESIZE_BORDER;

    if (isOnResizeBorder) {
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

function setupResizeHitTest(): void {
  type ResizeHit =
    | 'left'
    | 'right'
    | 'top'
    | 'bottom'
    | 'left-top'
    | 'right-top'
    | 'left-bottom'
    | 'right-bottom'
    | 'client';

  let lastCursor = '';
  let lastPointer: { clientX: number; clientY: number } | null = null;
  let resizeUiBlocked = false;

  function isInWindowControlsArea(clientX: number, clientY: number): boolean {
    const controls = document.querySelector<HTMLElement>('.window-controls');
    if (!controls) {
      return false;
    }

    const rect = controls.getBoundingClientRect();
    return clientX >= rect.left && clientX <= rect.right && clientY >= rect.top && clientY <= rect.bottom;
  }

  function getHitTest(clientX: number, clientY: number): ResizeHit {
    const width = window.innerWidth;
    const height = window.innerHeight;
    const isInWindowControls = isInWindowControlsArea(clientX, clientY);
    const topBorder = isInWindowControls ? WINDOW_CONTROLS_RESIZE_BORDER : RESIZE_BORDER;
    const rightBorder = isInWindowControls ? WINDOW_CONTROLS_RESIZE_BORDER : RESIZE_BORDER;

    const onLeft = clientX <= RESIZE_BORDER;
    const onRight = clientX >= width - rightBorder;
    const onTop = clientY <= topBorder;
    const onBottom = clientY >= height - RESIZE_BORDER;

    if (onLeft && onTop) return 'left-top';
    if (onRight && onTop) return 'right-top';
    if (onLeft && onBottom) return 'left-bottom';
    if (onRight && onBottom) return 'right-bottom';
    if (onLeft) return 'left';
    if (onRight) return 'right';
    if (onTop) return 'top';
    if (onBottom) return 'bottom';
    return 'client';
  }

  function getCursor(hit: ResizeHit): '' | 'nwse-resize' | 'nesw-resize' | 'ew-resize' | 'ns-resize' {
    const cursorMap: Record<ResizeHit, '' | 'nwse-resize' | 'nesw-resize' | 'ew-resize' | 'ns-resize'> = {
      'left-top': 'nwse-resize',
      'right-bottom': 'nwse-resize',
      'right-top': 'nesw-resize',
      'left-bottom': 'nesw-resize',
      left: 'ew-resize',
      right: 'ew-resize',
      top: 'ns-resize',
      bottom: 'ns-resize',
      client: ''
    };

    return cursorMap[hit];
  }

  function setCursor(cursor: '' | 'nwse-resize' | 'nesw-resize' | 'ew-resize' | 'ns-resize'): void {
    if (cursor === lastCursor) return;

    const elements = document.querySelectorAll<HTMLElement>(
      'html, body, .titlebar, .titlebar *, .window-controls, .window-controls *'
    );
    elements.forEach((element) => {
      element.style.cursor = cursor;
    });

    lastCursor = cursor;
  }

  function setResizeUiBlocked(blocked: boolean): void {
    if (blocked === resizeUiBlocked) return;

    document.documentElement.classList.toggle('resize-hit-active', blocked);
    document.body?.classList.toggle('resize-hit-active', blocked);
    resizeUiBlocked = blocked;
  }

  function updateCursorFromPoint(clientX: number, clientY: number): void {
    const hit = getHitTest(clientX, clientY);
    setResizeUiBlocked(hit !== 'client');
    setCursor(getCursor(hit));
  }

  document.addEventListener('mousemove', (e: MouseEvent) => {
    lastPointer = { clientX: e.clientX, clientY: e.clientY };

    updateCursorFromPoint(e.clientX, e.clientY);
  });

  document.addEventListener('mousedown', (e: MouseEvent) => {
    if (e.button !== 0) return;
    const hit = getHitTest(e.clientX, e.clientY);
    if (hit === 'client') return;

    if (window.chrome?.webview) {
      window.chrome.webview.postMessage(
        JSON.stringify({
          type: 'resizeStart',
          data: hit
        })
      );
    }
  });

  window.addEventListener('mouseup', (e: MouseEvent) => {
    if (e.button !== 0) return;
    lastPointer = { clientX: e.clientX, clientY: e.clientY };
    updateCursorFromPoint(e.clientX, e.clientY);
  });

  document.addEventListener('mouseleave', () => {
    setResizeUiBlocked(false);
    setCursor('');
  });

  window.addEventListener('blur', () => {
    setResizeUiBlocked(false);
    setCursor('');
  });

  window.addEventListener('resize', () => {
    if (!lastPointer) return;
    updateCursorFromPoint(lastPointer.clientX, lastPointer.clientY);
  });
}

document.addEventListener('DOMContentLoaded', initializeApp);
