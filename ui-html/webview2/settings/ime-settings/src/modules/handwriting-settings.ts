import { serializeHostMessage } from '../../../../shared/messages';
import type { ResolvedTheme } from './theme';

export function applyHandwritingPreviewTheme(theme: ResolvedTheme): void {
  const preview = document.querySelector<HTMLElement>('.handwriting-preview');
  preview?.classList.toggle('theme-light', theme === 'light');
  preview?.classList.toggle('theme-dark', theme === 'dark');

  const image = document.getElementById('handwritingPreviewImage');
  if (image instanceof HTMLImageElement) {
    image.src = theme === 'light' ? '/assets/handwriting_board_light.png' : '/assets/handwriting_board.png';
  }
}

export function setupHandwritingSettings(): void {
  document.getElementById('handwritingOpenButton')?.addEventListener('click', () => {
    window.chrome?.webview?.postMessage(serializeHostMessage({ type: 'openHandwritingPanel' }));
  });
}
