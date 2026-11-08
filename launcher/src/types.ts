// SPDX-License-Identifier: AGPL-3.0-or-later
export type GameId = 'sly1' | 'sly2' | 'sly3';
export type DisplayMode = 'windowed' | 'borderless_fullscreen';

export interface VersionDefinition {
  game_id: GameId;
  id: string;
  label: string;
  boot_name: string;
  boot_sha1: string;
}

export interface FoundVersion {
  game_id: GameId;
  version_id: string;
  label: string;
  image: string;
}

export interface InstalledVersion {
  image: string;
  executable: string;
}

export interface LauncherConfig {
  disc_folder: string | null;
  game_ports: Record<string, string>;
  playable: Record<string, boolean>;
  display_mode: Record<string, DisplayMode>;
  installed: Record<string, InstalledVersion>;
}

export interface AppSnapshot {
  data_root: string;
  config: LauncherConfig;
  versions: VersionDefinition[];
  installed_versions: string[];
  startup_status: string | null;
}

export interface ScanFinished {
  inspected: number;
  errors: string[];
  found_count: number;
}

export interface SetupProgress {
  game_id: GameId;
  version_id: string;
  message: string;
}

export interface SetupFinished {
  game_id: GameId;
  version_id: string;
  success: boolean;
  message: string;
  installed: InstalledVersion | null;
}

export interface LaunchFinished {
  game_id: GameId;
  success: boolean;
  message: string;
}
