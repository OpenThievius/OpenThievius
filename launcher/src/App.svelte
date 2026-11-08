<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
<script lang="ts">
  import { onMount } from 'svelte';
  import { invoke } from '@tauri-apps/api/core';
  import { listen, type UnlistenFn } from '@tauri-apps/api/event';
  import { open } from '@tauri-apps/plugin-dialog';
  import type {
    AppSnapshot,
    DisplayMode,
    FoundVersion,
    GameId,
    LaunchFinished,
    ScanFinished,
    SetupFinished,
    SetupProgress
  } from './types';

  const games: { id: GameId; number: string; name: string; subtitle: string }[] = [
    { id: 'sly1', number: '01', name: 'Sly 1', subtitle: 'The Thievius Raccoonus' },
    { id: 'sly2', number: '02', name: 'Sly 2', subtitle: 'Band of Thieves' },
    { id: 'sly3', number: '03', name: 'Sly 3', subtitle: 'Honor Among Thieves' }
  ];

  let snapshot: AppSnapshot | null = null;
  let page: 'games' | 'settings' = 'games';
  let selectedGame: GameId = 'sly1';
  let found: FoundVersion[] = [];
  let scanning = false;
  let scanErrors: string[] = [];
  let status = 'Loading launcher…';
  let statusTone: 'neutral' | 'success' | 'error' = 'neutral';
  let setupBusy = '';
  let setupProgress = '';
  let movingData = false;
  let portPickerBusy = '';
  let errorDetailsOpen = false;

  $: currentGame = games.find((game) => game.id === selectedGame)!;
  $: versions = snapshot?.versions.filter((version) => version.game_id === selectedGame) ?? [];
  $: gameFound = found.filter((item) => item.game_id === selectedGame);
  $: playable = snapshot?.config.playable[selectedGame] !== false;
  $: displayMode = snapshot?.config.display_mode[selectedGame] ?? 'windowed';
  $: gamePort = snapshot?.config.game_ports[selectedGame] ?? '';

  onMount(() => {
    const unlisteners: UnlistenFn[] = [];
    let disposed = false;

    const init = async () => {
      unlisteners.push(
        await listen<FoundVersion>('scan-version-found', (event) => {
          const item = event.payload;
          if (!found.some((existing) => existing.image === item.image && existing.version_id === item.version_id)) {
            found = [...found, item];
          }
        })
      );
      unlisteners.push(
        await listen<ScanFinished>('scan-finished', (event) => {
          scanning = false;
          scanErrors = event.payload.errors;
          if (event.payload.found_count === 0) {
            status = event.payload.inspected === 0 && scanErrors.length > 0
              ? 'No disc images could be scanned. Check the selected folder and permissions.'
              : 'No valid disc images found.';
            statusTone = event.payload.inspected === 0 && scanErrors.length > 0 ? 'error' : 'neutral';
          } else {
            status = `${event.payload.found_count} supported disc image${event.payload.found_count === 1 ? '' : 's'} found among ${event.payload.inspected} image${event.payload.inspected === 1 ? '' : 's'}.`;
            statusTone = 'success';
          }
        })
      );
      unlisteners.push(
        await listen<SetupProgress>('setup-progress', (event) => {
          setupBusy = `${event.payload.game_id}:${event.payload.version_id}`;
          setupProgress = event.payload.message;
          status = event.payload.message;
          statusTone = 'neutral';
        })
      );
      unlisteners.push(
        await listen<SetupFinished>('setup-finished', async (event) => {
          setupBusy = '';
          setupProgress = '';
          status = event.payload.message;
          statusTone = event.payload.success ? 'success' : 'error';
          await refreshState();
        })
      );
      unlisteners.push(
        await listen<LaunchFinished>('launch-finished', (event) => {
          status = event.payload.message;
          statusTone = event.payload.success ? 'success' : 'error';
        })
      );
      unlisteners.push(
        await listen<string>('launcher-status', (event) => {
          status = event.payload;
          statusTone = 'neutral';
        })
      );

      try {
        await refreshState();
        if (!disposed && snapshot?.config.disc_folder) {
          await runScan();
          if (snapshot.startup_status) status = `${snapshot.startup_status} A disc scan has started.`;
        } else if (!disposed) {
          status = snapshot?.startup_status ?? 'Choose a disc image folder in Settings to get started.';
        }
      } catch (error) {
        status = messageOf(error);
        statusTone = 'error';
      }
    };

    void init();
    return () => {
      disposed = true;
      for (const unlisten of unlisteners) unlisten();
    };
  });

  async function refreshState() {
    snapshot = await invoke<AppSnapshot>('get_state');
  }

  async function runScan() {
    if (!snapshot?.config.disc_folder) {
      status = 'Choose a disc image folder in Settings to scan.';
      statusTone = 'neutral';
      return;
    }
    found = [];
    scanErrors = [];
    scanning = true;
    status = 'Scanning the selected folder and its subfolders…';
    statusTone = 'neutral';
    try {
      await invoke('scan_discs');
    } catch (error) {
      scanning = false;
      status = messageOf(error);
      statusTone = 'error';
    }
  }

  function discsFor(versionId: string) {
    return gameFound.filter((item) => item.version_id === versionId);
  }

  function installed(versionId: string) {
    return snapshot?.installed_versions.includes(`${selectedGame}:${versionId}`) ?? false;
  }

  function imageFor(versionId: string) {
    return discsFor(versionId)[0]?.image ?? snapshot?.config.installed[`${selectedGame}:${versionId}`]?.image ?? '';
  }

  async function setupVersion(versionId: string) {
    const image = discsFor(versionId)[0]?.image;
    if (!image) return;
    setupBusy = `${selectedGame}:${versionId}`;
    setupProgress = 'Preparing setup…';
    try {
      await invoke('setup_version', { gameId: selectedGame, versionId, image });
    } catch (error) {
      setupBusy = '';
      setupProgress = '';
      status = messageOf(error);
      statusTone = 'error';
    }
  }

  async function playVersion(versionId: string) {
    const image = imageFor(versionId);
    if (!image) return;
    try {
      await invoke('launch_game', { gameId: selectedGame, versionId, image });
      status = `${versionId} is starting…`;
      statusTone = 'neutral';
    } catch (error) {
      status = messageOf(error);
      statusTone = 'error';
    }
  }

  async function chooseDiscFolder() {
    const folder = await open({ directory: true, multiple: false, defaultPath: snapshot?.config.disc_folder ?? undefined });
    if (typeof folder !== 'string') return;
    try {
      await invoke('save_disc_folder', { path: folder });
      await refreshState();
      status = 'Disc folder saved. Starting a scan…';
      await runScan();
    } catch (error) {
      status = messageOf(error);
      statusTone = 'error';
    }
  }

  async function chooseDataFolder() {
    const folder = await open({ directory: true, multiple: false });
    if (typeof folder !== 'string') return;
    movingData = true;
    status = 'Copying and verifying launcher data…';
    statusTone = 'neutral';
    try {
      const result = await invoke<string>('move_data_directory', { path: folder });
      await refreshState();
      status = result;
      statusTone = 'success';
    } catch (error) {
      status = messageOf(error);
      statusTone = 'error';
    } finally {
      movingData = false;
    }
  }

  async function chooseGamePort(gameId: GameId) {
    portPickerBusy = gameId;
    try {
      const path = await open({ multiple: false, directory: false });
      if (typeof path !== 'string') return;
      await invoke('set_game_port', { gameId, path });
      await refreshState();
      status = `${games.find((game) => game.id === gameId)?.name} port path saved.`;
      statusTone = 'success';
    } catch (error) {
      status = messageOf(error);
      statusTone = 'error';
    } finally {
      portPickerBusy = '';
    }
  }

  async function setPlayable(gameId: GameId, value: boolean) {
    try {
      await invoke('set_playable', { gameId, playable: value });
      await refreshState();
      status = `${games.find((game) => game.id === gameId)?.name} play setting saved.`;
      statusTone = 'success';
    } catch (error) {
      status = messageOf(error);
      statusTone = 'error';
    }
  }

  async function setDisplayMode(gameId: GameId, mode: DisplayMode) {
    try {
      await invoke('set_display_mode', { gameId, mode });
      await refreshState();
      status = 'Display mode saved.';
      statusTone = 'success';
    } catch (error) {
      status = messageOf(error);
      statusTone = 'error';
    }
  }

  function messageOf(error: unknown) {
    return error instanceof Error ? error.message : String(error);
  }
