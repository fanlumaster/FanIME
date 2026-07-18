import { loadHTML } from '../utils/common-utils';
import ftbHTML from '../../../../ftb/default.html?raw';

export async function setupSkin(): Promise<void> {
  const vertical = document.getElementById('skinCandidateVertical');
  const horizontal = document.getElementById('skinCandidateHorizontal');
  if (vertical) vertical.innerHTML = await loadHTML('/src/partials/candidate/candidate-wnd-v.html');
  if (horizontal) horizontal.innerHTML = await loadHTML('/src/partials/candidate/candidate-wnd-h.html');

  const toolbar = document.getElementById('skinToolbarPreview');
  if (!toolbar) return;
  const source = new DOMParser().parseFromString(ftbHTML, 'text/html');
  const statusBar = source.querySelector<HTMLElement>('.status-bar');
  if (!statusBar) return;
  statusBar.querySelectorAll('#en, #fullwidth, #puncEn').forEach((element) => element.remove());
  statusBar.querySelectorAll<HTMLElement>('[id]').forEach((element) => element.removeAttribute('id'));
  toolbar.replaceChildren(statusBar);
}
