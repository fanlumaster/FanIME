function postNativeMessage(type: string, data: string): void {
  window.chrome?.webview?.postMessage(JSON.stringify({ type, data }));
}

function openExternalUrl(url: string): void {
  postNativeMessage('openExternalUrl', url);
}

export function setupFeedbackSettings(): void {
  document.querySelectorAll<HTMLAnchorElement>('#feedback-settings .feedback-link').forEach((link) => {
    link.addEventListener('click', (event) => {
      event.preventDefault();
      openExternalUrl(link.href);
    });
  });

  document.querySelectorAll<HTMLButtonElement>('#feedback-settings [data-open-url]').forEach((button) => {
    button.addEventListener('click', () => {
      const url = button.dataset.openUrl;
      if (url) openExternalUrl(url);
    });
  });

  document.querySelectorAll<HTMLButtonElement>('#feedback-settings [data-copy-text]').forEach((button) => {
    button.addEventListener('click', () => {
      const value = button.dataset.copyText;
      if (!value) return;
      postNativeMessage('copyText', value);
      const originalLabel = button.textContent;
      button.textContent = '已复制';
      window.setTimeout(() => { button.textContent = originalLabel; }, 1600);
    });
  });
}