</script>

<svelte:head>
  <title>OpenThievius · Launcher</title>
  <meta name="description" content="Set up and launch your OpenThievius games." />
</svelte:head>

<div class="app-shell">
  <header class="topbar">
    <a class="brand" href="#games" onclick={(event) => { event.preventDefault(); page = 'games'; }}>
      <span class="brand-mark">O</span>
      <span class="brand-name">OpenThievius</span>
    </a>
    <nav aria-label="Main navigation" class="main-nav">
      <button class:active={page === 'games'} onclick={() => (page = 'games')}>Games</button>
      <button class:active={page === 'settings'} onclick={() => (page = 'settings')}>Settings</button>
    </nav>
    <div class="top-actions">
      <span class="scan-indicator" class:busy={scanning}></span>
      <button class="quiet-button" disabled={scanning || !snapshot?.config.disc_folder} onclick={runScan}>
        {scanning ? 'Scanning…' : 'Scan now'}
      </button>
    </div>
  </header>

  <main>
    {#if page === 'games'}
      <div class="game-tabs" aria-label="Select a game">
        {#each games as game}
          <button class:chosen={selectedGame === game.id} onclick={() => (selectedGame = game.id)}>
            <span class="game-tab-number">{game.number}</span>
            <span>{game.name}</span>
          </button>
        {/each}
      </div>

      <section class="game-hero" class:game-one={selectedGame === 'sly1'} class:game-two={selectedGame === 'sly2'} class:game-three={selectedGame === 'sly3'}>
        <div class="hero-content">
          <div class="eyebrow">OPEN THIEVIUS · {currentGame.number}</div>
          <h1>{currentGame.name}</h1>
          <p class="hero-subtitle">{currentGame.subtitle}</p>
          <div class="hero-meta">
            <span class="status-dot" class:ready={versions.length > 0}></span>
            {versions.length > 0 ? `${versions.length} supported retail version${versions.length === 1 ? '' : 's'}` : 'Version definitions coming soon'}
          </div>
        </div>
        <div class="hero-mark" aria-hidden="true">{currentGame.number}</div>
      </section>

      <section class="content-section">
        <div class="section-heading">
          <div>
            <div class="eyebrow">YOUR LIBRARY</div>
            <h2>Versions</h2>
          </div>
          <div class="section-actions">
            <span class="folder-path" title={snapshot?.config.disc_folder ?? ''}>
              {snapshot?.config.disc_folder ?? 'Disc folder not set'}
            </span>
            <button class="outline-button" onclick={chooseDiscFolder}>Choose folder</button>
          </div>
        </div>

        <div class="status-banner" class:tone-success={statusTone === 'success'} class:tone-error={statusTone === 'error'}>
          <span class="status-banner-icon">{scanning ? '◌' : statusTone === 'success' ? '✓' : statusTone === 'error' ? '!' : 'i'}</span>
          <div class="status-copy">
            <span>{status}</span>
            {#if setupProgress && setupBusy}
              <span class="progress-track"><i></i></span>
            {/if}
          </div>
          {#if scanErrors.length > 0}
            <button class="text-button" onclick={() => (errorDetailsOpen = !errorDetailsOpen)}>
              {errorDetailsOpen ? 'Hide details' : 'Details'}
            </button>
          {/if}
        </div>
        {#if errorDetailsOpen && scanErrors.length > 0}
          <div class="error-details">{#each scanErrors as error}<p>{error}</p>{/each}</div>
        {/if}

        {#if versions.length === 0}
          <div class="empty-state">
            <span class="empty-icon">⌕</span>
            <h3>No supported versions yet</h3>
            <p>This game will use its project’s maintained retail version definitions when they are available.</p>
          </div>
        {:else}
          <div class="version-list">
            {#each versions as version (version.id)}
              {@const discs = discsFor(version.id)}
              {@const ready = installed(version.id)}
              {@const source = imageFor(version.id)}
              {@const selectedForPlay = ready && playable && !!gamePort && !!source}
              <article class="version-row">
                <div class="version-region">{version.id.toUpperCase()}</div>
                <div class="version-info">
                  <h3>{version.label}</h3>
                  <p>{#if discs.length > 0}{discs.length} verified image{discs.length === 1 ? '' : 's'} found{:else if ready}Set up · source image not currently in scan folder{:else}No matching disc image found{/if}</p>
                  {#if discs.length > 0}
                    <span class="image-path" title={discs[0].image}>{discs[0].image}</span>
                  {/if}
                </div>
                <div class="version-state" class:installed={ready}>
                  <span class="state-dot"></span>
                  {ready ? 'Ready' : 'Not set up'}
                </div>
                <div class="version-actions">
                  <button class="outline-button" disabled={discs.length === 0 || !!setupBusy || movingData} onclick={() => setupVersion(version.id)}>
                    {setupBusy === `${selectedGame}:${version.id}` ? 'Setting up…' : ready ? 'Set up again' : 'Set up'}
                  </button>
                  <button class="primary-button" disabled={!selectedForPlay || !!setupBusy || movingData} title={!playable ? 'Play is disabled in Settings' : !gamePort ? 'Choose a native game port in Settings' : !ready ? 'Set up this version first' : ''} onclick={() => playVersion(version.id)}>
                    Play <span aria-hidden="true">↗</span>
                  </button>
                </div>
              </article>
            {/each}
          </div>
        {/if}
      </section>
    {:else}
      <section class="settings-page">
        <div class="page-title">
          <div class="eyebrow">PREFERENCES</div>
          <h1>Settings</h1>
          <p>Choose where OpenThievius finds discs and stores game data.</p>
        </div>

        <section class="settings-card">
          <div class="settings-card-heading">
            <span class="settings-icon">▧</span>
            <div><h2>Disc images</h2><p>The launcher scans this folder and its subfolders.</p></div>
          </div>
          <div class="setting-value-row">
            <div class="setting-value" title={snapshot?.config.disc_folder ?? ''}>{snapshot?.config.disc_folder ?? 'No folder selected'}</div>
            <button class="outline-button" onclick={chooseDiscFolder}>Choose folder</button>
          </div>
        </section>

        <section class="settings-card">
          <div class="settings-card-heading">
            <span class="settings-icon">⌂</span>
            <div><h2>OpenThievius data directory</h2><p>Configuration, installed boot files, game data, and saves live here.</p></div>
          </div>
          <div class="setting-value-row">
            <div class="setting-value" title={snapshot?.data_root}>{snapshot?.data_root ?? 'Loading…'}</div>
            <button class="outline-button" disabled={movingData} onclick={chooseDataFolder}>{movingData ? 'Moving…' : 'Move data'}</button>
          </div>
          <p class="setting-footnote">Choose an empty destination folder. The launcher copies and verifies everything before it removes the old directory.</p>
        </section>

        <section class="settings-card">
          <div class="settings-card-heading">
            <span class="settings-icon">▶</span>
            <div><h2>Native game ports</h2><p>Set the executable for each available native port. Play remains disabled until a port is configured.</p></div>
          </div>
          <div class="game-settings-list">
            {#each games as game}
              <div class="game-setting-row">
                <div class="game-setting-title"><strong>{game.name}</strong><span>{game.subtitle}</span></div>
                <div class="game-port-path" title={snapshot?.config.game_ports[game.id] ?? ''}>{snapshot?.config.game_ports[game.id] ?? 'Port not configured'}</div>
                <button class="outline-button" disabled={portPickerBusy === game.id} onclick={() => chooseGamePort(game.id)}>{portPickerBusy === game.id ? 'Opening…' : 'Choose port'}</button>
                <label class="toggle-setting">
                  <input type="checkbox" checked={snapshot?.config.playable[game.id] !== false} onchange={(event) => setPlayable(game.id, event.currentTarget.checked)} />
                  <span class="toggle"></span>
                  <span>Playable</span>
                </label>
              </div>
              <div class="game-display-row">
                <span>Display mode</span>
                <select value={snapshot?.config.display_mode[game.id] ?? 'windowed'} onchange={(event) => setDisplayMode(game.id, event.currentTarget.value as DisplayMode)}>
                  <option value="windowed">Windowed</option>
                  <option value="borderless_fullscreen">Borderless fullscreen</option>
                </select>
              </div>
            {/each}
          </div>
        </section>

        <div class="settings-note"><span>i</span><p>Playability is independent of setup. A version can be fully set up while Play stays unavailable until its native port is ready.</p></div>
      </section>
    {/if}
  </main>

  <footer class="app-footer">
    <span>OPEN THIEVIUS</span>
    <span>Built for your own discs</span>
  </footer>
</div>
