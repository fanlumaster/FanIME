import { updateConfig } from './config-sync';
import { setupToggleButton } from './shared';

const UPDATE_MANIFEST_URL = 'https://msime.app/update.json';
const RELEASES_PAGE_URL = 'https://github.com/metasequoiaime/MetasequoiaImeTsf/releases';
const REQUEST_TIMEOUT_MS = 10000;

type UpdateManifest = {
  version?: unknown;
  releaseUrl?: unknown;
};

type Version = {
  display: string;
  parts: number[];
};

function parseVersion(value: string): Version | null {
  const match = value.trim().match(/^v?(\d+(?:\.\d+)*)(?:[-+].*)?$/i);
  if (!match) {
    return null;
  }

  const display = match[1];
  if (!display) {
    return null;
  }

  return {
    display,
    parts: display.split('.').map(Number)
  };
}

function compareVersions(left: Version, right: Version): number {
  const length = Math.max(left.parts.length, right.parts.length);
  for (let index = 0; index < length; index += 1) {
    const difference = (left.parts[index] ?? 0) - (right.parts[index] ?? 0);
    if (difference !== 0) {
      return difference;
    }
  }
  return 0;
}

function postExternalUrl(url: string): void {
  if (window.chrome?.webview) {
    window.chrome.webview.postMessage(JSON.stringify({ type: 'openExternalUrl', data: url }));
    return;
  }

  window.open(url, '_blank', 'noopener,noreferrer');
}

function setDialogOpen(dialog: HTMLDialogElement, open: boolean): void {
  if (open) {
    if (!dialog.open) {
      dialog.showModal();
    }
    return;
  }

  if (dialog.open) {
    dialog.close();
  }
}

export function setupAboutSettings(): void {
  setupToggleButton('serverDiagnosticLogToggleBtn', (active) => {
    updateConfig('general.diagnostic_log', active);
  });
  setupToggleButton('tsfDiagnosticLogToggleBtn', (active) => {
    updateConfig('general.tsf_diagnostic_log', active);
  });

  const checkButton = document.getElementById('about-check-update');
  const versionLabel = document.querySelector('#about-settings .about-version');
  const statusLabel = document.getElementById('about-update-status');
  const dialog = document.getElementById('about-update-dialog');
  const dialogVersion = document.getElementById('about-update-dialog-version');
  const cancelButton = document.getElementById('about-update-cancel');
  const downloadButton = document.getElementById('about-update-download');

  if (!(checkButton instanceof HTMLButtonElement) ||
      !(versionLabel instanceof HTMLElement) ||
      !(statusLabel instanceof HTMLElement) ||
      !(dialog instanceof HTMLDialogElement) ||
      !(dialogVersion instanceof HTMLElement) ||
      !(cancelButton instanceof HTMLButtonElement) ||
      !(downloadButton instanceof HTMLButtonElement)) {
    console.warn('[about] update controls not found');
    return;
  }

  let releaseUrl = RELEASES_PAGE_URL;

  const setStatus = (message: string, kind: 'idle' | 'success' | 'error' = 'idle') => {
    statusLabel.textContent = message;
    statusLabel.dataset.kind = kind;
  };

  cancelButton.addEventListener('click', () => setDialogOpen(dialog, false));
  downloadButton.addEventListener('click', () => {
    setDialogOpen(dialog, false);
    postExternalUrl(releaseUrl);
  });
  dialog.addEventListener('click', (event) => {
    if (event.target === dialog) {
      setDialogOpen(dialog, false);
    }
  });

  checkButton.addEventListener('click', async () => {
    const currentVersion = parseVersion(versionLabel.textContent ?? '');
    if (!currentVersion) {
      setStatus('无法识别当前版本', 'error');
      return;
    }

    checkButton.disabled = true;
    checkButton.textContent = '正在检查…';
    setStatus('');

    const controller = new AbortController();
    const timeout = window.setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);

    try {
      const response = await fetch(`${UPDATE_MANIFEST_URL}?t=${Date.now()}`, {
        cache: 'no-store',
        signal: controller.signal
      });
      if (!response.ok) {
        throw new Error(`Update manifest returned ${response.status}`);
      }

      const manifest = await response.json() as UpdateManifest;
      if (typeof manifest.version !== 'string' ||
          typeof manifest.releaseUrl !== 'string' ||
          (manifest.releaseUrl !== RELEASES_PAGE_URL &&
           !manifest.releaseUrl.startsWith(`${RELEASES_PAGE_URL}/`))) {
        throw new Error('Invalid update manifest');
      }

      const latest = parseVersion(manifest.version);
      if (!latest) {
        throw new Error('Invalid release version');
      }

      if (compareVersions(latest, currentVersion) > 0) {
        releaseUrl = manifest.releaseUrl;
        dialogVersion.textContent = `v${latest.display}`;
        setStatus(`发现新版本 v${latest.display}`, 'success');
        setDialogOpen(dialog, true);
      } else {
        setStatus('已是最新版本', 'success');
      }
    } catch (error) {
      console.warn('[about] update check failed:', error);
      setStatus('检查失败，请稍后重试', 'error');
    } finally {
      window.clearTimeout(timeout);
      checkButton.disabled = false;
      checkButton.textContent = '检查更新';
    }
  });
}
