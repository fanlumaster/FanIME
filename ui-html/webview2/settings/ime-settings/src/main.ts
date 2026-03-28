import './styles/index.css';

import { loadHTML, showOnlyCurrentModule } from './utils/common-utils';
import { loadContent, setupSidebar } from './modules/sidebar';

const RESIZE_BORDER = 4;
const WINDOW_CONTROLS_RESIZE_BORDER = 2;
const MAXIMIZE_TOOLTIP_DELAY_MS = 1000;

const titlebarDragState = {
  isDraggingFromTitlebar: false,
  suspendCursorSyncUntilMouseMove: false
};

const windowState = {
  isMaximized: false
};

let onWindowStateChanged: ((isMaximized: boolean) => void) | null = null;

function applyMaximizeRestoreState(): void {
  const maximizeBtn = document.getElementById('btn-maximize');
  const restoreBtn = document.getElementById('btn-restore');

  if (!(maximizeBtn instanceof HTMLElement) || !(restoreBtn instanceof HTMLElement)) {
    return;
  }

  maximizeBtn.classList.toggle('is-hidden', windowState.isMaximized);
  restoreBtn.classList.toggle('is-hidden', !windowState.isMaximized);
}

function setWindowMaximized(isMaximized: boolean): void {
  windowState.isMaximized = isMaximized;
  applyMaximizeRestoreState();
  onWindowStateChanged?.(isMaximized);
}

