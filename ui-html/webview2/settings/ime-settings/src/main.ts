import { onHostMessage } from './utils/host-messages';
import { serializeHostMessage } from '../../../shared/messages';
import './styles/critical.css';

import { loadHTML, showOnlyCurrentModule } from './utils/common-utils';
import { loadContent, scheduleBackgroundModuleLoad, setupSidebar } from './modules/sidebar';
import { setupConfigSync } from './modules/config-sync';

const RESIZE_BORDER = 4;
const WINDOW_CONTROLS_RESIZE_BORDER = 2;

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

  const sidebarContainer = document.getElementById('sidebar-container');
  if (sidebarContainer && !sidebarContainer.querySelector('.sidebar')) {
    sidebarContainer.innerHTML = await loadHTML('/src/partials/sidebar.html');
  }

  setupConfigSync();
  await loadContent('appearance');
  showOnlyCurrentModule('appearance');
  setupSidebar();
  void import('./styles/deferred.css');
  scheduleBackgroundModuleLoad();
}

function setupWindowStateSync(): void {
  if (!window.chrome?.webview) {
    return;
  }

  onHostMessage('windowState', payload => {
    setWindowMaximized(payload.data.isMaximized);
  });
}

function setupTitlebarButtons(): void {
  const minimizeBtn = document.getElementById('btn-minimize');
  const maximizeBtn = document.getElementById('btn-maximize');
  const restoreBtn = document.getElementById('btn-restore');
  const closeBtn = document.getElementById('btn-close');
  const windowControls = document.querySelector<HTMLElement>('.window-controls');
  let minimizeMessageTimer: number | null = null;

  const postWindowMessage = (value: 'minimize' | 'maximize' | 'restore' | 'close') => {
    if (window.chrome?.webview) {
      window.chrome.webview.postMessage(
        serializeHostMessage({
          type: 'windowControl',
          data: value
        })
      );
    } else {
      console.warn('[windowControl] webview2 not available:', value);
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
      serializeHostMessage({
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

  const restoreWindowControlsHoverState = () => {
    windowControls?.classList.remove('window-controls-click-reset');
  };

  windowControls?.addEventListener('mouseenter', restoreWindowControlsHoverState);

  onWindowStateChanged = () => {
    windowControls?.classList.remove('window-controls-click-reset');
    maximizeBtn?.classList.remove('host-hover', 'host-active');
    restoreBtn?.classList.remove('host-hover', 'host-active');
    maximizeBtn?.blur();
    restoreBtn?.blur();
    applyMaximizeRestoreState();
    postMaximizeButtonRect();
  };

  if (window.chrome?.webview) {
    onHostMessage('maxButtonEvent', payload => {
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
  maximizeBtn?.addEventListener('click', () => {
    postWindowMessage('maximize');
  });
  restoreBtn?.addEventListener('click', () => {
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

  let pendingDragStart: { startX: number; startY: number } | null = null;
  const dragStartThreshold = 2;

  const postWindowMessage = (value: 'maximize' | 'restore') => {
    if (window.chrome?.webview) {
      window.chrome.webview.postMessage(
        serializeHostMessage({
          type: 'windowControl',
          data: value
        })
      );
    } else {
      console.warn('[windowControl] webview2 not available:', value);
    }
  };

  const clearPendingDragStart = () => {
    pendingDragStart = null;
  };

  function isInWindowControlsArea(clientX: number, clientY: number): boolean {
    const controls = document.querySelector<HTMLElement>('.window-controls');
    if (!controls) {
      return false;
    }

    const rect = controls.getBoundingClientRect();
    return clientX >= rect.left && clientX <= rect.right && clientY >= rect.top && clientY <= rect.bottom;
  }

  titlebar.addEventListener('mousedown', (e: MouseEvent) => {
    if (e.button !== 0) return;

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

    if (e.detail > 1) {
      return;
    }

    clearPendingDragStart();
    pendingDragStart = { startX: e.clientX, startY: e.clientY };
  });

  titlebar.addEventListener('mousemove', (e: MouseEvent) => {
    if (!pendingDragStart || e.buttons !== 1) {
      return;
    }

    const deltaX = Math.abs(e.clientX - pendingDragStart.startX);
    const deltaY = Math.abs(e.clientY - pendingDragStart.startY);
    if (deltaX + deltaY < dragStartThreshold) {
      return;
    }

    pendingDragStart = null;
    titlebarDragState.isDraggingFromTitlebar = true;
    titlebarDragState.suspendCursorSyncUntilMouseMove = true;

    if (window.chrome?.webview) {
      window.chrome.webview.postMessage(
        serializeHostMessage({
          type: 'dragStart'
        })
      );
    }
  });

  titlebar.addEventListener('mouseup', () => {
    clearPendingDragStart();
  });

  titlebar.addEventListener('dblclick', (e: MouseEvent) => {
    if (e.button !== 0) return;

    const target = e.target as HTMLElement | null;
    if (target?.closest('.window-controls')) {
      return;
    }

    clearPendingDragStart();

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

    if (windowState.isMaximized) {
      postWindowMessage('restore');
    } else {
      postWindowMessage('maximize');
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
        serializeHostMessage({
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
