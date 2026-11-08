// SPDX-License-Identifier: AGPL-3.0-or-later
mod config;
mod disc;

use gpui::{
    App, Application, AssetSource, Bounds, Context, PathPromptOptions, Render, SharedString, Timer,
    Window, WindowBounds, WindowOptions, div, prelude::*, px, rgb, size, svg,
};
use std::{
    borrow::Cow,
    path::PathBuf,
    process::Command,
    sync::mpsc::{self, TryRecvError},
    time::Duration,
};

const BG: u32 = 0x101713;
const PANEL: u32 = 0x1b2721;
const PANEL_LIGHT: u32 = 0x26372d;
const TEXT: u32 = 0xf2f3e9;
const MUTED: u32 = 0xa8b6a9;
const ACCENT: u32 = 0xc6e36a;

struct LauncherAssets;

impl AssetSource for LauncherAssets {
    fn load(&self, path: &str) -> anyhow::Result<Option<Cow<'static, [u8]>>> {
        let bytes: Option<&'static [u8]> = match path {
            "background-sly1.svg" => Some(include_bytes!("../resources/background-sly1.svg")),
            "background-sly2.svg" => Some(include_bytes!("../resources/background-sly2.svg")),
            "background-sly3.svg" => Some(include_bytes!("../resources/background-sly3.svg")),
            _ => None,
        };
        Ok(bytes.map(Cow::Borrowed))
    }

    fn list(&self, _path: &str) -> anyhow::Result<Vec<SharedString>> {
        Ok(Vec::new())
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Page {
    Games,
    Settings,
}

struct Launcher {
    page: Page,
    game: usize,
    data_root: PathBuf,
    config: config::LauncherConfig,
    definitions: Vec<disc::VersionDefinition>,
    found: Vec<disc::FoundVersion>,
    selected: Option<usize>,
    scan_started: bool,
    scanning: bool,
    scan_summary: String,
    scan_errors: Vec<String>,
    setup_status: String,
    storage_unavailable: bool,
    setup_busy: bool,
    data_move_busy: bool,
    launch_preflight_busy: bool,
    game_running: bool,
}

enum SetupEvent {
    Progress(String),
    Finished(Result<(disc::FoundVersion, PathBuf), std::io::Error>),
}

struct LaunchRequest {
    found: disc::FoundVersion,
    executable: PathBuf,
    port: PathBuf,
    data_root: PathBuf,
    display_mode: &'static str,
}

enum LaunchEvent {
    Finished(Result<LaunchRequest, String>),
}

impl Launcher {
    fn new(
        data_root: PathBuf,
        config: config::LauncherConfig,
        startup_status: Option<String>,
        storage_unavailable: bool,
    ) -> Self {
        Self {
            page: Page::Games,
            game: 0,
            data_root,
            config,
            definitions: serde_json::from_str(env!("SUPPORTED_VERSIONS"))
                .expect("embedded game version definitions"),
            found: Vec::new(),
            selected: None,
            scan_started: false,
            scanning: false,
            scan_summary: String::new(),
            scan_errors: Vec::new(),
            setup_status: startup_status.unwrap_or_default(),
            storage_unavailable,
            setup_busy: false,
            data_move_busy: false,
            launch_preflight_busy: false,
            game_running: false,
        }
    }

    fn installed_version(&self, found: &disc::FoundVersion) -> Option<config::InstalledVersion> {
        let key = format!("{}:{}", found.game_id, found.version_id);
        self.config
            .installed
            .get(&key)
            .filter(|installed| installed.executable.is_file())
            .cloned()
    }

    fn recover_setup_state(&mut self, found: &disc::FoundVersion) {
        let key = format!("{}:{}", found.game_id, found.version_id);
        let Some(definition) = self
            .definitions
            .iter()
            .find(|version| version.game_id == found.game_id && version.id == found.version_id)
        else {
            return;
        };
        if let Some(installed) = self.config.installed.get(&key) {
            if disc::boot_executable_matches(&installed.executable, definition) {
                return;
            }
            self.config.installed.remove(&key);
        }
        let executable = self
            .data_root
            .join(&found.game_id)
            .join(&found.version_id)
            .join("boot.elf");
        if disc::boot_executable_matches(&executable, definition) {
            self.config.installed.insert(
                key,
                config::InstalledVersion {
                    image: found.image.clone(),
                    executable,
                },
            );
            if let Err(error) = config::save(&self.data_root, &self.config) {
                self.setup_status = format!(
                    "{} is set up, but recovered setup state could not be saved: {error}",
                    found.label
                );
            }
        }
    }

    fn start_scan(&mut self, cx: &mut Context<Self>) {
        let Some(folder) = self.config.disc_folder.clone() else {
            self.scan_summary = "Choose a disc image folder in Settings to scan.".to_owned();
            return;
        };
        if self.storage_unavailable
            || self.scanning
            || self.setup_busy
            || self.data_move_busy
            || self.launch_preflight_busy
        {
            return;
        }
        self.scanning = true;
        self.scan_started = true;
        self.found.clear();
        self.selected = None;
        self.scan_errors.clear();
        self.scan_summary = "Scanning disc images…".to_owned();

        let definitions = self.definitions.clone();
        let (sender, receiver) = mpsc::channel();
        std::thread::spawn(move || disc::scan_folder(&folder, &definitions, sender));

        let weak = cx.weak_entity();
        cx.spawn(async move |_, cx| {
            loop {
                match receiver.try_recv() {
                    Ok(disc::ScanEvent::Found(found)) => {
                        let _ = weak.update(cx, |this, cx| {
                            if !this.found.iter().any(|item| {
                                item.game_id == found.game_id
                                    && item.version_id == found.version_id
                                    && item.image == found.image
                            }) {
                                this.recover_setup_state(&found);
                                this.found.push(found);
                                let current_game = ["sly1", "sly2", "sly3"][this.game];
                                let current_selection_matches_page = this
                                    .selected
                                    .and_then(|index| this.found.get(index))
                                    .is_some_and(|selected| selected.game_id == current_game);
                                if !current_selection_matches_page {
                                    let first_visible_game_match = this
                                        .found
                                        .iter()
                                        .position(|item| item.game_id == current_game);
                                    this.selected = first_visible_game_match
                                        .or_else(|| this.selected.or(Some(this.found.len() - 1)));
                                }
                            }
                            cx.notify();
                        });
                    }
                    Ok(disc::ScanEvent::Finished { inspected, errors }) => {
                        let _ = weak.update(cx, |this, cx| {
                            this.scanning = false;
                            this.scan_errors = errors;
                            this.scan_summary = if this.found.is_empty() {
                                if inspected == 0 && !this.scan_errors.is_empty() {
                                    "No disc images could be scanned. Check the selected folder and permissions.".to_owned()
                                } else {
                                    "No valid disc images found.".to_owned()
                                }
                            } else {
                                format!(
                                    "{} supported disc image(s) found among {inspected} image(s).",
                                    this.found.len()
                                )
                            };
                            cx.notify();
                        });
                        break;
                    }
                    Err(TryRecvError::Disconnected) => {
                        let _ = weak.update(cx, |this, cx| {
                            this.scanning = false;
                            this.scan_summary = "Disc scan stopped unexpectedly.".to_owned();
                            cx.notify();
                        });
                        break;
                    }
                    Err(TryRecvError::Empty) => {
                        Timer::after(Duration::from_millis(60)).await;
                    }
                }
            }
        })
        .detach();
    }

    fn choose_disc_folder(&mut self, cx: &mut Context<Self>) {
        if self.storage_unavailable
            || self.scanning
            || self.data_move_busy
            || self.setup_busy
            || self.launch_preflight_busy
        {
            return;
        }
        let result = cx.prompt_for_paths(PathPromptOptions {
            files: false,
            directories: true,
            multiple: false,
            prompt: Some("Choose the folder containing your disc images".into()),
        });
        let weak = cx.weak_entity();
        cx.spawn(async move |_, cx| {
            if let Ok(Ok(Some(paths))) = result.await {
                if let Some(path) = paths.into_iter().next() {
                    let _ = weak.update(cx, |this, cx| {
                        if this.scanning
                            || this.storage_unavailable
                            || this.data_move_busy
                            || this.setup_busy
                            || this.launch_preflight_busy
                        {
                            return;
                        }
                        let previous = this.config.disc_folder.clone();
                        this.config.disc_folder = Some(path);
                        if let Err(error) = config::save(&this.data_root, &this.config) {
                            this.config.disc_folder = previous;
                            this.scan_summary = format!("Could not save settings: {error}");
                            cx.notify();
                            return;
                        }
                        this.start_scan(cx);
                        cx.notify();
                    });
                }
            }
        })
        .detach();
    }

    fn choose_game_port(&mut self, game_id: &'static str, cx: &mut Context<Self>) {
        if self.storage_unavailable
            || self.data_move_busy
            || self.setup_busy
            || self.launch_preflight_busy
        {
            return;
        }
        let prompt = cx.prompt_for_paths(PathPromptOptions {
            files: true,
            directories: false,
            multiple: false,
            prompt: Some(format!("Choose the {game_id} native port executable").into()),
        });
        let weak = cx.weak_entity();
        cx.spawn(async move |_, cx| {
            if let Ok(Ok(Some(paths))) = prompt.await {
                if let Some(path) = paths.into_iter().next() {
                    let _ = weak.update(cx, |this, cx| {
                        if this.storage_unavailable
                            || this.data_move_busy
                            || this.setup_busy
                            || this.launch_preflight_busy
                        {
                            return;
                        }
                        if !is_executable(&path) {
                            this.setup_status =
                                "The selected path is not an executable file.".to_owned();
                            cx.notify();
                            return;
                        }
                        let previous = this.config.game_ports.insert(game_id.to_owned(), path);
                        if let Err(error) = config::save(&this.data_root, &this.config) {
                            if let Some(previous) = previous {
                                this.config.game_ports.insert(game_id.to_owned(), previous);
                            } else {
                                this.config.game_ports.remove(game_id);
                            }
                            this.setup_status = format!("Could not save settings: {error}");
                        } else {
                            this.setup_status = format!("The {game_id} native port was saved.");
                        }
                        cx.notify();
                    });
                }
            }
        })
        .detach();
    }

    fn choose_data_folder(&mut self, cx: &mut Context<Self>) {
        if self.storage_unavailable
            || self.scanning
            || self.setup_busy
            || self.data_move_busy
            || self.launch_preflight_busy
            || self.game_running
        {
            return;
        }
        let result = cx.prompt_for_paths(PathPromptOptions {
            files: false,
            directories: true,
            multiple: false,
            prompt: Some("Choose where OpenThievius stores its game data".into()),
        });
        let weak = cx.weak_entity();
        cx.spawn(async move |_, cx| {
            if let Ok(Ok(Some(paths))) = result.await {
                if let Some(new_root) = paths.into_iter().next() {
                    let old_root = match weak.read_with(cx, |this, _| {
                        if this.scanning
                            || this.storage_unavailable
                            || this.setup_busy
                            || this.data_move_busy
                            || this.launch_preflight_busy
                            || this.game_running
                        {
                            None
                        } else {
                            Some(this.data_root.clone())
                        }
                    }) {
                        Ok(Some(root)) => root,
                        Ok(None) => return,
                        Err(_) => return,
                    };
                    let (sender, receiver) = mpsc::channel();
                    let can_move = weak
                        .update(cx, |this, cx| {
                            if this.scanning
                                || this.storage_unavailable
                                || this.setup_busy
                                || this.data_move_busy
                                || this.launch_preflight_busy
                                || this.game_running
                            {
                                return false;
                            }
                            this.data_move_busy = true;
                            this.setup_status =
                                "Copying and verifying the data folder…".to_owned();
                            cx.notify();
                            true
                        })
                        .unwrap_or(false);
                    if !can_move {
                        return;
                    }
                    std::thread::spawn(move || {
                        let _ = sender.send(config::move_data_root(&old_root, &new_root));
                    });
                    loop {
                        match receiver.try_recv() {
                            Ok(Ok(move_result)) => {
                                let path = move_result.path;
                                let _ = weak.update(cx, |this, cx| {
                                    this.data_move_busy = false;
                                    this.data_root = path.clone();
                                    match config::load(&path) {
                                        Ok(settings) => this.config = settings,
                                        Err(error) => this.setup_status = format!("Data moved, but settings could not be reloaded: {error}"),
                                    }
                                    if let Some(warning) = move_result.cleanup_warning {
                                        this.setup_status = warning;
                                    } else if this.setup_status.starts_with("Copying and verifying") {
                                        this.setup_status = format!("Data folder moved to {}.", path.display());
                                    }
                                    cx.notify();
                                });
                                break;
                            }
                            Ok(Err(error)) => {
                                let _ = weak.update(cx, |this, cx| {
                                    this.data_move_busy = false;
                                    this.setup_status = format!("Could not move data folder: {error}");
                                    cx.notify();
                                });
                                break;
                            }
                            Err(TryRecvError::Disconnected) => {
                                let _ = weak.update(cx, |this, cx| {
                                    this.data_move_busy = false;
                                    this.setup_status = "Data folder move stopped unexpectedly.".to_owned();
                                    cx.notify();
                                });
                                break;
                            }
                            Err(TryRecvError::Empty) => {
                                Timer::after(Duration::from_millis(60)).await;
                            }
                        }
                    }
                }
            }
        })
        .detach();
    }

    fn retry_storage(&mut self, cx: &mut Context<Self>) {
        if !self.storage_unavailable
            || self.scanning
            || self.setup_busy
            || self.data_move_busy
            || self.launch_preflight_busy
            || self.game_running
        {
            return;
        }
        let (data_root, pointer_status) = match config::data_root_or_recover() {
            Ok(result) => result,
            Err(error) => {
                self.setup_status = format!("Storage is still unavailable: {error}");
                cx.notify();
                return;
            }
        };
        let (launcher_config, config_status) = match config::load_or_recover(&data_root) {
            Ok(result) => result,
            Err(error) => {
                self.setup_status = format!(
                    "Could not load launcher settings from {}: {error}",
                    data_root.display()
                );
                cx.notify();
                return;
            }
        };
        self.data_root = data_root;
        self.config = launcher_config;
        self.storage_unavailable = false;
        let mut startup_messages = Vec::new();
        startup_messages.extend(pointer_status);
        startup_messages.extend(config_status);
        self.setup_status = startup_messages.join("\n");
        self.scan_started = false;
        self.found.clear();
        self.selected = None;
        self.scan_errors.clear();
        self.scanning = false;
        self.start_scan(cx);
        cx.notify();
    }

    fn setup_selected(&mut self, cx: &mut Context<Self>) {
        let Some(index) = self.selected else { return };
        let Some(found) = self.found.get(index).cloned() else {
            return;
        };
        let Some(definition) = self
            .definitions
            .iter()
            .find(|definition| {
                definition.game_id == found.game_id && definition.id == found.version_id
            })
            .cloned()
        else {
            self.setup_status =
                "The selected version definition is no longer available.".to_owned();
            cx.notify();
            return;
        };
        let data_root = self.data_root.clone();
        if self.storage_unavailable
            || self.scanning
            || self.setup_busy
            || self.data_move_busy
            || self.launch_preflight_busy
            || self.game_running
        {
            return;
        }
        self.setup_busy = true;
        self.setup_status = format!("Preparing {}…", found.label);
        let (sender, receiver) = mpsc::channel();
        std::thread::spawn(move || {
            let _ = sender.send(SetupEvent::Progress(format!(
                "Reading and verifying {}…",
                found.label
            )));
            let result = (|| {
                let executable = disc::read_supported_boot_executable(&found.image, &definition)?;
                let version_dir = data_root.join(&found.game_id).join(&found.version_id);
                std::fs::create_dir_all(&version_dir)?;
                let executable_path = version_dir.join("boot.elf");
                let _ = sender.send(SetupEvent::Progress(format!(
                    "Writing {} to the game data folder…",
                    found.label
                )));
                config::atomic_write(&executable_path, &executable)?;
                Ok::<_, std::io::Error>((found, executable_path))
            })();
            let _ = sender.send(SetupEvent::Finished(result));
        });
        let weak = cx.weak_entity();
        cx.spawn(async move |_, cx| {
            loop {
                match receiver.try_recv() {
                    Ok(SetupEvent::Progress(message)) => {
                        let _ = weak.update(cx, |this, cx| {
                            this.setup_status = message;
                            cx.notify();
                        });
                    }
                    Ok(SetupEvent::Finished(result)) => {
                        let _ = weak.update(cx, |this, cx| {
                            this.setup_busy = false;
                            match result {
                                Ok((found, executable)) => {
                                    let key = format!("{}:{}", found.game_id, found.version_id);
                                    this.config.installed.insert(
                                        key,
                                        config::InstalledVersion {
                                            image: found.image,
                                            executable,
                                        },
                                    );
                                    this.setup_status = format!("{} is set up.", found.label);
                                    if let Err(error) = config::save(&this.data_root, &this.config) {
                                        this.setup_status = format!("Setup finished, but settings could not be saved: {error}");
                                    }
                                }
                                Err(error) => this.setup_status = format!("Setup failed: {error}"),
                            }
                            cx.notify();
                        });
                        break;
                    }
                    Err(TryRecvError::Disconnected) => {
                        let _ = weak.update(cx, |this, cx| {
                            this.setup_busy = false;
                            this.setup_status = "Setup worker stopped unexpectedly.".to_owned();
                            cx.notify();
                        });
                        break;
                    }
                    Err(TryRecvError::Empty) => {
                        Timer::after(Duration::from_millis(50)).await;
                    }
                }
            }
        })
        .detach();
    }

    fn toggle_playable(&mut self, game_id: &'static str, cx: &mut Context<Self>) {
        if self.storage_unavailable || self.data_move_busy || self.launch_preflight_busy {
            return;
        }
        let playable = self.config.is_playable(game_id);
        let previous = self.config.playable.insert(game_id.to_owned(), !playable);
        if let Err(error) = config::save(&self.data_root, &self.config) {
            if let Some(previous) = previous {
                self.config.playable.insert(game_id.to_owned(), previous);
            } else {
                self.config.playable.remove(game_id);
            }
            self.setup_status = format!("Could not save settings: {error}");
        }
        cx.notify();
    }

    fn toggle_display_mode(&mut self, game_id: &'static str, cx: &mut Context<Self>) {
        if self.storage_unavailable || self.data_move_busy || self.launch_preflight_busy {
            return;
        }
        let mode = self
            .config
            .display_mode
            .get(game_id)
            .copied()
            .unwrap_or_default();
        let mode = match mode {
            config::DisplayMode::Windowed => config::DisplayMode::BorderlessFullscreen,
            config::DisplayMode::BorderlessFullscreen => config::DisplayMode::Windowed,
        };
        let previous = self.config.display_mode.insert(game_id.to_owned(), mode);
        if let Err(error) = config::save(&self.data_root, &self.config) {
            if let Some(previous) = previous {
                self.config
                    .display_mode
                    .insert(game_id.to_owned(), previous);
            } else {
                self.config.display_mode.remove(game_id);
            }
            self.setup_status = format!("Could not save settings: {error}");
        }
        cx.notify();
    }

    fn launch_selected(&mut self, cx: &mut Context<Self>) {
        if self.storage_unavailable
            || self.scanning
            || self.setup_busy
            || self.data_move_busy
            || self.launch_preflight_busy
            || self.game_running
        {
            return;
        }
        let Some(index) = self.selected else { return };
        let Some(found) = self.found.get(index) else {
            return;
        };
        let found = found.clone();
        let Some(installed) = self.installed_version(&found) else {
            self.setup_status = "Set up this version before playing.".to_owned();
            cx.notify();
            return;
        };
        if !found.image.is_file() {
            self.setup_status =
                "The selected disc image is no longer available. Scan the folder again.".to_owned();
            cx.notify();
            return;
        }
        if !self.config.is_playable(&found.game_id) {
            self.setup_status = "Play is disabled for this game in launcher settings.".to_owned();
            cx.notify();
            return;
        }
        let Some(port) = find_game_port(&found.game_id, &self.config) else {
            self.setup_status =
                "The native game port is not installed alongside the launcher.".to_owned();
            cx.notify();
            return;
        };
        let Some(definition) = self
            .definitions
            .iter()
            .find(|definition| {
                definition.game_id == found.game_id && definition.id == found.version_id
            })
            .cloned()
        else {
            self.setup_status =
                "The selected version definition is no longer available.".to_owned();
            cx.notify();
            return;
        };
        let mode = self
            .config
            .display_mode
            .get(&found.game_id)
            .copied()
            .unwrap_or_default();
        let display_mode = match mode {
            config::DisplayMode::Windowed => "windowed",
            config::DisplayMode::BorderlessFullscreen => "borderless",
        };
        let request = LaunchRequest {
            found,
            executable: installed.executable,
            port,
            data_root: self.data_root.clone(),
            display_mode,
        };
        self.launch_preflight_busy = true;
        self.setup_status = "Verifying the selected disc and setup before launch…".to_owned();
        let (sender, receiver) = mpsc::channel();
        std::thread::spawn(move || {
            let result = disc::read_supported_boot_executable(&request.found.image, &definition)
                .map(|_| ())
                .map_err(|error| format!("selected disc could not be verified: {error}"))
                .and_then(|()| {
                    if disc::boot_executable_matches(&request.executable, &definition) {
                        Ok(request)
                    } else {
                        Err("extracted boot executable no longer matches this version; set it up again".to_owned())
                    }
                });
            let _ = sender.send(LaunchEvent::Finished(result));
        });
        let weak = cx.weak_entity();
        cx.spawn(async move |_, cx| {
            loop {
                match receiver.try_recv() {
                    Ok(LaunchEvent::Finished(Ok(request))) => {
                        let _ = weak.update(cx, |this, cx| {
                            this.launch_preflight_busy = false;
                            this.start_game(request, cx);
                        });
                        break;
                    }
                    Ok(LaunchEvent::Finished(Err(error))) => {
                        let _ = weak.update(cx, |this, cx| {
                            this.launch_preflight_busy = false;
                            this.setup_status = format!("Launch check failed: {error}. Scan the folder again or set up this version again.");
                            cx.notify();
                        });
                        break;
                    }
                    Err(TryRecvError::Disconnected) => {
                        let _ = weak.update(cx, |this, cx| {
                            this.launch_preflight_busy = false;
                            this.setup_status = "Launch verification stopped unexpectedly.".to_owned();
                            cx.notify();
                        });
                        break;
                    }
                    Err(TryRecvError::Empty) => {
                        Timer::after(Duration::from_millis(50)).await;
                    }
                }
            }
        })
        .detach();
        cx.notify();
    }

    fn start_game(&mut self, request: LaunchRequest, cx: &mut Context<Self>) {
        let key = format!("{}:{}", request.found.game_id, request.found.version_id);
        let mut source_update_warning = None;
        if self
            .config
            .installed
            .get(&key)
            .is_some_and(|installed| installed.image != request.found.image)
        {
            if let Some(installed) = self.config.installed.get_mut(&key) {
                installed.image = request.found.image.clone();
            }
            if let Err(error) = config::save(&self.data_root, &self.config) {
                source_update_warning = Some(error.to_string());
            }
        }
        match Command::new(&request.port)
            .arg(&request.executable)
            .arg(&request.found.image)
            .env("OPENTHIEVIUS_DISPLAY_MODE", request.display_mode)
            .env("OPENTHIEVIUS_GAME_ID", &request.found.game_id)
            .env(
                "OPENTHIEVIUS_GAME_DATA_DIR",
                request.data_root.join(&request.found.game_id),
            )
            .env(
                "OPENTHIEVIUS_SAVE_DIR",
                request.data_root.join(&request.found.game_id).join("saves"),
            )
            .spawn()
        {
            Ok(mut child) => {
                self.game_running = true;
                self.setup_status = match source_update_warning {
                    Some(error) => format!(
                        "{} is running; the selected disc path could not be saved: {error}",
                        request.found.label
                    ),
                    None => format!("{} is running…", request.found.label),
                };
                let weak = cx.weak_entity();
                cx.spawn(async move |_, cx| {
                    let (sender, receiver) = mpsc::channel();
                    std::thread::spawn(move || {
                        let _ = sender.send(child.wait());
                    });
                    loop {
                        match receiver.try_recv() {
                            Ok(Ok(status)) => {
                                let _ = weak.update(cx, |this, cx| {
                                    this.game_running = false;
                                    this.setup_status = format!("Game exited with {status}.");
                                    cx.notify();
                                });
                                break;
                            }
                            Ok(Err(error)) => {
                                let _ = weak.update(cx, |this, cx| {
                                    this.game_running = false;
                                    this.setup_status =
                                        format!("Could not monitor the game process: {error}");
                                    cx.notify();
                                });
                                break;
                            }
                            Err(TryRecvError::Disconnected) => {
                                let _ = weak.update(cx, |this, cx| {
                                    this.game_running = false;
                                    this.setup_status =
                                        "Game process monitoring stopped unexpectedly.".to_owned();
                                    cx.notify();
                                });
                                break;
                            }
                            Err(TryRecvError::Empty) => {
                                Timer::after(Duration::from_millis(100)).await;
                            }
                        }
                    }
                })
                .detach();
            }
            Err(error) => self.setup_status = format!("Could not launch the native port: {error}"),
        }
        cx.notify();
    }

    fn tab(&self, label: &'static str, page: Page, cx: &mut Context<Self>) -> impl IntoElement {
        let selected = self.page == page;
        div()
            .id(label)
            .px_4()
            .py_3()
            .rounded_md()
            .cursor_pointer()
            .bg(rgb(if selected { PANEL_LIGHT } else { BG }))
            .text_color(rgb(if selected { ACCENT } else { MUTED }))
            .child(label)
            .on_click(cx.listener(move |this, _, _, _| this.page = page))
    }

    fn game_tab(
        &self,
        index: usize,
        label: &'static str,
        cx: &mut Context<Self>,
    ) -> impl IntoElement {
        let selected = self.game == index;
        let game_id = ["sly1", "sly2", "sly3"][index];
        div()
            .id(label)
            .px_4()
            .py_3()
            .cursor_pointer()
            .border_b_2()
            .border_color(rgb(if selected { ACCENT } else { PANEL }))
            .text_color(rgb(if selected { TEXT } else { MUTED }))
            .child(label)
            .on_click(cx.listener(move |this, _, _, cx| {
                if !this.launch_preflight_busy {
                    this.game = index;
                    this.selected = this.found.iter().position(|found| found.game_id == game_id);
                    cx.notify();
                }
            }))
    }
}

impl Render for Launcher {
    fn render(&mut self, _window: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        if !self.scan_started {
            if self.config.disc_folder.is_some() {
                self.start_scan(cx);
            } else {
                self.scan_started = true;
                self.scan_summary = "Choose a disc image folder in Settings to scan.".to_owned();
            }
        }
        let nav = div()
            .flex()
            .items_center()
            .gap_2()
            .px_8()
            .py_4()
            .border_b_1()
            .border_color(rgb(PANEL))
            .child(
                div()
                    .text_xl()
                    .font_weight(gpui::FontWeight::BOLD)
                    .text_color(rgb(TEXT))
                    .child("OPENTHIEVIUS"),
            )
            .child(div().flex_1())
            .child(self.tab("Games", Page::Games, cx))
            .child(self.tab("Settings", Page::Settings, cx));

        let content = if self.page == Page::Games {
            self.games_view(cx).into_any_element()
        } else {
            self.settings_view(cx).into_any_element()
        };

        div()
            .size_full()
            .flex()
            .flex_col()
            .bg(rgb(BG))
            .text_color(rgb(TEXT))
            .child(nav)
            .child(content)
    }
}

impl Launcher {
    fn game_scan_summary(&self, game_id: &str, title: &str) -> String {
        let game_count = self
            .found
            .iter()
            .filter(|found| found.game_id == game_id)
            .count();
        format_game_scan_summary(
            title,
            self.config.disc_folder.is_some(),
            self.scanning,
            game_count,
            self.found.len(),
        )
    }

    fn games_view(&self, cx: &mut Context<Self>) -> impl IntoElement {
        let background = match self.game {
            0 => "background-sly1.svg",
            1 => "background-sly2.svg",
            _ => "background-sly3.svg",
        };
        let selectors = div()
            .flex()
            .gap_5()
            .px_10()
            .pt_6()
            .border_b_1()
            .border_color(rgb(PANEL));
        let selectors = selectors
            .child(self.game_tab(0, "Sly 1", cx))
            .child(self.game_tab(1, "Sly 2", cx))
            .child(self.game_tab(2, "Sly 3", cx));

        let body = match self.game {
            0 => self
                .game_panel("sly1", "Sly Cooper", "The Thievius Raccoonus · 2002", cx)
                .into_any_element(),
            1 => self
                .game_panel("sly2", "Sly 2", "Band of Thieves", cx)
                .into_any_element(),
            _ => self
                .game_panel("sly3", "Sly 3", "Honor Among Thieves", cx)
                .into_any_element(),
        };

        div()
            .relative()
            .flex()
            .flex_col()
            .flex_1()
            .child(
                svg()
                    .path(background)
                    .size_full()
                    .absolute()
                    .top_0()
                    .left_0()
                    .opacity(0.78),
            )
            .child(
                div()
                    .size_full()
                    .absolute()
                    .top_0()
                    .left_0()
                    .bg(gpui::black().opacity(0.32)),
            )
            .child(selectors)
            .child(
                div()
                    .relative()
                    .flex_1()
                    .id("game-results-scroll")
                    .overflow_y_scroll()
                    .flex()
                    .justify_center()
                    .items_start()
                    .p_8()
                    .child(body),
            )
    }

    fn game_panel(
        &self,
        game_id: &'static str,
        title: &'static str,
        subtitle: &'static str,
        cx: &mut Context<Self>,
    ) -> impl IntoElement {
        let mut panel = div()
            .w(px(680.))
            .flex()
            .flex_col()
            .gap_5()
            .rounded_xl()
            .border_1()
            .border_color(rgb(PANEL_LIGHT))
            .bg(rgb(PANEL))
            .p_8()
            .child(
                div()
                    .text_3xl()
                    .font_weight(gpui::FontWeight::BOLD)
                    .child(title),
            )
            .child(div().text_color(rgb(MUTED)).child(subtitle));

        for (index, found) in self.found.iter().enumerate() {
            if found.game_id != game_id {
                continue;
            }
            let selected = self.selected == Some(index);
            let path = found.image.display().to_string();
            panel = panel.child(
                div()
                    .id(("found-version", index))
                    .flex()
                    .items_center()
                    .gap_3()
                    .rounded_lg()
                    .border_1()
                    .border_color(rgb(if selected { ACCENT } else { BG }))
                    .bg(rgb(if selected { PANEL_LIGHT } else { BG }))
                    .p_4()
                    .cursor_pointer()
                    .on_click(cx.listener(move |this, _, _, cx| {
                        if !this.launch_preflight_busy {
                            this.selected = Some(index);
                            cx.notify();
                        }
                    }))
                    .child(div().size_3().rounded_full().bg(rgb(ACCENT)))
                    .child(
                        div()
                            .flex_1()
                            .flex()
                            .flex_col()
                            .child(
                                div()
                                    .font_weight(gpui::FontWeight::MEDIUM)
                                    .child(SharedString::from(found.label.clone())),
                            )
                            .child(div().text_sm().text_color(rgb(MUTED)).child(
                                SharedString::from(format!(
                                    "{} · {}",
                                    found.game_id.to_uppercase(),
                                    found.version_id
                                )),
                            ))
                            .child(
                                div()
                                    .text_xs()
                                    .text_color(rgb(MUTED))
                                    .child(SharedString::from(path)),
                            ),
                    )
                    .child(pill(if self.installed_version(found).is_some() {
                        "Set up"
                    } else {
                        "Found"
                    })),
            );
        }

        panel = panel
            .child(
                div()
                    .text_color(rgb(MUTED))
                    .child(SharedString::from(self.game_scan_summary(game_id, title))),
            )
            .child(
                div().flex().gap_3().child(
                    div()
                        .id("scan-now-games")
                        .child(button(
                            "Scan now",
                            !self.scanning
                                && !self.setup_busy
                                && !self.data_move_busy
                                && !self.launch_preflight_busy
                                && self.config.disc_folder.is_some(),
                        ))
                        .on_click(cx.listener(|this, _, _, cx| this.start_scan(cx))),
                ),
            );
        if let Some(index) = self.selected {
            if let Some(found) = self
                .found
                .get(index)
                .filter(|found| found.game_id == game_id)
            {
                let is_setup = self.installed_version(found).is_some();
                let source_available = found.image.is_file();
                let playable = self.config.is_playable(&found.game_id);
                let port_available = find_game_port(&found.game_id, &self.config).is_some();
                panel = panel.child(
                    div()
                        .flex()
                        .gap_3()
                        .child(
                            div()
                                .id("setup-selected")
                                .child(button(
                                    "Set up",
                                    !self.setup_busy
                                        && !self.data_move_busy
                                        && !self.launch_preflight_busy
                                        && !self.game_running,
                                ))
                                .on_click(cx.listener(|this, _, _, cx| this.setup_selected(cx))),
                        )
                        .child(
                            div()
                                .id("play-selected")
                                .child(button(
                                    "Play",
                                    is_setup
                                        && playable
                                        && source_available
                                        && port_available
                                        && !self.scanning
                                        && !self.setup_busy
                                        && !self.data_move_busy
                                        && !self.launch_preflight_busy
                                        && !self.game_running,
                                ))
                                .on_click(cx.listener(|this, _, _, cx| this.launch_selected(cx))),
                        ),
                );
                let play_status = if !playable {
                    Some("Play is disabled in Settings. Setup remains available.")
                } else if !source_available {
                    Some("The source disc image is unavailable. Choose its folder again and scan.")
                } else if !port_available {
                    Some(
                        "Setup is available. Choose a native port executable in Settings before playing.",
                    )
                } else {
                    None
                };
                if let Some(play_status) = play_status {
                    panel = panel.child(div().text_sm().text_color(rgb(MUTED)).child(play_status));
                }
            }
        }
        if !self.setup_status.is_empty() {
            panel = panel.child(
                div()
                    .text_color(rgb(MUTED))
                    .child(SharedString::from(self.setup_status.clone())),
            );
        }
        if !self
            .definitions
            .iter()
            .any(|version| version.game_id == game_id)
        {
            panel = panel.child(
                div()
                    .text_color(rgb(MUTED))
                    .child("No supported version definitions are available for this game yet."),
            );
        }
        if !self.scan_errors.is_empty() {
            panel = panel.child(
                div()
                    .text_sm()
                    .text_color(rgb(MUTED))
                    .child(SharedString::from(format!(
                        "{} image or folder error(s). Showing up to 3:",
                        self.scan_errors.len()
                    ))),
            );
            for error in self.scan_errors.iter().take(3) {
                panel = panel.child(
                    div()
                        .text_xs()
                        .text_color(rgb(MUTED))
                        .child(SharedString::from(error.clone())),
                );
            }
        }
        panel
    }

    fn settings_view(&self, cx: &mut Context<Self>) -> impl IntoElement {
        let folder_label = self
            .config
            .disc_folder
            .as_ref()
            .map(|path| path.display().to_string())
            .unwrap_or_else(|| "No folder selected".to_owned());
        let data_root_label = self.data_root.display().to_string();
        let mut settings = div()
            .w(px(680.))
            .flex()
            .flex_col()
            .gap_5()
            .rounded_xl()
            .border_1()
            .border_color(rgb(PANEL_LIGHT))
            .bg(rgb(PANEL))
            .p_8()
            .child(
                div()
                    .text_3xl()
                    .font_weight(gpui::FontWeight::BOLD)
                    .child("Settings"),
            )
            .child(div().text_color(rgb(MUTED)).child("Disc image folder"))
            .child(
                div()
                    .flex()
                    .items_center()
                    .gap_3()
                    .rounded_lg()
                    .bg(rgb(BG))
                    .p_4()
                    .child(
                        div()
                            .flex_1()
                            .text_color(rgb(MUTED))
                            .child(SharedString::from(folder_label)),
                    )
                    .child(
                        div()
                            .id("choose-disc-folder")
                            .child(button(
                                "Choose folder",
                                !self.storage_unavailable
                                    && !self.scanning
                                    && !self.data_move_busy
                                    && !self.setup_busy
                                    && !self.launch_preflight_busy,
                            ))
                            .on_click(cx.listener(|this, _, _, cx| this.choose_disc_folder(cx))),
                    ),
            )
            .child(
                div()
                    .id("scan-now-settings")
                    .child(button(
                        "Scan now",
                        !self.scanning
                            && !self.storage_unavailable
                            && !self.setup_busy
                            && !self.data_move_busy
                            && !self.launch_preflight_busy
                            && self.config.disc_folder.is_some(),
                    ))
                    .on_click(cx.listener(|this, _, _, cx| this.start_scan(cx))),
            )
            .child(
                div()
                    .text_color(rgb(MUTED))
                    .child("OpenThievius data folder"),
            )
            .child(
                div()
                    .flex()
                    .items_center()
                    .gap_3()
                    .rounded_lg()
                    .bg(rgb(BG))
                    .p_4()
                    .child(
                        div()
                            .flex_1()
                            .text_color(rgb(MUTED))
                            .child(SharedString::from(data_root_label)),
                    )
                    .child(
                        div()
                            .id("choose-data-folder")
                            .child(button(
                                "Move data folder",
                                !self.storage_unavailable
                                    && !self.scanning
                                    && !self.setup_busy
                                    && !self.data_move_busy
                                    && !self.launch_preflight_busy
                                    && !self.game_running,
                            ))
                            .on_click(cx.listener(|this, _, _, cx| this.choose_data_folder(cx))),
                    ),
            )
            .child(
                div()
                    .text_sm()
                    .text_color(rgb(MUTED))
                    .child(SharedString::from(self.scan_summary.clone())),
            );
        for (game_index, (game_id, title)) in
            [("sly1", "Sly 1"), ("sly2", "Sly 2"), ("sly3", "Sly 3")]
                .into_iter()
                .enumerate()
        {
            let playable = self.config.is_playable(game_id);
            let port_label = find_game_port(game_id, &self.config)
                .map(|path| path.display().to_string())
                .unwrap_or_else(|| "No native port available".to_owned());
            let mode = self
                .config
                .display_mode
                .get(game_id)
                .copied()
                .unwrap_or_default();
            let mode_label = match mode {
                config::DisplayMode::Windowed => "Windowed",
                config::DisplayMode::BorderlessFullscreen => "Borderless fullscreen",
            };
            settings =
                settings.child(
                    div()
                        .flex()
                        .items_center()
                        .gap_3()
                        .child(div().flex_1().child(title))
                        .child(
                            div()
                                .id(("playable-setting", game_index))
                                .child(button(
                                    if playable {
                                        "Play enabled"
                                    } else {
                                        "Play disabled"
                                    },
                                    !self.storage_unavailable
                                        && !self.data_move_busy
                                        && !self.launch_preflight_busy,
                                ))
                                .on_click(cx.listener(move |this, _, _, cx| {
                                    this.toggle_playable(game_id, cx)
                                })),
                        )
                        .child(
                            div()
                                .id(("display-mode-setting", game_index))
                                .child(button(
                                    mode_label,
                                    !self.storage_unavailable
                                        && !self.data_move_busy
                                        && !self.launch_preflight_busy,
                                ))
                                .on_click(cx.listener(move |this, _, _, cx| {
                                    this.toggle_display_mode(game_id, cx)
                                })),
                        ),
                );
            settings = settings.child(
                div()
                    .flex()
                    .items_center()
                    .gap_3()
                    .child(
                        div()
                            .flex_1()
                            .text_sm()
                            .text_color(rgb(MUTED))
                            .child(SharedString::from(port_label)),
                    )
                    .child(
                        div()
                            .id(("choose-native-port", game_index))
                            .child(button(
                                "Choose port executable",
                                !self.data_move_busy
                                    && !self.storage_unavailable
                                    && !self.setup_busy
                                    && !self.launch_preflight_busy,
                            ))
                            .on_click(cx.listener(move |this, _, _, cx| {
                                this.choose_game_port(game_id, cx)
                            })),
                    ),
            );
        }
        if self.storage_unavailable {
            settings = settings.child(
                div()
                    .id("retry-storage")
                    .child(button("Retry storage", true))
                    .on_click(cx.listener(|this, _, _, cx| this.retry_storage(cx))),
            );
        }
        if !self.setup_status.is_empty() {
            settings = settings.child(
                div()
                    .text_color(rgb(MUTED))
                    .child(SharedString::from(self.setup_status.clone())),
            );
        }
        div()
            .flex_1()
            .id("settings-scroll")
            .overflow_y_scroll()
            .flex()
            .justify_center()
            .items_start()
            .p_8()
            .child(settings)
    }
}

fn format_game_scan_summary(
    title: &str,
    has_disc_folder: bool,
    scanning: bool,
    game_count: usize,
    total_count: usize,
) -> String {
    if !has_disc_folder {
        return "Choose a disc image folder in Settings to scan.".to_owned();
    }
    if scanning {
        return if game_count == 0 {
            "Scanning disc images…".to_owned()
        } else {
            format!(
                "Scanning disc images… {} found so far.",
                plural_count(game_count, &format!("supported {title} disc image"))
            )
        };
    }
    if game_count > 0 {
        return format!(
            "Found {}.",
            plural_count(game_count, &format!("supported {title} disc image"))
        );
    }
    if total_count == 0 {
        "No valid disc images found.".to_owned()
    } else {
        format!(
            "No supported {title} disc images found; found {} for other games.",
            plural_count(total_count, "supported image")
        )
    }
}

fn plural_count(count: usize, singular: &str) -> String {
    if count == 1 {
        format!("{count} {singular}")
    } else {
        format!("{count} {singular}s")
    }
}

#[cfg(test)]
mod tests {
    use super::format_game_scan_summary;

    #[test]
    fn game_scan_summary_scopes_results_and_preserves_required_empty_message() {
        assert_eq!(
            format_game_scan_summary("Sly 1", true, false, 0, 0),
            "No valid disc images found."
        );
        assert_eq!(
            format_game_scan_summary("Sly 1", true, false, 1, 1),
            "Found 1 supported Sly 1 disc image."
        );
        assert_eq!(
            format_game_scan_summary("Sly 2", true, false, 0, 1),
            "No supported Sly 2 disc images found; found 1 supported image for other games."
        );
        assert_eq!(
            format_game_scan_summary("Sly 1", true, true, 2, 2),
            "Scanning disc images… 2 supported Sly 1 disc images found so far."
        );
    }
}

fn pill(label: &'static str) -> impl IntoElement {
    div()
        .rounded_full()
        .bg(rgb(PANEL_LIGHT))
        .px_3()
        .py_2()
        .text_sm()
        .text_color(rgb(MUTED))
        .child(label)
}

fn button(label: &'static str, enabled: bool) -> impl IntoElement {
    let button = div()
        .px_5()
        .py_3()
        .rounded_md()
        .bg(rgb(if enabled { ACCENT } else { PANEL_LIGHT }))
        .text_color(rgb(if enabled { BG } else { MUTED }))
        .child(label);
    if enabled {
        button.cursor_pointer()
    } else {
        button
    }
}

fn find_game_port(game_id: &str, config: &config::LauncherConfig) -> Option<PathBuf> {
    if let Some(path) = config
        .game_ports
        .get(game_id)
        .filter(|path| is_executable(path))
    {
        return absolute_executable(path);
    }
    let env_name = match game_id {
        "sly1" => "OPENTHIEVIUS_SLY1_EXECUTABLE",
        "sly2" => "OPENTHIEVIUS_SLY2_EXECUTABLE",
        "sly3" => "OPENTHIEVIUS_SLY3_EXECUTABLE",
        _ => return None,
    };
    if let Some(path) = std::env::var_os(env_name)
        .map(PathBuf::from)
        .filter(|path| is_executable(path))
    {
        return absolute_executable(&path);
    }
    let executable_name = format!("{game_id}_native{}", std::env::consts::EXE_SUFFIX);
    let mut candidates = Vec::new();
    if let Some(appimage) = std::env::var_os("APPIMAGE") {
        let appimage_path = PathBuf::from(appimage);
        if let Some(parent) = appimage_path.parent() {
            candidates.push(parent.join(&executable_name));
        }
    }
    if let Ok(current_exe) = std::env::current_exe() {
        if let Some(parent) = current_exe.parent() {
            candidates.push(parent.join(&executable_name));
            candidates.push(parent.join("../lib/openthievius").join(&executable_name));
        }
    }
    if let Ok(current_dir) = std::env::current_dir() {
        candidates.push(current_dir.join("build/port").join(&executable_name));
    }
    candidates
        .into_iter()
        .find(|path| is_executable(path))
        .and_then(|path| absolute_executable(&path))
}

fn absolute_executable(path: &std::path::Path) -> Option<PathBuf> {
    path.canonicalize()
        .ok()
        .filter(|canonical| is_executable(canonical))
}

#[cfg(unix)]
fn is_executable(path: &std::path::Path) -> bool {
    use std::os::unix::fs::PermissionsExt;
    path.metadata()
        .map(|metadata| metadata.is_file() && metadata.permissions().mode() & 0o111 != 0)
        .unwrap_or(false)
}

#[cfg(windows)]
fn is_executable(path: &std::path::Path) -> bool {
    path.is_file()
        && path
            .extension()
            .and_then(|extension| extension.to_str())
            .is_some_and(|extension| extension.eq_ignore_ascii_case("exe"))
}

#[cfg(not(any(unix, windows)))]
fn is_executable(path: &std::path::Path) -> bool {
    path.is_file()
}

fn main() {
    let suggested_root = config::suggested_data_root().unwrap_or_else(|_| PathBuf::from("."));
    let (data_root, pointer_status, root_error) = match config::data_root_or_recover() {
        Ok((root, status)) => (root, status, None),
        Err(error) => (
            suggested_root.clone(),
            None,
            Some(format!(
                "Could not initialize the OpenThievius data folder. The standard location is {}; the saved data location could not be opened: {error}",
                suggested_root.display()
            )),
        ),
    };
    let (launcher_config, config_status) = if root_error.is_some() {
        (config::LauncherConfig::default(), None)
    } else {
        match config::load_or_recover(&data_root) {
            Ok((config, status)) => (config, status),
            Err(error) => (
                config::LauncherConfig::default(),
                Some(format!(
                    "Could not load launcher settings from {}: {error}",
                    data_root.display()
                )),
            ),
        }
    };
    let storage_unavailable = root_error.is_some() || config_status.is_some();
    let mut startup_messages = Vec::new();
    startup_messages.extend(pointer_status);
    startup_messages.extend(root_error);
    startup_messages.extend(config_status);
    let startup_status = (!startup_messages.is_empty()).then(|| startup_messages.join("\n"));
    Application::new()
        .with_assets(LauncherAssets)
        .run(move |cx: &mut App| {
            let data_root = data_root.clone();
            let launcher_config = launcher_config.clone();
            let startup_status = startup_status.clone();
            let storage_unavailable = storage_unavailable;
            let bounds = Bounds::centered(None, size(px(1100.), px(760.)), cx);
            cx.open_window(
                WindowOptions {
                    window_bounds: Some(WindowBounds::Windowed(bounds)),
                    window_min_size: Some(size(px(780.), px(560.))),
                    app_id: Some("org.openthievius.launcher".to_owned()),
                    ..Default::default()
                },
                move |_, cx| {
                    let data_root = data_root.clone();
                    let launcher_config = launcher_config.clone();
                    let startup_status = startup_status.clone();
                    cx.new(|_| {
                        Launcher::new(
                            data_root,
                            launcher_config,
                            startup_status,
                            storage_unavailable,
                        )
                    })
                },
            )
            .expect("open launcher window");
            cx.activate(true);
        });
}
