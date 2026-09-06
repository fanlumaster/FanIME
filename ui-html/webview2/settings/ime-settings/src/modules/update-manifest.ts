// Validation and trust reporting for https://msime.app/update.json, kept apart from the settings DOM
// so it can be tested. The manifest publishes four fields about the installer; the settings page used
// to read only two of them, so the digest the release pipeline had already computed never reached the
// person who was about to download an unsigned executable.

export type Version = {
  display: string;
  parts: number[];
};

export type UpdateManifest = {
  version?: unknown;
  releaseUrl?: unknown;
  installerName?: unknown;
  installerSha256?: unknown;
  signed?: unknown;
};

export type ValidatedUpdate = {
  version: Version;
  releaseUrl: string;
  installerName: string | null;
  installerSha256: string | null;
  signed: boolean | null;
};

export function parseVersion(value: string): Version | null {
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

export function compareVersions(left: Version, right: Version): number {
  const length = Math.max(left.parts.length, right.parts.length);
  for (let index = 0; index < length; index += 1) {
    const difference = (left.parts[index] ?? 0) - (right.parts[index] ?? 0);
    if (difference !== 0) {
      return difference;
    }
  }
  return 0;
}

// The installer name reaches a command the user is told to paste, so it is restricted to the shape
// the release workflow produces rather than accepted as any string.
const INSTALLER_NAME = /^MetasequoiaIME_Setup_v[\w.-]+\.exe$/i;
const SHA256 = /^[0-9a-f]{64}$/;

export function validateManifest(manifest: UpdateManifest, releasesPageUrl: string): ValidatedUpdate | null {
  if (typeof manifest.version !== 'string' || typeof manifest.releaseUrl !== 'string') {
    return null;
  }
  if (manifest.releaseUrl !== releasesPageUrl && !manifest.releaseUrl.startsWith(`${releasesPageUrl}/`)) {
    return null;
  }
  const version = parseVersion(manifest.version);
  if (!version) {
    return null;
  }
  // The optional fields are reported when they are well formed and ignored otherwise. A manifest
  // that predates them, or that is served by something else, still yields a usable update check.
  return {
    version,
    releaseUrl: manifest.releaseUrl,
    installerName: typeof manifest.installerName === 'string' && INSTALLER_NAME.test(manifest.installerName)
      ? manifest.installerName
      : null,
    installerSha256: typeof manifest.installerSha256 === 'string' && SHA256.test(manifest.installerSha256)
      ? manifest.installerSha256
      : null,
    signed: typeof manifest.signed === 'boolean' ? manifest.signed : null
  };
}

export type InstallerTrust = {
  /** Shown prominently: the release is known not to carry a code signature. */
  warning: string | null;
  /** The command to run and the digest to expect, when the manifest published one. */
  verify: { command: string; sha256: string } | null;
};

export function describeInstallerTrust(update: ValidatedUpdate): InstallerTrust {
  const name = update.installerName ?? 'MetasequoiaIME_Setup_v<版本>.exe';
  return {
    warning: update.signed === false
      ? '该版本未经代码签名，SmartScreen 会拦截，且 uiAccess 失效（候选窗无法浮在以管理员身份运行的程序之上）。请务必核对下面的校验值。'
      : null,
    verify: update.installerSha256
      ? { command: `Get-FileHash .\\${name} -Algorithm SHA256`, sha256: update.installerSha256 }
      : null
  };
}
