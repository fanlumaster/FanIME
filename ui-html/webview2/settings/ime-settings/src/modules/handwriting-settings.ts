export function setupHandwritingSettings(): void {
  document.getElementById('handwritingOpenButton')?.addEventListener('click', () => {
    window.chrome?.webview?.postMessage(JSON.stringify({ type: 'openHandwritingPanel' }));
  });
}