async function initializeApp() {
  setupWindowStateSync();
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

function setupWindowStateSync(): void {
  if (!window.chrome?.webview) {
    return;
  }

  window.chrome.webview.addEventListener('message', (event: Event & { data?: any }) => {
    const payload = typeof event.data === 'string' ? safeParseJson(event.data) : event.data;
    if (!payload || typeof payload !== 'object') {
      return;
    }

    if (payload.type !== 'windowState') {
      return;
    }

    const nextState = payload.data;
    if (typeof nextState === 'boolean') {
      setWindowMaximized(nextState);
      return;
    }

    if (typeof nextState === 'string') {
      setWindowMaximized(nextState === 'maximized');
      return;
    }

    if (nextState && typeof nextState.isMaximized === 'boolean') {
      setWindowMaximized(nextState.isMaximized);
    }
  });
}

function safeParseJson(value: string): unknown {
  try {
    return JSON.parse(value);
  } catch {
    return null;
  }
}

function setupTitlebarButtons(): void {
  const minimizeBtn = document.getElementById('btn-minimize');
  const maximizeBtn = document.getElementById('btn-maximize');
  const restoreBtn = document.getElementById('btn-restore');
  const closeBtn = document.getElementById('btn-close');
  const windowControls = document.querySelector<HTMLElement>('.window-controls');
  let minimizeMessageTimer: number | null = null;
  let maximizeHoverTimer: number | null = null;
  let maximizeSnapRequested = false;

  const postWindowMessage = (value: 'minimize' | 'maximize' | 'restore' | 'close') => {
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

  const postSnapLayoutMessage = () => {
    if (windowState.isMaximized) {
      return;
    }

    if (window.chrome?.webview) {
      window.chrome.webview.postMessage(
        JSON.stringify({
          type: 'snapLayout',
          data: {
            source: 'maximizeButtonHover'
          }
        })
      );
    } else {
      console.warn('[snapLayout] webview2 not available');
    }
  };

  const getActiveMaxButton = (): HTMLElement | null => {
    if (windowState.isMaximized && restoreBtn instanceof HTMLElement) {
      return restoreBtn;
    }
    if (maximizeBtn instanceof HTMLElement) {
      return maximizeBtn;
    }
    return null;
  };

  const postMaximizeButtonRect = () => {
    if (!window.chrome?.webview) {
      return;
    }

    const activeBtn = getActiveMaxButton();
    if (!activeBtn) {
      return;
    }

    const rect = activeBtn.getBoundingClientRect();
    window.chrome.webview.postMessage(
      JSON.stringify({
        type: 'maximizeButtonRect',
        data: {
          x: rect.left,
          y: rect.top,
          width: rect.width,
          height: rect.height,
          dpr: window.devicePixelRatio || 1
        }
      })
    );
  };

  const clearMaximizeHoverTimer = () => {
    if (maximizeHoverTimer !== null) {
      window.clearTimeout(maximizeHoverTimer);
      maximizeHoverTimer = null;
    }
  };

  const resetMaximizeHoverState = () => {
    clearMaximizeHoverTimer();
    maximizeSnapRequested = false;
  };

  const restoreWindowControlsHoverState = () => {
    windowControls?.classList.remove('window-controls-click-reset');
  };

  windowControls?.addEventListener('mouseenter', restoreWindowControlsHoverState);

  onWindowStateChanged = () => {
    resetMaximizeHoverState();
    windowControls?.classList.remove('window-controls-click-reset');
    maximizeBtn?.classList.remove('host-hover', 'host-active');
    restoreBtn?.classList.remove('host-hover', 'host-active');
    maximizeBtn?.blur();
    restoreBtn?.blur();
    applyMaximizeRestoreState();
    postMaximizeButtonRect();
  };

  if (window.chrome?.webview) {
    window.chrome.webview.addEventListener('message', (event: Event & { data?: any }) => {
      const payload = typeof event.data === 'string' ? safeParseJson(event.data) : event.data;
      if (!payload || typeof payload !== 'object') {
        return;
      }

      if (payload.type !== 'maxButtonEvent') {
        return;
      }

      const eventType = payload.data?.event;
      if (typeof eventType !== 'string') {
        return;
      }

      const activeBtn = getActiveMaxButton();
      if (!activeBtn) {
        return;
      }

      if (eventType === 'enter') {
        windowControls?.classList.remove('window-controls-click-reset');
        activeBtn.classList.add('host-hover');
        return;
      }

      if (eventType === 'leave') {
        activeBtn.classList.remove('host-hover', 'host-active');
        return;
      }

      if (eventType === 'down') {
        activeBtn.classList.add('host-hover', 'host-active');
        return;
      }

      if (eventType === 'up') {
        activeBtn.classList.remove('host-active');
        resetMaximizeHoverState();
        if (windowState.isMaximized) {
          postWindowMessage('restore');
        } else {
          postWindowMessage('maximize');
        }
      }
    });
  }

  minimizeBtn?.addEventListener('click', () => {
    windowControls?.classList.add('window-controls-click-reset');

    if (minimizeBtn instanceof HTMLElement) {
      minimizeBtn.blur();
    }

    if (minimizeMessageTimer !== null) {
      window.clearTimeout(minimizeMessageTimer);
    }

    minimizeMessageTimer = window.setTimeout(() => {
      postWindowMessage('minimize');
      minimizeMessageTimer = null;
    }, 100);
  });
  maximizeBtn?.addEventListener('mouseenter', () => {
    if (windowState.isMaximized) {
      return;
    }

    clearMaximizeHoverTimer();

    if (maximizeSnapRequested) {
      return;
    }

    maximizeHoverTimer = window.setTimeout(() => {
      postSnapLayoutMessage();
      maximizeSnapRequested = true;
      maximizeHoverTimer = null;
    }, MAXIMIZE_TOOLTIP_DELAY_MS);
  });
  maximizeBtn?.addEventListener('mouseleave', resetMaximizeHoverState);
  maximizeBtn?.addEventListener('blur', resetMaximizeHoverState);
  maximizeBtn?.addEventListener('click', () => {
    resetMaximizeHoverState();
    postWindowMessage('maximize');
  });
  restoreBtn?.addEventListener('click', () => {
    resetMaximizeHoverState();
    postWindowMessage('restore');
  });
  closeBtn?.addEventListener('click', () => postWindowMessage('close'));

  postMaximizeButtonRect();

  if (maximizeBtn instanceof HTMLElement || restoreBtn instanceof HTMLElement) {
    const observer = new ResizeObserver(() => postMaximizeButtonRect());
    if (maximizeBtn instanceof HTMLElement) {
      observer.observe(maximizeBtn);
    }
    if (restoreBtn instanceof HTMLElement) {
      observer.observe(restoreBtn);
    }
  }

  window.addEventListener('resize', postMaximizeButtonRect);

  applyMaximizeRestoreState();
  onWindowStateChanged?.(windowState.isMaximized);
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

    let isOnResizeBorder = false;

    if (!windowState.isMaximized) {
      const activeBorder = isInWindowControlsArea(e.clientX, e.clientY)
        ? WINDOW_CONTROLS_RESIZE_BORDER
        : RESIZE_BORDER;
      isOnResizeBorder =
        e.clientX <= RESIZE_BORDER ||
        e.clientX >= window.innerWidth - activeBorder ||
        e.clientY <= activeBorder ||
        e.clientY >= window.innerHeight - RESIZE_BORDER;
    }

    if (isOnResizeBorder) {
      return;
    }

    titlebarDragState.isDraggingFromTitlebar = true;
    titlebarDragState.suspendCursorSyncUntilMouseMove = true;

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
  type ResizeHit = 'top' | 'client';

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
    if (windowState.isMaximized) {
      return 'client';
    }

    const isInWindowControls = isInWindowControlsArea(clientX, clientY);
    const topBorder = isInWindowControls ? WINDOW_CONTROLS_RESIZE_BORDER : RESIZE_BORDER;
    const onTop = clientY <= topBorder;
    if (onTop) return 'top';
    return 'client';
  }

  function getCursor(hit: ResizeHit): '' | 'ns-resize' {
    const cursorMap: Record<ResizeHit, '' | 'ns-resize'> = {
      top: 'ns-resize',
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
    const cursor = getCursor(hit);

    if (hit === 'client') {
      setResizeUiBlocked(false);
      setCursor('');
      return;
    }

    setResizeUiBlocked(true);
    setCursor(cursor);
  }

  function clearResizeCursorState(): void {
    setResizeUiBlocked(false);
    setCursor('');
  }

  document.addEventListener('mousemove', (e: MouseEvent) => {
    lastPointer = { clientX: e.clientX, clientY: e.clientY };
    titlebarDragState.suspendCursorSyncUntilMouseMove = false;

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

    if (titlebarDragState.isDraggingFromTitlebar) {
      titlebarDragState.isDraggingFromTitlebar = false;
      clearResizeCursorState();
      return;
    }

    if (titlebarDragState.suspendCursorSyncUntilMouseMove) {
      clearResizeCursorState();
      return;
    }

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
    if (titlebarDragState.suspendCursorSyncUntilMouseMove) {
      clearResizeCursorState();
      return;
    }
    updateCursorFromPoint(lastPointer.clientX, lastPointer.clientY);
  });
}

document.addEventListener('DOMContentLoaded', initializeApp);
