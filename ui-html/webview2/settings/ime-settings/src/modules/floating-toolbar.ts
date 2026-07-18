import { setupToggleButton } from './shared';
import { updateConfig } from './config-sync';
import ftbHTML from '../../../../ftb/default.html?raw';

export function setupFloatingToolbar(): void {
  mountFloatingToolbarPreview();

  setupToggleButton('ftbToggleBtn', (active) => {
    updateConfig('general.floating_toolbar', active);
    document.getElementById('ftbToggleBtn')?.setAttribute('aria-checked', String(active));
  });
}

function mountFloatingToolbarPreview(): void {
  const host = document.getElementById('ftbPreviewHost');
  if (!host) return;

  const source = new DOMParser().parseFromString(ftbHTML, 'text/html');
  const statusBar = source.querySelector<HTMLElement>('.status-bar');
  if (!statusBar) return;

  statusBar.querySelectorAll('#en, #fullwidth, #puncEn').forEach((element) => element.remove());
  statusBar.querySelectorAll<HTMLElement>('[id]').forEach((element) => element.removeAttribute('id'));
  host.replaceChildren(statusBar);
}
